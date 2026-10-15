// Web Serial connection to an iii device: line framing and writes.

export class iiiConnection {
    constructor() {
        this.port = null;
        this.preferredPort = null;
        this.reader = null;
        this.writer = null;
        this.readableStreamClosed = null;
        this.writableStreamClosed = null;
        this.isConnected = false;
        this.lineBuffer = '';
        this.partialLineFlushTimer = null;
        this.partialLineFlushDelayMs = 40;
        this.partialLineFlushEnabled = true;
        this.onDataReceived = null;
        this.onConnectionChange = null;
        this._textEncoder = new TextEncoder();
    }

    async connect(port = null) {
        try {
            this.port = port || this.preferredPort;

            if (!this.port) {
                this.port = await navigator.serial.requestPort({
                    filters: [{ usbVendorId: 0xCAFE, usbProductId: 0x1101 }]
                });
            }

            this.preferredPort = this.port;

            await this.port.open({
                baudRate: 115200,
                dataBits: 8,
                stopBits: 1,
                parity: 'none',
                flowControl: 'none'
            });

            this.isConnected = true;

            const textDecoder = new TextDecoderStream();
            this.readableStreamClosed = this.port.readable.pipeTo(textDecoder.writable);
            this.reader = textDecoder.readable.getReader();

            const textEncoder = new TextEncoderStream();
            this.writableStreamClosed = textEncoder.readable.pipeTo(this.port.writable);
            this.writer = textEncoder.writable.getWriter();

            this.startReading();

            if (this.onConnectionChange) {
                this.onConnectionChange(true);
            }

            return true;
        } catch (error) {
            console.error('Connection error:', error);
            const browserError = String(error?.message || error || 'unknown serial error');

            if (this.onConnectionChange) {
                this.onConnectionChange(false, 'connection failed', { browserError, reason: 'connect-failed' });
            }
            return false;
        }
    }

    async startReading() {
        try {
            while (this.isConnected) {
                const { value, done } = await this.reader.read();
                if (done) break;
                if (!value) continue;

                this.lineBuffer += value;

                let newlineIndex = -1;
                while ((newlineIndex = this.lineBuffer.indexOf('\n')) !== -1) {
                    const line = this.lineBuffer.substring(0, newlineIndex);
                    this.lineBuffer = this.lineBuffer.substring(newlineIndex + 1);
                    if (line && this.onDataReceived) {
                        this.onDataReceived(line);
                    }
                }

                // Event lines (^^...) are only useful whole; never flush them early.
                if (this.lineBuffer && !this.lineBuffer.startsWith('^^')) {
                    this.schedulePartialLineFlush();
                } else {
                    this.clearPartialLineFlush();
                }
            }

            this.flushPartialLineBuffer();
        } catch (error) {
            console.error('Read error:', error);
            if (!this.isConnected) return;

            this.isConnected = false;
            if (this.reader) {
                await this.reader.cancel().catch(() => {});
            }
            if (this.writer) {
                await this.writer.close().catch(() => {});
            }

            this.reader = null;
            this.writer = null;
            this.flushPartialLineBuffer();

            if (this.port) {
                await this.port.close().catch(() => {});
                this.port = null;
            }

            if (this.onConnectionChange) {
                this.onConnectionChange(false, 'disconnected');
            }
        }
    }

    async write(data) {
        if (!this.isConnected || !this.writer) {
            throw new Error('Not connected');
        }

        let payload = String(data);
        const byteLength = this._textEncoder.encode(payload).length;
        if (byteLength % 64 === 0) {
            payload += '\n';
        }

        await this.writer.write(payload);
    }

    async writeLine(line) {
        await this.write(`${line}\n`);
    }

    async disconnect() {
        this.isConnected = false;
        this.clearPartialLineFlush();

        if (this.reader) {
            await this.reader.cancel().catch(() => {});
            await this.readableStreamClosed?.catch(() => {});
        }
        if (this.writer) {
            await this.writer.close().catch(() => {});
            await this.writableStreamClosed?.catch(() => {});
        }
        if (this.port) {
            await this.port.close().catch(() => {});
        }

        this.port = null;
        this.reader = null;
        this.writer = null;
        this.flushPartialLineBuffer();

        if (this.onConnectionChange) {
            this.onConnectionChange(false);
        }
    }

    schedulePartialLineFlush() {
        if (!this.partialLineFlushEnabled) return;

        this.clearPartialLineFlush();

        this.partialLineFlushTimer = setTimeout(() => {
            this.flushPartialLineBuffer();
        }, this.partialLineFlushDelayMs);
    }

    clearPartialLineFlush() {
        if (!this.partialLineFlushTimer) return;
        clearTimeout(this.partialLineFlushTimer);
        this.partialLineFlushTimer = null;
    }

    flushPartialLineBuffer() {
        this.clearPartialLineFlush();

        if (!this.lineBuffer) return;

        const partial = this.lineBuffer;
        this.lineBuffer = '';
        if (partial && this.onDataReceived) {
            this.onDataReceived(partial);
        }
    }
}
