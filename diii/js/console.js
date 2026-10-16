// Console output, the REPL input and what arrives from the device.
// Mixed into diiiApp (diii.js); `this` is the app.

export const consoleMethods = {
    outputText(text, options = {}) {
        const { autoScroll = true } = options;
        if (!this.elements.output) return;
        this.elements.output.appendChild(document.createTextNode(text));
        if (autoScroll) {
            this.elements.output.scrollTop = this.elements.output.scrollHeight;
        }
    },

    outputLine(text, options = {}) {
        this.outputText(`${text}\n`, options);
    },

    outputHTML(html, options = {}) {
        const { autoScroll = true } = options;
        if (!this.elements.output) return;
        const span = document.createElement('span');
        span.innerHTML = html;
        this.elements.output.appendChild(span);
        if (autoScroll) {
            this.elements.output.scrollTop = this.elements.output.scrollHeight;
        }
    },

    clearOutput() {
        if (this.elements.output) this.elements.output.textContent = '';
    },

    handleReplInput(event) {
        const input = this.elements.replInput;
        if (!input) return;

        if (event.key === 'Enter' && !event.shiftKey) {
            event.preventDefault();
            const code = input.value.trim();
            if (!code) return;
            this.sendReplCommand(code);
            return;
        }

        const noModifiers = !event.shiftKey && !event.altKey && !event.ctrlKey && !event.metaKey;

        if (noModifiers && event.key === 'ArrowUp') {
            event.preventDefault();
            this.navigateReplHistory('up');
            return;
        }

        if (noModifiers && event.key === 'ArrowDown') {
            event.preventDefault();
            this.navigateReplHistory('down');
            return;
        }

        if (this.historyIndex !== -1 && event.key.length === 1) {
            this.historyIndex = -1;
            this.currentInput = '';
        }
    },

    handleGlobalShortcuts(event) {
        if (event.defaultPrevented) return;

        const isConnectToggle =
            (event.metaKey || event.ctrlKey) &&
            event.shiftKey &&
            !event.altKey &&
            String(event.key).toLowerCase() === 'c';

        if (!isConnectToggle) return;

        event.preventDefault();
        this.toggleConnection();
    },

    navigateReplHistory(direction) {
        const input = this.elements.replInput;
        if (!input || this.commandHistory.length === 0) return;

        if (direction === 'up') {
            if (this.historyIndex === -1) {
                this.currentInput = input.value;
            }
            if (this.historyIndex < this.commandHistory.length - 1) {
                this.historyIndex += 1;
                input.value = this.commandHistory[this.commandHistory.length - 1 - this.historyIndex];
                input.selectionStart = input.selectionEnd = input.value.length;
            }
            return;
        }

        if (this.historyIndex === -1) return;
        this.historyIndex -= 1;
        input.value =
            this.historyIndex === -1
                ? this.currentInput
                : this.commandHistory[this.commandHistory.length - 1 - this.historyIndex];
        input.selectionStart = input.selectionEnd = input.value.length;
    },

    resetReplInput() {
        this.elements.replInput.value = '';
        this.historyIndex = -1;
        this.currentInput = '';
    },

    async sendReplCommand(code) {
        this.outputLine(`>> ${code}`);
        const isHelpShortcut = /^h$/i.test(code.trim());
        const isUploadShortcut = /^u$/i.test(code.trim());
        const isReUploadShortcut = /^r$/i.test(code.trim());

        if (this.commandHistory.length === 0 || this.commandHistory[this.commandHistory.length - 1] !== code) {
            this.commandHistory.push(code);
        }

        if (isHelpShortcut) {
            this.showHelp();
            this.resetReplInput();
            return;
        }

        if (isUploadShortcut) {
            this.openUploadPicker();
            this.resetReplInput();
            return;
        }

        if (isReUploadShortcut) {
            await this.refreshUploadAndRunLastScript();
            this.resetReplInput();
            return;
        }

        if (!this.iiiDevice.isConnected) {
            this.outputLine('no iii device connected.');
            this.resetReplInput();
            return;
        }

        try {
            const fileSelectMatch = code.match(/^\^\^s\s+(.+)$/);
            if (fileSelectMatch) {
                await this.openAndSelectRemoteFile(fileSelectMatch[1].trim());
                this.resetReplInput();
                return;
            }

            for (const line of code.split('\n')) {
                await this.iiiDevice.writeLine(line);
                await this.delay(1);
            }

            this.resetReplInput();
        } catch (error) {
            this.outputLine(`Error: ${error.message}`);
        }
    },

    handleiiiOutput(data) {
        const cleaned = String(data).replace(/\r/g, '');
        if (!cleaned) return;

        // LED frames can arrive at any time, so they must never end up in
        // captured output (file lists etc.) or in the console.
        if (cleaned.startsWith('^^grid(')) {
            this.handleGridFrame(cleaned);
            return;
        }

        if (this.handleSuppressedOutputLine(cleaned)) {
            return;
        }

        if (this.handleLuaCaptureLine(cleaned)) {
            return;
        }

        // e.g. the editor waiting for "-- file written: ..." after a save
        for (const observe of this.outputObservers || []) observe(cleaned);

        if (!cleaned.includes('^^')) {
            this.outputLine(cleaned);
            return;
        }

        const parts = cleaned.split('^^');
        for (const part of parts) {
            if (!part.trim()) continue;
            const eventMatch = part.match(/^(\w+)\(([^)]*)\)/);

            if (!eventMatch) {
                this.outputLine(part.trim());
                continue;
            }

            const event = eventMatch[1];
            const args = eventMatch[2] ? eventMatch[2].split(',').map((item) => item.trim()) : [];

            this.handleiiiEvent(event, args);
        }
    },

    handleiiiEvent(event, args) {
        this.outputLine(`^^${event}(${args.join(', ')})`);
    },

    queueSuppressedOutputLine(line, ttlMs = 2500) {
        const value = String(line || '');
        if (!value) return;
        this.pendingSuppressedOutputLines.push({
            line: value,
            expiresAt: Date.now() + ttlMs
        });
    },

    handleSuppressedOutputLine(line) {
        if (!this.pendingSuppressedOutputLines.length) return false;

        const now = Date.now();
        this.pendingSuppressedOutputLines = this.pendingSuppressedOutputLines.filter((entry) => entry.expiresAt > now);
        const matchIndex = this.pendingSuppressedOutputLines.findIndex((entry) => entry.line === line);
        if (matchIndex === -1) return false;

        this.pendingSuppressedOutputLines.splice(matchIndex, 1);
        return true;
    },

    showHelp() {
        this.outputLine('');
        this.outputLine(' diii helpers:');
        this.outputLine(' h            show this help');
        this.outputLine(' u            open file picker (same as upload button)');
        this.outputLine(' r            re-upload and run last uploaded script');
        this.outputLine(' Cmd/Ctrl+Shift+C  connect/disconnect');
        if (this.gridSupported) {
            this.outputLine(' grid button  show the LEDs and press keys from the browser');
        }
        this.outputLine('');
        this.outputLine(' common iii commands:');
        this.outputLine(' ^^i          init');
        this.outputLine(' ^^c          clean init');
        this.outputLine(' help()       print iii api');
        this.outputLine('');
        this.outputHTML(
            'Docs: <a href="https://monome.org/docs/iii/code" target="_blank" rel="noopener noreferrer">monome.org/docs/iii/code</a>\n'
        );
    }
};
