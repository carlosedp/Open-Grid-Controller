/**
 * diii web app
 * Minimal serial REPL + script browser iii devices.
 *
 * The app is one class; its methods live in js/*.js by topic and are
 * mixed in below, so they all share `this`.
 */

import { iiiConnection } from './js/connection.js';
import { layoutMethods } from './js/layout.js';
import { consoleMethods } from './js/console.js';
import { deviceMethods } from './js/device.js';
import { luaMethods } from './js/lua.js';
import { uploadMethods } from './js/uploads.js';
import { fileMethods } from './js/files.js';
import { gridMethods } from './js/grid.js';
import { backupUiMethods } from './js/backup-ui.js';
import { editorUiMethods } from './js/editor-ui.js';
import { firmwareUiMethods } from './js/firmware-ui.js';
import { midiUiMethods } from './js/midi-ui.js';

class diiiApp {
    constructor() {
        this.iiiDevice = new iiiConnection();
        this.selectedPort = null;
        this.selectedPortInfo = null;
        this.deviceLabelsByPort = new WeakMap();
        this.connectedDeviceLabel = null;
        this.hasConnectedThisSession = false;
        this.pendingConnectAttemptType = null;
        this.autoReconnectEnabled = false;
        this.autoReconnectTimer = null;
        this.reconnectDelayMs = 900;
        this.isManualDisconnect = false;

        this.commandHistory = [];
        this.historyIndex = -1;
        this.currentInput = '';
        this.pendingLuaCapture = null;
        this.luaCaptureSeq = 0;
        this.reconnectAfterRestartTimer = null;
        this.fileEntries = [];
        this.firstBadgeFileNames = new Set();
        this.fileFreeSpaceBytes = null;
        this.openMenuFile = null;
        this.isExplorerCollapsed = true;
        this.fileRunQueue = Promise.resolve();
        this.fileRefreshQueue = Promise.resolve();
        this.fileRefreshInFlight = false;
        this.fileRefreshRequested = false;
        this.pendingSuppressedOutputLines = [];
        this.lastUploadedScript = null;
        this.explorerWidthStorageKey = 'webdiii.explorerWidth';
        this.explorerWidthDefault = 280;
        this.explorerWidthMin = 220;
        this.explorerWidthMax = 520;
        this.isResizingExplorer = false;
        this.explorerResizePointerId = null;
        this.explorerResizeStartX = 0;
        this.explorerResizeStartWidth = this.explorerWidthDefault;

        // Virtual grid: shows the LEDs of devices that can mirror them
        // (og_mirror, Open Grid firmware) and sends presses with og_key.
        this.gridSupported = false;
        this.gridPanelOpen = false;
        this.gridSize = { x: 16, y: 8 };
        this.gridCells = [];
        this.gridLevels = [];
        this.gridPointers = new Map();
        this.uploadInProgress = false;

        this.cacheElements();
        this.bindEvents();
        this.restoreExplorerWidth();
        this.setExplorerCollapsed(true);
        this.checkBrowserSupport();
        this.renderFileList();

        this.outputLine('//// welcome. connect to an iii compatible grid or arc to begin.');
        this.restoreHistory();
        this.restoreMidiDrawer();
        this.announceDraft();
    }

