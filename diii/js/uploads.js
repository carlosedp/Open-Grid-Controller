// Uploading scripts: file picker, drag and drop, the r shortcut.
// Mixed into diiiApp (diii.js); `this` is the app.

export const uploadMethods = {
    getUploadLines(text) {
        return String(text)
            .replace(/\r\n/g, '\n')
            .replace(/\r/g, '\n')
            .split('\n')
            .map((line) => line.replace(/\s+$/g, ''));
    },

    async openAndSelectRemoteFile(fileName) {
        const normalizedName = String(fileName || '').trim();
        if (!normalizedName) {
            throw new Error('Missing file name for ^^s');
        }

        this.queueSuppressedOutputLine('-- receiving data');
        this.queueSuppressedOutputLine(`-- set filename: ${normalizedName}`);

        await this.iiiDevice.writeLine('^^s');
        await this.delay(100);
        await this.iiiDevice.writeLine(normalizedName);
        await this.delay(100);
        await this.iiiDevice.writeLine('^^f');
        await this.delay(100);
    },

    async sendScriptTextToiii(fileName, text) {
        // Anything sent between ^^s and ^^w would end up inside the file.
        this.releaseAllGridKeys();
        this.uploadInProgress = true;
        try {
            await this.sendScriptLinesToiii(fileName, text);
        } finally {
            this.uploadInProgress = false;
        }
    },

    async sendScriptLinesToiii(fileName, text) {
        const lines = this.getUploadLines(text);

        await this.executeLuaCapture(`fs_remove_file(${this.luaQuote(fileName)})`);
        // Match diii upload protocol:
        // ^^s, <filename>, ^^f, ^^s, <file lines>, ^^w
        await this.openAndSelectRemoteFile(fileName);
        await this.iiiDevice.writeLine('^^s');
        await this.delay(100);

        for (const line of lines) {
            await this.iiiDevice.writeLine(line);
            await this.delay(1);
        }

        await this.delay(100);
        await this.iiiDevice.writeLine('^^w');

        // Sync: wait for the device to finish ^^w processing (compilation +
        // LittleFS flash write/erase) before issuing any further commands.
        // A fixed delay is unreliable because LFS block compaction can take
        // longer than any reasonable constant.
        await this.executeLuaCapture('print(1)');
    },

    async uploadTextAsScript(name, text, options = {}) {
        const { refreshList = true } = options;

        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device (click connect in the header)');
            return;
        }

        try {
            this.outputLine(`Uploading ${name}...`);
            await this.sendScriptTextToiii(name, text);
            if (refreshList) {
                await this.requestFileListRefresh();
            }
        } catch (error) {
            this.outputLine(`Upload error: ${error.message}`);
        }
    },

    supportsFileSystemPicker() {
        return typeof window?.showOpenFilePicker === 'function';
    },

    async chooseLuaFileWithHandle() {
        if (!this.supportsFileSystemPicker()) {
            return null;
        }

        try {
            const handles = await window.showOpenFilePicker({
                multiple: false,
                excludeAcceptAllOption: true,
                types: [{
                    description: 'Lua scripts',
                    accept: {
                        'text/plain': ['.lua'],
                        'application/x-lua': ['.lua']
                    }
                }]
            });

            const handle = handles?.[0];
            if (!handle) return null;

            const file = await handle.getFile();
            return { file, handle };
        } catch (error) {
            if (error?.name === 'AbortError') {
                return null;
            }

            this.outputLine(`File picker error: ${error.message}`);
            return null;
        }
    },

    async openUploadPicker() {
        if (this.supportsFileSystemPicker()) {
            const picked = await this.chooseLuaFileWithHandle();
            if (!picked) return;
            await this.uploadSelectedFile(picked.file, { fileHandle: picked.handle });
            return;
        }

        if (!this.elements.fileInput) return;
        this.elements.fileInput.value = '';
        this.elements.fileInput.click();
    },

    cacheLastUploadedScript({ name, text, fileHandle = null }) {
        if (!name || typeof text !== 'string') return;
        this.lastUploadedScript = {
            name,
            text,
            fileHandle
        };
    },

    async uploadSelectedFile(file, options = {}) {
        if (!file) return false;

        if (!file.name.toLowerCase().endsWith('.lua')) {
            this.outputLine('Error: Only .lua files are supported');
            return false;
        }

        try {
            this.setExplorerCollapsed(false);
            const text = await file.text();
            await this.uploadTextAsScript(file.name, text);
            this.cacheLastUploadedScript({
                name: file.name,
                text,
                fileHandle: options.fileHandle || null
            });
            return true;
        } catch (error) {
            this.outputLine(`Upload error: ${error.message}`);
            return false;
        }
    },

    async getRefreshableLastScript() {
        // no previous upload case
        if (!this.lastUploadedScript) {
            this.outputLine('No previous upload found. Use u to pick a lua file first.');
            return null;
        }

        // previous upload exists
        if (this.lastUploadedScript.fileHandle) {
            try {
                const file = await this.lastUploadedScript.fileHandle.getFile();
                
                return {
                    name: file.name,
                    text: await file.text(),
                    fileHandle: this.lastUploadedScript.fileHandle
                };
            } catch (error) {
                this.outputLine(`Refresh error: ${error.message}`);
                return null;
            }
        }

        // this path is hit if browser doesn't support file system access API
        if (!this.supportsFileSystemPicker()) {
            this.openUploadPicker();
            return null;
        }

        // this path is hit if browser DOES support file system access API
        const picked = await this.chooseLuaFileWithHandle();
        if (!picked) return null;

        return {
            name: picked.file.name,
            text: await picked.file.text(),
            fileHandle: picked.handle
        };
    },

    async refreshUploadAndRunLastScript() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('no iii device connected.');
            return;
        }

        const script = await this.getRefreshableLastScript();
        if (!script) return;

        try {
            this.outputLine(`r: refreshing ${script.name}`);
            this.queueSuppressedOutputLine('-- re-init with no script', 8000);
            this.queueSuppressedOutputLine('-- init: skip script', 8000);
            this.queueSuppressedOutputLine('-- lua lib', 8000);
            await this.iiiDevice.writeLine('^^c');
            await this.delay(200);
            await this.uploadTextAsScript(script.name, script.text, { refreshList: false });
            this.cacheLastUploadedScript({
                name: script.name,
                text: script.text,
                fileHandle: script.fileHandle
            });
            await this.executeLua(`fs_run_file("lib.lua")`);
            await this.executeLua(`fs_run_file(${this.luaQuote(script.name)})`);
            this.requestFileListRefresh().catch((error) => {
                this.outputLine(`File list error: ${error.message}`);
            });
        } catch (error) {
            this.outputLine(`r command error: ${error.message}`);
        }
    },

    async handleFileSelect(event) {
        const file = event.target?.files?.[0];
        if (!file) return;
        await this.uploadSelectedFile(file);
    },

    setupDragAndDrop() {
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach((eventName) => {
            document.body.addEventListener(eventName, (event) => {
                event.preventDefault();
                event.stopPropagation();
            }, false);
        });

        document.body.addEventListener('drop', async (event) => {
            const files = event.dataTransfer?.files;
            if (!files || files.length === 0) return;

            const file = files[0];
            await this.uploadSelectedFile(file);
        });
    }
};
