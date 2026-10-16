// File explorer pane: collapse, width and the resize handle.
// Mixed into diiiApp (diii.js); `this` is the app.

export const layoutMethods = {
    toggleExplorer() {
        this.setExplorerCollapsed(!this.isExplorerCollapsed);
    },

    setExplorerCollapsed(collapsed) {
        this.isExplorerCollapsed = Boolean(collapsed);
        this.elements.fileExplorerPane?.classList.toggle('collapsed', this.isExplorerCollapsed);
        this.elements.explorerResizer?.classList.toggle('hidden', this.isExplorerCollapsed);

        if (this.elements.fileExplorerPane) {
            if (this.isExplorerCollapsed) {
                this.elements.fileExplorerPane.style.width = '';
                this.elements.fileExplorerPane.style.minWidth = '';
                this.elements.fileExplorerPane.style.maxWidth = '';
            } else {
                this.restoreExplorerWidth();
            }
        }

        if (this.elements.explorerResizer) {
            this.elements.explorerResizer.setAttribute('aria-hidden', String(this.isExplorerCollapsed));
            this.elements.explorerResizer.setAttribute('aria-disabled', String(this.isExplorerCollapsed));
            this.elements.explorerResizer.tabIndex = this.isExplorerCollapsed ? -1 : 0;
        }
        if (this.elements.toggleExplorerBtn) {
            this.elements.toggleExplorerBtn.setAttribute('aria-expanded', String(!this.isExplorerCollapsed));
        }
        if (this.elements.refreshExplorerBtn) {
            this.elements.refreshExplorerBtn.hidden = this.isExplorerCollapsed;
            this.elements.refreshExplorerBtn.setAttribute('aria-hidden', String(this.isExplorerCollapsed));
            this.elements.refreshExplorerBtn.tabIndex = this.isExplorerCollapsed ? -1 : 0;
        }
        if (this.elements.explorerChevron) {
            this.elements.explorerChevron.textContent = this.isExplorerCollapsed ? '›' : '‹';
        }

        this.updateExplorerResizerA11y();
    },

    clampExplorerWidth(width) {
        const containerWidth = this.elements.splitContainer?.clientWidth || 0;
        const dynamicMax =
            containerWidth > 0 ? Math.max(this.explorerWidthMin, containerWidth - 320) : this.explorerWidthMax;
        const hardMax = Math.max(this.explorerWidthMin, Math.min(this.explorerWidthMax, dynamicMax));
        return Math.max(this.explorerWidthMin, Math.min(hardMax, Math.round(width)));
    },

    setExplorerWidth(width, { persist = true } = {}) {
        const pane = this.elements.fileExplorerPane;
        if (!pane || !Number.isFinite(Number(width))) return;

        const clamped = this.clampExplorerWidth(Number(width));
        pane.style.width = `${clamped}px`;
        pane.style.minWidth = `${clamped}px`;
        pane.style.maxWidth = `${clamped}px`;
        this.updateExplorerResizerA11y(clamped);

        if (persist) {
            try {
                window.localStorage.setItem(this.explorerWidthStorageKey, String(clamped));
            } catch {
                // ignore localStorage failures
            }
        }
    },

    restoreExplorerWidth() {
        let restored = this.explorerWidthDefault;

        try {
            const raw = window.localStorage.getItem(this.explorerWidthStorageKey);
            if (raw != null) {
                const parsed = Number.parseInt(raw, 10);
                if (Number.isFinite(parsed)) {
                    restored = parsed;
                }
            }
        } catch {
            // ignore localStorage failures
        }

        this.setExplorerWidth(restored, { persist: false });
    },

    getExplorerWidth() {
        return this.elements.fileExplorerPane?.getBoundingClientRect?.().width || this.explorerWidthDefault;
    },

    updateExplorerResizerA11y(width = this.getExplorerWidth()) {
        const resizer = this.elements.explorerResizer;
        if (!resizer) return;

        const maxWidth = this.clampExplorerWidth(Number.MAX_SAFE_INTEGER);
        const currentWidth = this.clampExplorerWidth(width);
        resizer.setAttribute('aria-valuemin', String(this.explorerWidthMin));
        resizer.setAttribute('aria-valuemax', String(maxWidth));
        resizer.setAttribute('aria-valuenow', String(currentWidth));
        resizer.setAttribute('aria-valuetext', `${currentWidth} pixels`);
    },

    handleExplorerResizerKeydown(event) {
        if (this.isExplorerCollapsed) return;

        const key = event.key;
        const isArrow = key === 'ArrowLeft' || key === 'ArrowRight';
        const isBoundary = key === 'Home' || key === 'End';
        if (!isArrow && !isBoundary) return;

        event.preventDefault();

        const currentWidth = this.getExplorerWidth();
        const step = event.shiftKey ? 48 : 16;

        if (key === 'Home') {
            this.setExplorerWidth(this.explorerWidthMin);
            return;
        }

        if (key === 'End') {
            this.setExplorerWidth(this.clampExplorerWidth(Number.MAX_SAFE_INTEGER));
            return;
        }

        const direction = key === 'ArrowRight' ? 1 : -1;
        this.setExplorerWidth(currentWidth + direction * step);
    },

    startExplorerResize(event) {
        if (this.isExplorerCollapsed) return;
        if (!this.elements.fileExplorerPane || !this.elements.explorerResizer) return;

        event.preventDefault();

        this.isResizingExplorer = true;
        this.explorerResizePointerId = event.pointerId;
        this.explorerResizeStartX = event.clientX;
        this.explorerResizeStartWidth = this.elements.fileExplorerPane.getBoundingClientRect().width;

        this.elements.explorerResizer.classList.add('dragging');
        this.elements.explorerResizer.setPointerCapture(event.pointerId);
        document.body.classList.add('is-resizing-explorer');

        this.boundExplorerResizeMove = this.boundExplorerResizeMove || ((e) => this.handleExplorerResizeMove(e));
        this.boundExplorerResizeEnd = this.boundExplorerResizeEnd || ((e) => this.endExplorerResize(e));

        window.addEventListener('pointermove', this.boundExplorerResizeMove);
        window.addEventListener('pointerup', this.boundExplorerResizeEnd);
        window.addEventListener('pointercancel', this.boundExplorerResizeEnd);
    },

    handleExplorerResizeMove(event) {
        if (!this.isResizingExplorer) return;
        if (this.explorerResizePointerId != null && event.pointerId !== this.explorerResizePointerId) return;

        const delta = event.clientX - this.explorerResizeStartX;
        this.setExplorerWidth(this.explorerResizeStartWidth + delta);
    },

    endExplorerResize(event) {
        if (!this.isResizingExplorer) return;
        if (event && this.explorerResizePointerId != null && event.pointerId !== this.explorerResizePointerId) return;

        this.isResizingExplorer = false;
        this.explorerResizePointerId = null;

        this.elements.explorerResizer?.classList.remove('dragging');
        document.body.classList.remove('is-resizing-explorer');

        if (event && this.elements.explorerResizer?.hasPointerCapture?.(event.pointerId)) {
            this.elements.explorerResizer.releasePointerCapture(event.pointerId);
        }

        if (this.boundExplorerResizeMove) {
            window.removeEventListener('pointermove', this.boundExplorerResizeMove);
        }
        if (this.boundExplorerResizeEnd) {
            window.removeEventListener('pointerup', this.boundExplorerResizeEnd);
            window.removeEventListener('pointercancel', this.boundExplorerResizeEnd);
        }
    },

    // "device ▾" in the header: reboot, bootloader, backup, restore, reformat
    setDeviceMenuOpen(open) {
        this.elements.deviceMenu?.classList.toggle('open', open);
        this.elements.deviceMenuBtn?.setAttribute('aria-expanded', String(open));
    },

    handleDeviceMenuDocumentEvent(event) {
        if (!this.elements.deviceMenu?.classList.contains('open')) return;
        if (event.type === 'keydown') {
            if (event.key === 'Escape') this.setDeviceMenuOpen(false);
            return;
        }
        // an item was chosen, or the click was elsewhere
        if (!event.target?.closest?.('.device-menu-wrap') || event.target.closest('.file-menu-item')) {
            this.setDeviceMenuOpen(false);
        }
    }
};
