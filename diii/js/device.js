// Connecting, reconnecting, and the device buttons (reboot, bootloader, ...).
// Mixed into diiiApp (diii.js); `this` is the app.

export const deviceMethods = {
    async toggleConnection() {
        if (this.iiiDevice.isConnected) {
            await this.disconnect();
            return;
        }
        await this.connect();
    },

    async connect(options = {}) {
        const { auto = false } = options;
        this.pendingConnectAttemptType = auto ? 'auto' : 'manual';

        try {
            if (!auto) {
                this.outputLine('Connecting to iii device...');
            }

            let reconnectPort = this.selectedPort;

            if (reconnectPort && 'serial' in navigator) {
                try {
                    const availablePorts = await navigator.serial.getPorts();
                    const matchingPort = this.findMatchingPort(availablePorts, reconnectPort, this.selectedPortInfo);
                    if (matchingPort) {
                        reconnectPort = matchingPort;
                        this.selectedPort = matchingPort;
                    }
                } catch {
                    // ignore and fall back to the remembered port object
                }
            }

            this.connectedDeviceLabel = reconnectPort ? this.getCachedDeviceLabel(reconnectPort) : null;

            let connected = await this.iiiDevice.connect(reconnectPort);

            if (!connected && this.selectedPort && !auto) {
                this.selectedPort = null;
                this.selectedPortInfo = null;
                this.connectedDeviceLabel = null;
                connected = await this.iiiDevice.connect();
            }

            if (connected) {
                this.selectedPort = this.iiiDevice.port;
                this.selectedPortInfo = this.getPortInfo(this.selectedPort);
                this.hasConnectedThisSession = true;
                this.autoReconnectEnabled = true;
                this.clearAutoReconnectTimer();
                this.setExplorerCollapsed(false);
                const cachedDeviceType = this.getCachedDeviceLabel(this.selectedPort);
                if (cachedDeviceType) {
                    this.connectedDeviceLabel = cachedDeviceType;
                    this.elements.replStatusText.textContent = cachedDeviceType;
                }

                const deviceType = cachedDeviceType || (await this.updateConnectedDeviceLabel());

                if (auto) {
                    if (deviceType) {
                        this.outputLine(`${deviceType} reconnected.`);
                    } else {
                        this.outputLine('Reconnected.');
                    }
                } else if (deviceType) {
                    this.outputLine(`${deviceType} connected! Ready to code.`);
                } else {
                    this.outputLine('Connected! Ready to code.');
                }

                if (!auto) {
                    this.outputLine('Drag and drop a lua file here to auto-upload.');
                    this.outputLine('');
                }

                await this.detectVirtualGrid();
                await this.requestFileListRefresh();
                return true;
            }

            if (auto) {
                this.scheduleAutoReconnect();
            }

            return false;
        } finally {
            this.pendingConnectAttemptType = null;
        }
    },

    async updateConnectedDeviceLabel() {
        if (!this.iiiDevice.isConnected || !this.elements.replStatusText) {
            return null;
        }

        try {
            const lines = await this.executeLuaCapture('print(device_id())');
            const deviceType = lines.map((line) => String(line).trim()).find((line) => line.length > 0);

            if (deviceType) {
                this.cacheDeviceLabel(deviceType, this.iiiDevice.port);
                this.connectedDeviceLabel = deviceType;
                this.elements.replStatusText.textContent = deviceType;
                return deviceType;
            }
        } catch {
            // fall back to cached label below
        }

        const cachedDeviceType = this.getCachedDeviceLabel(this.iiiDevice.port);
        this.connectedDeviceLabel = cachedDeviceType;
        this.elements.replStatusText.textContent = cachedDeviceType || 'connected';
        return cachedDeviceType;
    },

    cacheDeviceLabel(deviceLabel, port) {
        const normalizedLabel = String(deviceLabel || '').trim();
        if (!normalizedLabel || !port) {
            return;
        }

        this.deviceLabelsByPort.set(port, normalizedLabel);
    },

    getCachedDeviceLabel(port) {
        if (!port) {
            return null;
        }

        return this.deviceLabelsByPort.get(port) || null;
    },

    async disconnect(manual = true) {
        this.isManualDisconnect = manual;
        if (manual) {
            this.autoReconnectEnabled = false;
            this.clearAutoReconnectTimer();
        }

        await this.iiiDevice.disconnect();
        this.requestFileListRefresh();
        this.outputLine('');
        this.outputLine('disconnected');
        this.outputLine('');
        this.fileFreeSpaceBytes = null;
        this.deviceMemory = null;
        this.fileEntries = [];
        this.updateFileSpaceFooter(null);
        this.renderFileList();
    },

    handleConnectionChange(connected, error, detail = null) {
        if (!this.elements.connectionBtn || !this.elements.replStatusIndicator || !this.elements.replStatusText) return;

        if (connected) {
            this.elements.connectionBtn.textContent = 'disconnect';
            this.elements.replStatusIndicator.classList.add('connected');
            this.elements.replStatusText.textContent = this.connectedDeviceLabel || 'connected';
            this.elements.replInput?.focus();
            this.hasConnectedThisSession = true;
            this.isManualDisconnect = false;
            return;
        }

        this.elements.connectionBtn.textContent = 'connect';
        this.elements.replStatusIndicator.classList.remove('connected');
        this.elements.gridCells?.classList.add('offline');
        this.releaseAllGridKeys({ send: false });

        const browserError = String(detail?.browserError || '').trim();
        const isConnectFailure = error === 'connection failed';
        const isManualConnectFailure = isConnectFailure && this.pendingConnectAttemptType === 'manual';

        if (isManualConnectFailure || (!this.hasConnectedThisSession && isConnectFailure)) {
            this.elements.replStatusText.textContent = 'connection failed';
            if (browserError) {
                this.outputLine(`Browser error: ${browserError}`);
            }
        } else if (this.hasConnectedThisSession) {
            this.elements.replStatusText.textContent = 'disconnected';
        } else {
            this.elements.replStatusText.textContent = 'not connected';
        }

        if (error && error.includes('disconnected')) {
            this.outputLine('');
            this.outputLine(error);

            if (this.autoReconnectEnabled && !this.isManualDisconnect && this.selectedPort) {
                this.scheduleAutoReconnect();
            }
        }

        this.isManualDisconnect = false;
    },

    handleSerialPortConnect(event) {
        if (!this.autoReconnectEnabled || this.iiiDevice.isConnected || !this.selectedPort) {
            return;
        }

        const eventPort = event?.port;
        if (eventPort && !this.isSamePort(eventPort, this.selectedPort, this.selectedPortInfo)) {
            return;
        }

        this.scheduleAutoReconnect(150);
    },

    async handleSerialPortDisconnect(event) {
        if (!this.selectedPort) {
            return;
        }

        const eventPort = event?.port;
        if (eventPort && !this.isSamePort(eventPort, this.selectedPort, this.selectedPortInfo)) {
            return;
        }

        if (this.autoReconnectEnabled && !this.isManualDisconnect) {
            if (this.iiiDevice.isConnected) {
                await this.disconnect(false);
            }
            this.scheduleAutoReconnect();
        }
    },

    getPortInfo(port) {
        try {
            return port?.getInfo?.() || null;
        } catch {
            return null;
        }
    },

    isSamePort(portA, portB, preferredInfo = null) {
        if (!portA || !portB) return false;
        if (portA === portB) return true;

        const infoA = this.getPortInfo(portA);
        const infoB = preferredInfo || this.getPortInfo(portB);
        if (!infoA || !infoB) return false;

        return infoA.usbVendorId === infoB.usbVendorId && infoA.usbProductId === infoB.usbProductId;
    },

    findMatchingPort(ports, preferredPort, preferredInfo = null) {
        if (!Array.isArray(ports) || ports.length === 0) {
            return null;
        }

        const exactMatch = ports.find((port) => port === preferredPort);
        if (exactMatch) return exactMatch;

        if (!preferredInfo) {
            return null;
        }

        return (
            ports.find((port) => {
                const info = this.getPortInfo(port);
                if (!info) return false;

                return (
                    info.usbVendorId === preferredInfo.usbVendorId && info.usbProductId === preferredInfo.usbProductId
                );
            }) || null
        );
    },

    clearAutoReconnectTimer() {
        if (!this.autoReconnectTimer) return;
        clearTimeout(this.autoReconnectTimer);
        this.autoReconnectTimer = null;
    },

    scheduleAutoReconnect(delay = this.reconnectDelayMs) {
        if (!this.autoReconnectEnabled || this.iiiDevice.isConnected || !this.selectedPort || this.autoReconnectTimer) {
            return;
        }

        this.autoReconnectTimer = setTimeout(async () => {
            this.autoReconnectTimer = null;

            if (!this.autoReconnectEnabled || this.iiiDevice.isConnected) {
                return;
            }

            await this.connect({ auto: true });
        }, delay);
    },

    restartDevice() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }
        this.outputLine('> ^^r');
        this.iiiDevice.writeLine('^^r');

        if (this.reconnectAfterRestartTimer) {
            clearTimeout(this.reconnectAfterRestartTimer);
        }

        this.reconnectAfterRestartTimer = setTimeout(async () => {
            if (!this.iiiDevice.isConnected) {
                await this.connect({ auto: true });
            }
            this.reconnectAfterRestartTimer = null;
        }, 1000);
    },

    bootloaderDevice() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }
        this.outputLine('> ^^b');
        this.iiiDevice.writeLine('^^b');
    },

    // ^^i: restart Lua and run lib.lua and init.lua (the "first" script)
    // again, without rebooting, so the USB connection stays up.
    reinitDevice() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }
        this.releaseAllGridKeys();
        this.outputLine('> ^^i');
        this.iiiDevice.writeLine('^^i');
    },

    async reformatFs() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }

        if (!window.confirm('Reformat filesystem? This will erase all files on your iii device.')) {
            return;
        }

        try {
            await this.executeLuaCapture('fs_reformat()');
            this.outputLine('Filesystem reformatted.');
            await this.requestFileListRefresh();
        } catch (error) {
            this.outputLine(`Reformat error: ${error.message}`);
        }
    },

    helpDevice() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }
        this.showHelp();
        this.outputLine('> help()');
        this.iiiDevice.writeLine('help()');
    }
};
