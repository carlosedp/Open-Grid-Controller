// Running Lua on the device and capturing what it prints.
// Mixed into diiiApp (diii.js); `this` is the app.

export const luaMethods = {
    luaQuote(value) {
        return `'${String(value)
            .replace(/\\/g, '\\\\')
            .replace(/'/g, "\\'")
            .replace(/\r/g, '\\r')
            .replace(/\n/g, '\\n')}'`;
    },

    handleLuaCaptureLine(line) {
        const capture = this.pendingLuaCapture;
        if (!capture) return false;

        if (line === capture.beginToken) {
            capture.started = true;
            return true;
        }

        if (line === capture.endToken) {
            clearTimeout(capture.timeoutId);
            this.pendingLuaCapture = null;
            capture.resolve(capture.lines);
            return true;
        }

        if (!capture.started) return false;

        capture.lines.push(line);
        return true;
    },

    async executeLua(command) {
        if (!this.iiiDevice.isConnected) {
            throw new Error('Not connected to usb device');
        }

        await this.iiiDevice.writeLine(command);

        return true;
    },

    async executeLuaCapture(commands) {
        if (!this.iiiDevice.isConnected) {
            throw new Error('Not connected to usb device');
        }

        if (this.pendingLuaCapture) {
            throw new Error('Device is busy, please try again');
        }

        const captureId = ++this.luaCaptureSeq;
        const beginToken = `__webdiii_begin:${captureId}`;
        const endToken = `__webdiii_end:${captureId}`;

        const resultPromise = new Promise((resolve, reject) => {
            const timeoutId = setTimeout(() => {
                this.pendingLuaCapture = null;
                reject(new Error('Timed out waiting for device response'));
            }, 7000);

            this.pendingLuaCapture = {
                beginToken,
                endToken,
                started: false,
                lines: [],
                timeoutId,
                resolve,
                reject
            };
        });

        this.iiiDevice.partialLineFlushEnabled = false;
        this.iiiDevice.clearPartialLineFlush();

        try {
            await this.iiiDevice.writeLine(`print(${this.luaQuote(beginToken)})`);

            const lines = Array.isArray(commands)
                ? commands
                : String(commands).split('\n');

            for (const rawLine of lines) {
                const line = String(rawLine).trim();
                if (!line) continue;
                await this.iiiDevice.writeLine(`${line}`);
            }

            await this.iiiDevice.writeLine(`print(${this.luaQuote(endToken)})`);

            return await resultPromise;
        } finally {
            this.iiiDevice.partialLineFlushEnabled = true;
            this.iiiDevice.schedulePartialLineFlush();
        }
    }
};
