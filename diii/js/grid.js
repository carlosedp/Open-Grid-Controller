// Virtual grid: shows the LEDs of devices that can mirror them
// (og_mirror, Open Grid firmware) and sends presses with og_key.
// Mixed into diiiApp (diii.js); `this` is the app.

export const gridMethods = {
    async detectVirtualGrid() {
        let size = null;
        try {
            const lines = await this.executeLuaCapture(
                'print(og_mirror ~= nil and grid_size_x ~= nil and (grid_size_x() .. "x" .. grid_size_y()) or "none")'
            );
            const match = lines
                .map((line) => String(line).trim())
                .join('')
                .match(/^(\d+)x(\d+)$/);
            if (match) size = { x: Number(match[1]), y: Number(match[2]) };
        } catch {
            // device busy or not answering; treat as unsupported
        }

        this.gridSupported = Boolean(size);
        // the drawer only exists for devices that can mirror their LEDs
        if (this.elements.gridDrawer) this.elements.gridDrawer.hidden = !this.gridSupported;
        if (!this.gridSupported) return;

        if (!this.gridCells.length || size.x !== this.gridSize.x || size.y !== this.gridSize.y) {
            this.gridSize = size;
            this.buildGridCells();
        }
        this.elements.gridCells?.classList.remove('offline');
        this.updateGridStatus();
        if (this.gridPanelOpen) await this.setGridMirror(true);
    },

    // The drawer remembers whether it was open; the device decides whether it shows.
    restoreGridDrawer() {
        let open = false;
        try {
            open = window.localStorage.getItem('webdiii.gridDrawer') === 'open';
        } catch {
            // storage blocked
        }
        this.gridPanelOpen = open;
        this.showGridDrawerState();
    },

    showGridDrawerState() {
        const open = this.gridPanelOpen;
        this.elements.gridDrawer?.classList.toggle('open', open);
        if (this.elements.gridToggleBtn) {
            this.elements.gridToggleBtn.textContent = open ? 'grid ▾' : 'grid ▸';
            this.elements.gridToggleBtn.setAttribute('aria-expanded', String(open));
        }
        this.updateGridStatus();
    },

    updateGridStatus() {
        if (!this.elements.gridStatus) return;
        const size = `${this.gridSize.x}×${this.gridSize.y}`;
        const live = this.gridPanelOpen && this.iiiDevice.isConnected;
        this.elements.gridStatus.textContent = this.gridSupported ? (live ? `${size} · live` : size) : '';
    },

    buildGridCells() {
        const container = this.elements.gridCells;
        if (!container) return;

        container.replaceChildren();
        container.style.gridTemplateColumns = `repeat(${this.gridSize.x}, minmax(0, 1fr))`;
        container.style.gridTemplateRows = `repeat(${this.gridSize.y}, minmax(0, 1fr))`;
        // keeps keys square while the grid follows the drawer's size (style.css)
        container.style.setProperty('--grid-ratio', String(this.gridSize.x / this.gridSize.y));
        this.gridCells = [];
        this.gridLevels = [];

        for (let y = 1; y <= this.gridSize.y; y++) {
            for (let x = 1; x <= this.gridSize.x; x++) {
                const cell = document.createElement('div');
                cell.className = 'grid-cell';
                cell.dataset.x = String(x);
                cell.dataset.y = String(y);
                cell.setAttribute('role', 'gridcell');
                container.appendChild(cell);
                this.gridCells.push(cell);
                this.gridLevels.push(0);
            }
        }
    },

    async toggleGridPanel(open = !this.gridPanelOpen) {
        this.gridPanelOpen = open;
        try {
            window.localStorage.setItem('webdiii.gridDrawer', open ? 'open' : 'closed');
        } catch {
            // storage blocked
        }
        this.showGridDrawerState();
        if (!open) this.releaseAllGridKeys();
        if (this.iiiDevice.isConnected && this.gridSupported) {
            await this.setGridMirror(open);
        }
    },

    async setGridMirror(on) {
        try {
            await this.executeLua(`og_mirror(${on ? 'true' : 'false'})`);
        } catch {
            // disconnected; detectVirtualGrid() re-enables on reconnect
        }
    },

    // "^^grid(<one hex digit per LED, row-major>)"
    handleGridFrame(line) {
        const match = line.match(/^\^\^grid\(([0-9a-f]+)\)/i);
        if (!match || !this.gridCells.length) return;

        const levels = match[1];
        const count = Math.min(levels.length, this.gridCells.length);
        for (let i = 0; i < count; i++) {
            const level = parseInt(levels[i], 16);
            if (level === this.gridLevels[i]) continue;
            this.gridLevels[i] = level;
            this.gridCells[i].style.backgroundColor = level
                ? `rgba(255, 190, 100, ${(0.12 + (0.88 * level) / 15).toFixed(3)})`
                : '';
        }
    },

    handleGridPointerDown(event) {
        const cell = event.target.closest?.('.grid-cell');
        if (!cell || (event.button !== 0 && event.button !== 2)) return;
        event.preventDefault();
        // A mouse holds one key at a time; scripts like meadowphysics want a
        // key held while others are pressed. Shift-click or right-click
        // latches a key down, and any click on it lets it go.
        if (event.shiftKey || event.button === 2 || this.gridHeld.has(cell)) {
            this.toggleGridHold(cell);
            return;
        }
        this.gridPointers.set(event.pointerId, cell);
        cell.classList.add('pressed');
        this.sendGridKey(cell, 1);
    },

    handleGridPointerUp(event) {
        const cell = this.gridPointers.get(event.pointerId);
        if (!cell) return;
        this.gridPointers.delete(event.pointerId);
        // another finger may still hold the same key, or it was latched meanwhile
        if (!this.isGridKeyDown(cell)) {
            cell.classList.remove('pressed');
            this.sendGridKey(cell, 0);
        }
    },

    isGridKeyDown(cell) {
        return this.gridHeld.has(cell) || [...this.gridPointers.values()].includes(cell);
    },

    toggleGridHold(cell) {
        if (this.gridHeld.delete(cell)) {
            cell.classList.remove('held');
            if (!this.isGridKeyDown(cell)) {
                cell.classList.remove('pressed');
                this.sendGridKey(cell, 0);
            }
        } else {
            const wasDown = this.isGridKeyDown(cell);
            this.gridHeld.add(cell);
            cell.classList.add('held', 'pressed');
            if (!wasDown) this.sendGridKey(cell, 1);
        }
        this.updateGridHeld();
    },

    updateGridHeld() {
        const button = this.elements.gridReleaseBtn;
        if (!button) return;
        const count = this.gridHeld.size;
        button.hidden = count === 0;
        button.textContent = count > 1 ? `release ${count}` : 'release';
    },

    releaseAllGridKeys({ send = true } = {}) {
        const cells = new Set([...this.gridPointers.values(), ...this.gridHeld]);
        this.gridPointers.clear();
        this.gridHeld.clear();
        for (const cell of cells) {
            cell.classList.remove('pressed', 'held');
            if (send) this.sendGridKey(cell, 0);
        }
        this.updateGridHeld();
    },

    sendGridKey(cell, z) {
        if (!this.iiiDevice.isConnected || this.uploadInProgress) return;
        this.executeLua(`og_key(${cell.dataset.x},${cell.dataset.y},${z})`).catch(() => {});
    }
};
