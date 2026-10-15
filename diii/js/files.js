// File explorer contents and the per-file actions.
// Mixed into diiiApp (diii.js); `this` is the app.

export const fileMethods = {
    formatSizeKb(bytes) {
        const kb = (Number(bytes) || 0) / 1024;
        return `${Math.round(kb)}kb`;
    },

    updateFileSpaceFooter(bytes) {
        if (!this.elements.fileSpaceFooter) return;

        if (!Number.isFinite(Number(bytes))) {
            this.elements.fileSpaceFooter.textContent = 'free space: -- kb';
            return;
        }

        const kb = Math.round(Number(bytes) / 1024);
        this.elements.fileSpaceFooter.textContent = `free space: ${kb} kb`;
    },

    isInitFile(name) {
        return name === 'init.lua' || name === 'init';
    },

    getSortedFileEntries() {
        const entries = [...this.fileEntries];
        return entries.sort((a, b) => {
            const order = (name) => {
                if (this.isInitFile(name)) return 0;
                if (name === 'lib.lua') return 1;
                return 2;
            };

            const rankDiff = order(a.name) - order(b.name);
            if (rankDiff !== 0) return rankDiff;
            return a.name.localeCompare(b.name);
        });
    },

    handleDocumentClick(event) {
        if (!this.openMenuFile) return;
        if (event.target?.closest('.file-row')) return;
        this.openMenuFile = null;
        this.renderFileList();
    },

    renderFileList() {
        if (!this.elements.fileList) return;

        this.elements.fileList.textContent = '';

        if (!this.iiiDevice.isConnected) {
            const empty = document.createElement('div');
            empty.className = 'file-list-empty';
            empty.textContent = 'connect to load files';
            this.elements.fileList.appendChild(empty);
            return;
        }

        if (this.fileEntries.length === 0) {
            const empty = document.createElement('div');
            empty.className = 'file-list-empty';
            empty.textContent = 'no files';
            this.elements.fileList.appendChild(empty);
            return;
        }

        const sortedEntries = this.getSortedFileEntries();
        const pinnedCount = sortedEntries.filter(
            (entry) => entry.name === 'lib.lua' || this.isInitFile(entry.name)
        ).length;

        for (let index = 0; index < sortedEntries.length; index += 1) {
            const entry = sortedEntries[index];
            const row = document.createElement('div');
            row.className = 'file-row';

            const main = document.createElement('div');
            main.className = 'file-main';
            const isLibFile = entry.name === 'lib.lua';
            const isInitLuaFile = entry.name === 'init.lua';

            if (!isLibFile && !isInitLuaFile) {
                const playBtn = document.createElement('button');
                playBtn.className = 'file-play-btn';
                playBtn.type = 'button';
                playBtn.textContent = '▶';
                playBtn.setAttribute('aria-label', `run ${entry.name}`);
                playBtn.addEventListener('click', async (event) => {
                    event.stopPropagation();
                    await this.enqueueRunFile(entry.name);
                });
                main.appendChild(playBtn);
            }

            const label = document.createElement('div');
            label.className = 'file-label';
            label.textContent = Number.isFinite(entry.size)
                ? `${entry.name} (${this.formatSizeKb(entry.size)})`
                : entry.name;

            main.appendChild(label);

            if (entry.name !== 'init.lua' && this.firstBadgeFileNames.has(entry.name)) {
                const firstBadge = document.createElement('span');
                firstBadge.className = 'file-first-pill';
                firstBadge.textContent = 'first';
                firstBadge.setAttribute('aria-label', `${entry.name} is configured in init.lua`);
                main.appendChild(firstBadge);
            }

            const menuBtn = document.createElement('button');
            menuBtn.className = 'file-menu-btn';
            menuBtn.type = 'button';
            menuBtn.textContent = '⋯';
            menuBtn.setAttribute('aria-label', `actions for ${entry.name}`);
            menuBtn.addEventListener('click', (event) => {
                event.stopPropagation();
                this.openMenuFile = this.openMenuFile === entry.name ? null : entry.name;
                this.renderFileList();
            });

            const menu = document.createElement('div');
            menu.className = `file-menu${this.openMenuFile === entry.name ? ' open' : ''}`;

            const actions = isInitLuaFile
                ? [
                      { label: 'read', fn: () => this.showFile(entry.name) },
                      { label: 'delete', fn: () => this.deleteFile(entry.name) }
                  ]
                : isLibFile
                  ? [
                        { label: 'download', fn: () => this.downloadFile(entry.name) },
                        { label: 'read', fn: () => this.showFile(entry.name) }
                    ]
                  : [
                        { label: 'run', fn: () => this.enqueueRunFile(entry.name) },
                        { label: 'download', fn: () => this.downloadFile(entry.name) },
                        { label: 'first', fn: () => this.configureFirst(entry.name) },
                        { label: 'read', fn: () => this.showFile(entry.name) },
                        { label: 'delete', fn: () => this.deleteFile(entry.name) }
                    ];

            for (const action of actions) {
                const item = document.createElement('button');
                item.type = 'button';
                item.className = 'file-menu-item';
                item.textContent = action.label;
                item.addEventListener('click', async (event) => {
                    event.stopPropagation();
                    this.openMenuFile = null;
                    this.renderFileList();
                    await action.fn();
                });
                menu.appendChild(item);
            }

            row.appendChild(main);
            row.appendChild(menuBtn);
            row.appendChild(menu);
            this.elements.fileList.appendChild(row);

            if (pinnedCount > 0 && index === pinnedCount - 1 && index < sortedEntries.length - 1) {
                const separator = document.createElement('div');
                separator.className = 'file-list-separator';
                this.elements.fileList.appendChild(separator);
            }
        }
    },

    async refreshFileList() {
        if (!this.iiiDevice.isConnected) {
            this.fileEntries = [];
            this.firstBadgeFileNames = new Set();
            this.fileFreeSpaceBytes = null;
            this.updateFileSpaceFooter(null);
            this.renderFileList();
            return;
        }

        try {
            const lsLines = await this.executeLuaCapture(
                'for _, __name in ipairs(fs_list_files()) do local __size = fs_file_size(__name) or 0; print("__webdiii_file\\t" .. __name .. "\\t" .. tostring(__size)) end'
            );
            const memLines = await this.executeLuaCapture('print(fs_free_space())');

            const entries = this.parseFileEntriesFromLs(lsLines);
            this.fileFreeSpaceBytes = this.parseMemoryFooterFromMem(memLines);

            this.fileEntries = entries;

            try {
                await this.refreshFirstBadgeFileNames(entries);
            } catch {
                this.firstBadgeFileNames = new Set();
            }

            this.updateFileSpaceFooter(this.fileFreeSpaceBytes);
            this.renderFileList();
        } catch (error) {
            this.firstBadgeFileNames = new Set();
            this.fileFreeSpaceBytes = null;
            this.updateFileSpaceFooter(null);
            this.outputLine(`File list error: ${error.message}`);
        }
    },

    requestFileListRefresh() {
        this.fileRefreshRequested = true;

        this.fileRefreshQueue = this.fileRefreshQueue
            .catch(() => {})
            .then(async () => {
                if (this.fileRefreshInFlight || !this.fileRefreshRequested) {
                    return;
                }

                this.fileRefreshRequested = false;
                this.fileRefreshInFlight = true;

                try {
                    await this.refreshFileList();
                } finally {
                    this.fileRefreshInFlight = false;
                }

                if (this.fileRefreshRequested) {
                    await this.requestFileListRefresh();
                }
            });

        return this.fileRefreshQueue;
    },

    getFirstRunFileTargetFromInit(initContent) {
        const content = String(initContent || '');
        const withoutBlockComments = content.replace(/--\[\[[\s\S]*?\]\]/g, '');
        const withoutLineComments = withoutBlockComments.replace(/--.*$/gm, '');
        const match = withoutLineComments.match(/fs_run_file\s*\(\s*(['"])([^'"]+)\1\s*\)/);
        return match?.[2]?.trim() || '';
    },

    parseFileEntriesFromLs(lines) {
        const entries = [];
        const seenNames = new Set();

        for (const rawLine of lines) {
            const line = String(rawLine || '').trim();

            if (!line.startsWith('__webdiii_file\t')) continue;

            const parts = line.split('\t');
            if (parts.length < 3) continue;

            const name = String(parts[1] || '').trim();
            const isLua = name.toLowerCase().endsWith('.lua');
            const isInit = name === 'init';
            if (!isLua && !isInit) continue;
            if (seenNames.has(name)) continue;
            seenNames.add(name);

            const parsedSize = Number.parseInt(parts[2], 10);
            entries.push({
                name,
                size: Number.isFinite(parsedSize) ? parsedSize : null
            });
        }

        return entries;
    },

    parseMemoryFooterFromMem(lines) {
        const bytes = Number.parseInt(lines[0], 10);
        return Number.isFinite(bytes) && bytes >= 0 ? bytes : null;
    },

    async refreshFirstBadgeFileNames(entries) {
        const hasInitLua = entries.some((entry) => entry.name === 'init.lua');
        if (!hasInitLua) {
            this.firstBadgeFileNames = new Set();
            return;
        }

        const initContent = await this.readRemoteFile('init.lua');
        const targetName = this.getFirstRunFileTargetFromInit(initContent);

        if (!targetName) {
            this.firstBadgeFileNames = new Set();
            return;
        }

        const hasMatchingFile = entries.some((entry) => entry.name === targetName);
        this.firstBadgeFileNames = hasMatchingFile ? new Set([targetName]) : new Set();
    },

    async readRemoteFile(fileName) {
        const lines = await this.executeLuaCapture(`cat(${this.luaQuote(fileName)})`);
        return lines.join('\n');
    },

    async configureFirst(fileName) {
        try {
            await this.executeLuaCapture(`first(${this.luaQuote(fileName)})`);
            this.outputLine(`${fileName} will now run at at startup`);
            await this.requestFileListRefresh();
        } catch (error) {
            this.outputLine(`First error: ${error.message}`);
        }
    },

    async downloadFile(fileName) {
        try {
            const content = await this.readRemoteFile(fileName);
            const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });
            const url = URL.createObjectURL(blob);
            const link = document.createElement('a');
            link.href = url;
            link.download = fileName;
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            URL.revokeObjectURL(url);
            this.outputLine(`Downloaded ${fileName}`);
        } catch (error) {
            this.outputLine(`Download error: ${error.message}`);
        }
    },

    async showFile(fileName) {
        try {
            if (!this.elements.output) return;

            const topSpacerLine = document.createElement('span');
            topSpacerLine.textContent = '\n';
            this.elements.output.appendChild(topSpacerLine);

            const headerLine = document.createElement('span');
            headerLine.textContent = `${fileName} contents:\n\n`;
            this.elements.output.appendChild(headerLine);

            const lines = await this.executeLuaCapture(`cat(${this.luaQuote(fileName)})`);
            for (const line of lines) {
                this.outputLine(line, { autoScroll: false });
            }

            this.outputText('\n', { autoScroll: false });

            this.elements.output.scrollTop = topSpacerLine.offsetTop;
        } catch (error) {
            this.outputLine(`Show error: ${error.message}`);
        }
    },

    async enqueueRunFile(fileName) {
        const task = async () => {
            await this.runFile(fileName);
        };

        this.fileRunQueue = this.fileRunQueue.catch(() => {}).then(task);

        return this.fileRunQueue;
    },

    async runFile(fileName) {
        this.queueSuppressedOutputLine('-- re-init with no script', 8000);
        this.queueSuppressedOutputLine('-- init: skip script', 8000);
        this.queueSuppressedOutputLine('-- lua lib', 8000);
        this.outputLine(`running ${fileName}...`);
        await this.iiiDevice.writeLine('^^c');
        await this.delay(500);
        await this.executeLua(`fs_run_file("lib.lua")`);
        await this.delay(500);
        await this.executeLua(`fs_run_file(${this.luaQuote(fileName)})`);
    },

    async deleteFile(fileName) {
        if (!window.confirm(`Delete ${fileName}?`)) {
            return;
        }

        try {
            await this.executeLuaCapture(`fs_remove_file(${this.luaQuote(fileName)})`);
            this.outputLine(`Deleted ${fileName}`);
            await this.requestFileListRefresh();
        } catch (error) {
            this.outputLine(`Delete error: ${error.message}`);
        }
    }
};
