// Backup and restore buttons; the work is done in backup.js.
// Mixed into diiiApp (diii.js); `this` is the app.

import { backupDevice, readBackup, restoreFiles } from './backup.js';

export const backupUiMethods = {
    backupApi() {
        return {
            capture: (lua) => this.executeLuaCapture(lua),
            upload: (name, text) => this.sendScriptTextToiii(name, text),
            log: (line) => this.outputLine(line)
        };
    },

    async backupToZip() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }

        try {
            this.outputLine('backup: reading files...');
            const { zip, count } = await backupDevice(this.backupApi());
            const stamp = new Date().toISOString().slice(0, 16).replace('T', '-').replace(':', '');
            const label = String(this.connectedDeviceLabel || 'iii').replace(/[^a-z0-9_-]+/gi, '-');
            const fileName = `${label}-backup-${stamp}.zip`;

            const url = URL.createObjectURL(new Blob([zip], { type: 'application/zip' }));
            const link = document.createElement('a');
            link.href = url;
            link.download = fileName;
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            URL.revokeObjectURL(url);

            this.outputLine(`backup: ${count} files saved to ${fileName}`);
        } catch (error) {
            this.outputLine(`Backup error: ${error.message}`);
        }
    },

    async restoreFromZip() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }

        const file = await this.pickZipFile();
        if (!file) return;

        try {
            const { restore, skipped } = await readBackup(new Uint8Array(await file.arrayBuffer()));
            if (!restore.length) {
                this.outputLine(`restore: nothing to restore in ${file.name}`);
                return;
            }

            const names = restore.map((entry) => entry.name).join(', ');
            if (
                !window.confirm(
                    `Restore ${restore.length} files from ${file.name}?\n\n${names}\n\nFiles with the same names are replaced; other files stay.`
                )
            ) {
                return;
            }

            if (skipped.length) this.outputLine(`restore: skipping ${skipped.join(', ')}`);
            await restoreFiles(this.backupApi(), restore);
            this.outputLine(`restore: ${restore.length} files written`);
            await this.requestFileListRefresh();
        } catch (error) {
            this.outputLine(`Restore error: ${error.message}`);
        }
    },

    pickZipFile() {
        return new Promise((resolve) => {
            const input = document.createElement('input');
            input.type = 'file';
            input.accept = '.zip,application/zip';
            input.addEventListener('change', () => resolve(input.files?.[0] || null));
            input.addEventListener('cancel', () => resolve(null));
            input.click();
        });
    }
};
