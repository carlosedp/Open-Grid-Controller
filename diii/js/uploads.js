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

    async sendScriptTextToiii(fileName, text, options = {}) {
        // Anything sent between ^^s and ^^w would end up inside the file.
        this.releaseAllGridKeys();
        this.uploadInProgress = true;
        try {
            await this.sendScriptLinesToiii(fileName, text, options);
        } finally {
            this.uploadInProgress = false;
        }
    },

    // Uploads a script and reports whether the device wrote it ("-- file
    // written: <name>"; a compile error doesn't print that).
    async uploadChecked(fileName, text, options = {}) {
        let written = false;
        const observe = (line) => {
            if (line === `-- file written: ${fileName}`) written = true;
        };
        this.outputObservers = this.outputObservers || new Set();
        this.outputObservers.add(observe);
        try {
            await this.sendScriptTextToiii(fileName, text, options);
        } catch (error) {
            this.outputLine(`Upload error: ${error.message}`);
        } finally {
            this.outputObservers.delete(observe);
        }
        return written;
    },

    // keepOld: the firmware replaces the file only once the new one compiles
    // (Open Grid), so don't delete it first; stock iii writes without
    // truncating, so a shorter file would keep the old one's tail.
    // onProgress(fraction) follows the lines sent.
    async sendScriptLinesToiii(fileName, text, { keepOld = false, onProgress = null } = {}) {
        const lines = this.getUploadLines(text);

        if (!keepOld) await this.executeLuaCapture(`fs_remove_file(${this.luaQuote(fileName)})`);
        // Match diii upload protocol:
        // ^^s, <filename>, ^^f, ^^s, <file lines>, ^^w
        await this.openAndSelectRemoteFile(fileName);
        await this.iiiDevice.writeLine('^^s');
        await this.delay(100);

        for (const [i, line] of lines.entries()) {
            await this.iiiDevice.writeLine(line);
            await this.delay(1);
            onProgress?.((i + 1) / lines.length);
        }

        await this.delay(100);
        await this.iiiDevice.writeLine('^^w');

        // Sync: wait for the device to finish ^^w processing (compilation +
        // LittleFS flash write/erase) before issuing any further commands.
        // A fixed delay is unreliable because LFS block compaction can take
        // longer than any reasonable constant.
        await this.executeLuaCapture('print(1)');
    },

    // Returns whether the device wrote the file.
    async uploadTextAsScript(name, text, options = {}) {
        const { refreshList = true, onProgress = null } = options;

        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device (click connect in the header)');
            return false;
        }

        this.outputLine(`Uploading ${name}...`);
        const written = await this.uploadChecked(name, text, { onProgress });
        if (refreshList) {
            await this.requestFileListRefresh().catch((error) => {
                this.outputLine(`File list error: ${error.message}`);
            });
        }
        return written;
    },

    supportsFileSystemPicker() {
        return typeof window?.showOpenFilePicker === 'function';
    },

    async chooseLuaFileWithHandle() {
        const picked = await this.chooseLuaFilesWithHandles({ multiple: false });
        return picked[0] || null;
    },

    async chooseLuaFilesWithHandles({ multiple = true } = {}) {
        if (!this.supportsFileSystemPicker()) {
            return [];
        }

        try {
            const handles = await window.showOpenFilePicker({
                multiple,
                excludeAcceptAllOption: true,
                types: [
                    {
                        description: 'Lua scripts',
                        accept: {
                            'text/plain': ['.lua'],
                            'application/x-lua': ['.lua']
                        }
                    }
                ]
            });

            return Promise.all((handles || []).map(async (handle) => ({ file: await handle.getFile(), handle })));
        } catch (error) {
            if (error?.name === 'AbortError') {
                return [];
            }

            this.outputLine(`File picker error: ${error.message}`);
            return [];
        }
    },

    async openUploadPicker() {
        if (this.supportsFileSystemPicker()) {
            await this.uploadFiles(await this.chooseLuaFilesWithHandles());
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
            const written = await this.uploadTextAsScript(file.name, text, {
                refreshList: options.refreshList !== false,
                onProgress: options.onProgress
            });
            this.cacheLastUploadedScript({
                name: file.name,
                text,
                fileHandle: options.fileHandle || null
            });
            return written;
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
        await this.uploadFiles([...(event.target?.files || [])].map((file) => ({ file })));
    },

    // Uploads several scripts one after the other, then refreshes the list once.
    // picked: [{ file, handle? }] (handle: the r shortcut can re-read the file)
    async uploadFiles(picked) {
        const lua = picked.filter(({ file }) => file.name.toLowerCase().endsWith('.lua'));
        const skipped = picked.filter((entry) => !lua.includes(entry)).map(({ file }) => file.name);
        if (skipped.length) this.outputLine(`skipped (only .lua files): ${skipped.join(', ')}`);
        if (!lua.length) {
            if (skipped.length) this.flashUploadBox('nothing uploaded', 'only .lua files can be uploaded', true);
            return;
        }
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device (click connect in the header)');
            this.flashUploadBox('not connected', 'connect a device first, then drop the files again', true);
            return;
        }

        let uploaded = 0;
        for (const [i, { file, handle }] of lua.entries()) {
            const count = lua.length > 1 ? ` (${i + 1} of ${lua.length})` : '';
            this.showUploadBox({ title: `uploading${count}`, detail: file.name, progress: i / lua.length });
            const onProgress = (fraction) => this.setUploadProgress((i + fraction) / lua.length);
            if (await this.uploadSelectedFile(file, { fileHandle: handle || null, refreshList: false, onProgress })) {
                uploaded++;
            }
        }
        if (lua.length > 1) this.outputLine(`uploaded ${uploaded} of ${lua.length} files`);
        const failed = lua.length - uploaded;
        if (failed) {
            this.flashUploadBox(`${failed} of ${lua.length} not uploaded`, 'see the console for the error', true);
        } else {
            this.flashUploadBox(
                lua.length > 1 ? `uploaded ${uploaded} files` : 'uploaded',
                lua.length > 1 ? '' : lua[0].file.name
            );
        }
        await this.requestFileListRefresh();
    },

    // The box over the page while files are dragged in and uploaded.
    showUploadBox({ title, detail = '', progress = null, dragging = false, failed = false }) {
        const { uploadModal, uploadTitle, uploadDetail } = this.elements;
        if (!uploadModal) return;
        clearTimeout(this.uploadBoxTimer);
        uploadTitle.textContent = title;
        uploadDetail.textContent = detail;
        uploadModal.classList.toggle('dragging', dragging);
        uploadModal.classList.toggle('failed', failed);
        if (progress !== null) this.setUploadProgress(progress);
        uploadModal.style.display = 'flex';
    },

    setUploadProgress(fraction) {
        const bar = this.elements.uploadProgressBar;
        if (bar) bar.style.width = `${Math.round(Math.min(1, Math.max(0, fraction)) * 100)}%`;
    },

    hideUploadBox() {
        clearTimeout(this.uploadBoxTimer);
        if (this.elements.uploadModal) this.elements.uploadModal.style.display = 'none';
    },

    // A result that goes away on its own.
    flashUploadBox(title, detail, failed = false) {
        this.showUploadBox({ title, detail, progress: 1, failed });
        this.uploadBoxTimer = setTimeout(() => this.hideUploadBox(), failed ? 3000 : 1200);
    },

    setupDragAndDrop() {
        const hasFiles = (event) => [...(event.dataTransfer?.types || [])].includes('Files');

        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach((eventName) => {
            document.body.addEventListener(
                eventName,
                (event) => {
                    event.preventDefault();
                    event.stopPropagation();
                },
                false
            );
        });

        document.body.addEventListener('dragenter', (event) => {
            if (!hasFiles(event) || this.uploadInProgress) return;
            this.showUploadBox({
                title: 'drop to upload',
                detail: this.iiiDevice.isConnected ? '.lua files go to the device' : 'no device connected',
                dragging: true
            });
        });

        // relatedTarget is null only when the pointer leaves the window
        document.body.addEventListener('dragleave', (event) => {
            if (!event.relatedTarget && this.elements.uploadModal?.classList.contains('dragging')) {
                this.hideUploadBox();
            }
        });

        document.body.addEventListener('drop', async (event) => {
            if (this.elements.uploadModal?.classList.contains('dragging')) this.hideUploadBox();
            const files = event.dataTransfer?.files;
            if (!files || files.length === 0) return;
            await this.uploadFiles([...files].map((file) => ({ file })));
        });
    }
};
