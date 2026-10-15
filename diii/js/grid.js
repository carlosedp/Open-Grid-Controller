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
            const match = lines.map((line) => String(line).trim()).join('').match(/^(\d+)x(\d+)$/);
            if (match) size = { x: Number(match[1]), y: Number(match[2]) };
        } catch {
            // device busy or not answering; treat as unsupported
        }

        this.gridSupported = Boolean(size);
        if (this.elements.gridBtn) this.elements.gridBtn.hidden = !this.gridSupported;

        if (!this.gridSupported) {
            if (this.gridPanelOpen) this.toggleGridPanel(false);
            return;
        }

        if (!this.gridCells.length || size.x !== this.gridSize.x || size.y !== this.gridSize.y) {
            this.gridSize = size;
            this.buildGridCells();
        }
        this.elements.gridCells?.classList.remove('offline');
        if (this.gridPanelOpen) await this.setGridMirror(true);
    },

    buildGridCells() {
        const container = this.elements.gridCells;
        if (!container) return;

        container.replaceChildren();
        container.style.gridTemplateColumns = `repeat(${this.gridSize.x}, minmax(0, 1fr))`;
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
        if (this.elements.gridPanel) this.elements.gridPanel.hidden = !open;
        this.elements.gridBtn?.classList.toggle('active', open);
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
                ? `rgba(255, 190, 100, ${(0.12 + 0.88 * level / 15).toFixed(3)})`
                : '';
        }
    },

    handleGridPointerDown(event) {
        const cell = event.target.closest?.('.grid-cell');
        if (!cell) return;
        event.preventDefault();
        this.gridPointers.set(event.pointerId, cell);
        cell.classList.add('pressed');
        this.sendGridKey(cell, 1);
    },

    handleGridPointerUp(event) {
        const cell = this.gridPointers.get(event.pointerId);
        if (!cell) return;
        this.gridPointers.delete(event.pointerId);
        // another finger may still hold the same key
        if (![...this.gridPointers.values()].includes(cell)) {
            cell.classList.remove('pressed');
            this.sendGridKey(cell, 0);
        }
    },

    releaseAllGridKeys({ send = true } = {}) {
        const cells = new Set(this.gridPointers.values());
        this.gridPointers.clear();
        for (const cell of cells) {
            cell.classList.remove('pressed');
            if (send) this.sendGridKey(cell, 0);
        }
    },

    sendGridKey(cell, z) {
        if (!this.iiiDevice.isConnected || this.uploadInProgress) return;
        this.executeLua(`og_key(${cell.dataset.x},${cell.dataset.y},${z})`).catch(() => {});
    }
};