    cacheElements() {
        this.elements = {
            scriptReferenceBtn: document.getElementById('scriptReferenceBtn'),
            referenceModal: document.getElementById('referenceModal'),
            referenceSearch: document.getElementById('referenceSearch'),
            referenceList: document.getElementById('referenceList'),
            referenceCloseBtn: document.getElementById('referenceCloseBtn'),

            splitContainer: document.getElementById('splitContainer'),
            fileExplorerPane: document.getElementById('fileExplorerPane'),
            explorerResizer: document.getElementById('explorerResizer'),
            fileList: document.getElementById('fileList'),
            fileSpaceFooter: document.getElementById('fileSpaceFooter'),
            toggleExplorerBtn: document.getElementById('toggleExplorerBtn'),
            refreshExplorerBtn: document.getElementById('refreshExplorerBtn'),
            explorerChevron: document.getElementById('explorerChevron'),

            connectionBtn: document.getElementById('replConnectionBtn'),
            replStatusPill: document.getElementById('replStatusPill'),
            replStatusIndicator: document.getElementById('replStatusIndicator'),
            replStatusText: document.getElementById('replStatusText'),

            output: document.getElementById('output'),
            replInput: document.getElementById('replInput'),
            replPane: document.getElementById('replPane'),
            uploadBtn: document.getElementById('uploadBtn'),
            reinitBtn: document.getElementById('reinitBtn'),
            firmwareBtn: document.getElementById('firmwareBtn'),
            midiDrawer: document.getElementById('midiDrawer'),
            midiToggleBtn: document.getElementById('midiToggleBtn'),
            midiStatus: document.getElementById('midiStatus'),
            midiNote: document.getElementById('midiNote'),
            midiList: document.getElementById('midiList'),
            midiShowIn: document.getElementById('midiShowIn'),
            midiShowOut: document.getElementById('midiShowOut'),
            midiShowClock: document.getElementById('midiShowClock'),
            midiPauseBtn: document.getElementById('midiPauseBtn'),
            midiClearBtn: document.getElementById('midiClearBtn'),
            firmwareModal: document.getElementById('firmwareModal'),
            firmwareBody: document.getElementById('firmwareBody'),
            firmwareCloseBtn: document.getElementById('firmwareCloseBtn'),
            restartBtn: document.getElementById('restartBtn'),
            bootloaderBtn: document.getElementById('bootloaderBtn'),
            reformatBtn: document.getElementById('reformatBtn'),
            backupBtn: document.getElementById('backupBtn'),
            restoreBtn: document.getElementById('restoreBtn'),

            deviceMenuBtn: document.getElementById('deviceMenuBtn'),
            deviceMenu: document.getElementById('deviceMenu'),
            newFileBtn: document.getElementById('newFileBtn'),

            workArea: document.getElementById('workArea'),
            editorPane: document.getElementById('editorPane'),
            editorExpandBtn: document.getElementById('editorExpandBtn'),
            editorExpandTitle: document.getElementById('editorExpandTitle'),
            editorLayoutBtn: document.getElementById('editorLayoutBtn'),
            editorMinimizeBtn: document.getElementById('editorMinimizeBtn'),
            editorResizer: document.getElementById('editorResizer'),
            editorHost: document.getElementById('editorHost'),
            editorTitle: document.getElementById('editorTitle'),
            editorDirty: document.getElementById('editorDirty'),
            editorSaveBtn: document.getElementById('editorSaveBtn'),
            editorSaveAsBtn: document.getElementById('editorSaveAsBtn'),
            editorRunBtn: document.getElementById('editorRunBtn'),
            editorCloseBtn: document.getElementById('editorCloseBtn'),
            helpBtn: document.getElementById('helpBtn'),
            clearBtn: document.getElementById('clearBtn'),
            gridBtn: document.getElementById('gridBtn'),
            gridPanel: document.getElementById('gridPanel'),
            gridCells: document.getElementById('gridCells'),

            fileInput: document.getElementById('fileInput'),

            browserWarning: document.getElementById('browserWarning'),
            closeWarning: document.getElementById('closeWarning')
        };
    }

