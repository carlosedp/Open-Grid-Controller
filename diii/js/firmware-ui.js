// Firmware version in the status pill, and the update helper: compares the
// device with the latest GitHub release and walks through flashing it.
// Mixed into diiiApp (diii.js); `this` is the app.

import { RELEASES_PAGE, fetchLatestRelease, isNewer, parseVersion } from './firmware.js';

export const firmwareUiMethods = {
    // After connecting: version (and board, on Open Grid firmware) into the
    // status pill, then one update check per session.
    async detectFirmware() {
        try {
            // one print per value: iii's print() puts each argument on its own line
            const lines = await this.executeLuaCapture(
                'print(device_version())\nif og_board then print(og_board()) end'
            );
            const [version = '', board = ''] = lines.map((s) => String(s).trim());
            this.firmwareVersion = version || null;
            this.firmwareBoard = board || null;
        } catch {
            this.firmwareVersion = null;
            this.firmwareBoard = null;
        }
        this.showFirmwareInStatus();
        if (!this.firmwareChecked && parseVersion(this.firmwareVersion)) {
            this.firmwareChecked = true;
            this.checkForFirmwareUpdate({ quiet: true });
        }
    },

    showFirmwareInStatus() {
        const text = this.elements.replStatusText;
        const pill = this.elements.replStatusPill;
        if (!text || !this.iiiDevice.isConnected) return;
        const label = this.connectedDeviceLabel || 'connected';
        text.textContent = this.firmwareVersion ? `${label} · ${this.firmwareVersion}` : label;
        if (pill) pill.title = this.firmwareBoard ? `${this.firmwareVersion} for ${this.firmwareBoard}` : '';
    },

    async checkForFirmwareUpdate({ quiet = false } = {}) {
        try {
            this.latestRelease = await fetchLatestRelease();
            this.firmwareCheckError = null;
        } catch (error) {
            this.latestRelease = undefined;
            this.firmwareCheckError = error.message;
        }
        const update = this.latestRelease && isNewer(this.latestRelease.version, this.firmwareVersion);
        this.elements.deviceMenuBtn?.classList.toggle('has-update', Boolean(update));
        this.elements.firmwareBtn?.classList.toggle('has-update', Boolean(update));
        if (update && quiet) {
            this.outputLine(
                `firmware ${this.latestRelease.version} is available (this device has ${this.firmwareVersion}): device ▾ → firmware`
            );
        }
        if (!quiet) this.renderFirmwareDialog();
    },

    openFirmwareDialog() {
        if (!this.elements.firmwareModal) return;
        this.elements.firmwareModal.style.display = 'flex';
        this.renderFirmwareDialog();
        if (this.latestRelease === undefined && !this.firmwareCheckError) this.checkForFirmwareUpdate();
    },

    closeFirmwareDialog() {
        if (this.elements.firmwareModal) this.elements.firmwareModal.style.display = 'none';
    },

    renderFirmwareDialog() {
        const body = this.elements.firmwareBody;
        if (!body) return;
        const el = (tag, text, className) => {
            const node = document.createElement(tag);
            if (text) node.textContent = text;
            if (className) node.className = className;
            return node;
        };
        const button = (label, onClick, title = '') => {
            const b = el('button', label, 'repl-action-btn');
            b.type = 'button';
            b.title = title;
            b.addEventListener('click', onClick);
            return b;
        };
        const link = (label, href) => {
            const a = el('a', label, 'repl-connection-btn');
            a.href = href;
            a.target = '_blank';
            a.rel = 'noopener noreferrer';
            return a;
        };

        const connected = this.iiiDevice.isConnected;
        const release = this.latestRelease;
        const current = this.firmwareVersion;
        const board = this.firmwareBoard;
        const rows = [];
        rows.push(
            el('p', connected ? `this device: ${current || 'unknown'}${board ? ` for ${board}` : ''}` : 'not connected')
        );

        if (release === undefined && !this.firmwareCheckError) rows.push(el('p', 'checking for updates...'));
        else if (this.firmwareCheckError)
            rows.push(el('p', `couldn't check for updates (${this.firmwareCheckError}); offline?`));
        else if (release === null) rows.push(el('p', 'no firmware release published yet.'));
        else if (connected && current && !parseVersion(current)) {
            rows.push(
                el('p', `latest Open Grid firmware: ${release.version}. This device runs other firmware (${current}).`)
            );
        } else if (connected && !isNewer(release.version, current)) {
            rows.push(el('p', `up to date (latest release: ${release.version}).`));
        } else {
            rows.push(el('p', `update available: ${release.version}`, 'firmware-update'));
        }

        const actions = el('div', '', 'firmware-actions');
        const file = release && board ? release.uf2[board] : null;
        if (file) actions.append(link(`download ${release.version} for ${board}`, file));
        else if (release) {
            for (const [name, url] of Object.entries(release.uf2)) actions.append(link(`download for ${name}`, url));
        }
        if (release) actions.append(link('release notes ↗', release.page));
        else actions.append(link('releases ↗', RELEASES_PAGE));
        actions.append(
            button(
                'check again',
                () => (
                    (this.latestRelease = undefined),
                    (this.firmwareCheckError = null),
                    this.renderFirmwareDialog(),
                    this.checkForFirmwareUpdate()
                )
            )
        );
        rows.push(actions);

        const steps = el('ol', '', 'firmware-steps');
        const step = (...children) => {
            const li = el('li');
            li.append(...children);
            steps.append(li);
        };
        step(
            el('span', 'Back up your files first. '),
            button('backup', () => this.backupToZip(), 'download every file as one zip'),
            el(
                'span',
                ' A firmware that is a lot bigger can move where files are kept, and they would seem gone (restore them from the zip).'
            )
        );
        step(el('span', 'Download the .uf2 for your board (above).'));
        step(
            el('span', 'Put the device in bootloader mode: '),
            button('enter bootloader', () => this.bootloaderDevice(), 'restarts the device as a USB drive'),
            el('span', ' It disconnects and shows up as a USB drive called RPI-RP2.')
        );
        step(
            el(
                'span',
                'Drag the .uf2 onto RPI-RP2. The device installs it, restarts, and diii reconnects (or click connect).'
            )
        );
        rows.push(el('h3', 'how to update', 'reference-group'), steps);

        body.replaceChildren(...rows);
    }
};