    bindEvents() {
        const on = (element, eventName, handler) => {
            if (element) element.addEventListener(eventName, handler);
        };

        on(this.elements.connectionBtn, 'click', () => this.toggleConnection());
        on(this.elements.replStatusPill, 'click', () => this.toggleConnection());
        on(this.elements.replInput, 'keydown', (e) => this.handleReplInput(e));
        on(document, 'keydown', (e) => this.handleGlobalShortcuts(e));
        on(this.elements.toggleExplorerBtn, 'click', () => this.toggleExplorer());
        on(this.elements.refreshExplorerBtn, 'click', () => this.requestFileListRefresh());
        on(this.elements.explorerResizer, 'pointerdown', (e) => this.startExplorerResize(e));
        on(this.elements.explorerResizer, 'keydown', (e) => this.handleExplorerResizerKeydown(e));
        on(this.elements.uploadBtn, 'click', () => this.openUploadPicker());
        on(this.elements.reinitBtn, 'click', () => this.reinitDevice());
        on(this.elements.firmwareBtn, 'click', () => this.openFirmwareDialog());
        on(this.elements.midiToggleBtn, 'click', () => this.setMidiDrawerOpen(!this.midiDrawerOpen));
        on(this.elements.midiShowIn, 'change', (e) => this.getMidiMonitor().setFilter('in', e.target.checked));
        on(this.elements.midiShowOut, 'change', (e) => this.getMidiMonitor().setFilter('out', e.target.checked));
        on(this.elements.midiShowClock, 'change', (e) => this.getMidiMonitor().setFilter('clock', e.target.checked));
        on(this.elements.midiClearBtn, 'click', () => this.getMidiMonitor().clear());
        on(this.elements.midiPauseBtn, 'click', () => {
            const monitor = this.getMidiMonitor();
            monitor.setPaused(!monitor.paused);
            this.elements.midiPauseBtn.textContent = monitor.paused ? 'resume' : 'pause';
            this.elements.midiPauseBtn.classList.toggle('active', monitor.paused);
        });
        on(this.elements.firmwareCloseBtn, 'click', () => this.closeFirmwareDialog());
        on(this.elements.firmwareModal, 'click', (e) => {
            if (e.target === this.elements.firmwareModal) this.closeFirmwareDialog();
        });
        on(document, 'keydown', (e) => {
            if (e.key === 'Escape' && this.elements.firmwareModal?.style.display === 'flex') this.closeFirmwareDialog();
        });
        on(this.elements.restartBtn, 'click', () => this.restartDevice());
        on(this.elements.bootloaderBtn, 'click', () => this.bootloaderDevice());
        on(this.elements.reformatBtn, 'click', () => this.reformatFs());
        on(this.elements.backupBtn, 'click', () => this.backupToZip());
        on(this.elements.restoreBtn, 'click', () => this.restoreFromZip());
        on(this.elements.editorSaveBtn, 'click', () => this.editorSave());
        on(this.elements.editorSaveAsBtn, 'click', () => this.editorSaveAs());
        on(this.elements.editorRunBtn, 'click', () => this.editorRun());
        on(this.elements.editorCloseBtn, 'click', () => this.closeEditor());
        on(this.elements.editorLayoutBtn, 'click', () => this.toggleEditorLayout());
        on(this.elements.editorMinimizeBtn, 'click', () => this.setEditorCollapsed(true));
        on(this.elements.editorExpandBtn, 'click', () => this.setEditorCollapsed(false));
        on(this.elements.newFileBtn, 'click', () => this.newFile());
        on(this.elements.deviceMenuBtn, 'click', () =>
            this.setDeviceMenuOpen(!this.elements.deviceMenu?.classList.contains('open'))
        );
        on(document, 'click', (e) => this.handleDeviceMenuDocumentEvent(e));
        on(document, 'keydown', (e) => this.handleDeviceMenuDocumentEvent(e));
        on(this.elements.editorResizer, 'pointerdown', (e) => this.startEditorResize(e));
        on(window, 'beforeunload', (e) => {
            if (this.scriptEditor?.dirty) e.preventDefault();
        });
        on(this.elements.helpBtn, 'click', () => this.helpDevice());
        on(this.elements.clearBtn, 'click', () => this.clearOutput());
        on(this.elements.fileSpaceFooter, 'click', () => this.iiiDevice.isConnected && this.requestFileListRefresh());
        on(this.elements.output, 'click', (e) => this.handleOutputClick(e));
        on(this.elements.gridBtn, 'click', () => this.toggleGridPanel());
        on(this.elements.gridCells, 'pointerdown', (e) => this.handleGridPointerDown(e));
        on(window, 'pointerup', (e) => this.handleGridPointerUp(e));
        on(window, 'pointercancel', (e) => this.handleGridPointerUp(e));
        on(this.elements.fileInput, 'change', (e) => this.handleFileSelect(e));
        on(document, 'click', (e) => this.handleDocumentClick(e));

        on(this.elements.closeWarning, 'click', () => {
            this.elements.browserWarning.style.display = 'none';
        });

        on(this.elements.scriptReferenceBtn, 'click', () => this.openReference());
        on(this.elements.referenceSearch, 'input', () => this.renderReferenceList());
        on(this.elements.referenceCloseBtn, 'click', () => this.closeReference());
        on(this.elements.referenceModal, 'click', (e) => {
            if (e.target === this.elements.referenceModal) this.closeReference();
        });
        on(document, 'keydown', (e) => {
            if (e.key === 'Escape' && this.elements.referenceModal?.style.display === 'flex') this.closeReference();
        });

        this.iiiDevice.onDataReceived = (data) => this.handleiiiOutput(data);
        this.iiiDevice.onConnectionChange = (connected, error, detail) =>
            this.handleConnectionChange(connected, error, detail);

        if ('serial' in navigator) {
            navigator.serial.addEventListener('connect', (event) => this.handleSerialPortConnect(event));
            navigator.serial.addEventListener('disconnect', (event) => this.handleSerialPortDisconnect(event));
        }

        this.setupDragAndDrop();
    }

    checkBrowserSupport() {
        if ('serial' in navigator) return;
        if (this.elements.browserWarning) this.elements.browserWarning.style.display = 'flex';
        if (this.elements.connectionBtn) this.elements.connectionBtn.disabled = true;
        this.outputLine('ERROR: Web Serial API not supported in this browser.');
        this.outputLine('Please use Chrome, Edge, or Opera.');
    }

    delay(ms) {
        return new Promise((resolve) => setTimeout(resolve, ms));
    }
}

Object.assign(
    diiiApp.prototype,
    layoutMethods,
    consoleMethods,
    deviceMethods,
    luaMethods,
    uploadMethods,
    fileMethods,
    gridMethods,
    backupUiMethods,
    editorUiMethods,
    firmwareUiMethods,
    midiUiMethods
);

document.addEventListener('DOMContentLoaded', () => {
    new diiiApp();

    if ('serviceWorker' in navigator) {
        navigator.serviceWorker.register('sw.js').catch((error) => {
            console.error('Service worker registration failed:', error);
        });
    }
});
