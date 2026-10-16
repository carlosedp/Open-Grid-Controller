// The MIDI drawer above the button bar. Open, it turns on og_midi_monitor()
// (Open Grid firmware og-0.3.8+) and lists what the device sends and
// receives; closed, the monitor is off again. The list itself is in
// midi-monitor.js. Mixed into diiiApp (diii.js); `this` is the app.

import { MidiMonitor, parseMidiLine } from './midi-monitor.js';

const DRAWER_KEY = 'webdiii.midiDrawer';

export const midiUiMethods = {
    getMidiMonitor() {
        if (!this.midiMonitor) {
            this.midiMonitor = new MidiMonitor({ list: this.elements.midiList, status: this.elements.midiStatus });
        }
        return this.midiMonitor;
    },

    restoreMidiDrawer() {
        let open = false;
        try {
            open = window.localStorage.getItem(DRAWER_KEY) === 'open';
        } catch {
            // storage blocked
        }
        this.setMidiDrawerOpen(open, { persist: false });
    },

    async setMidiDrawerOpen(open, { persist = true } = {}) {
        this.midiDrawerOpen = open;
        this.elements.midiDrawer?.classList.toggle('open', open);
        this.elements.midiToggleBtn?.setAttribute('aria-expanded', String(open));
        if (this.elements.midiToggleBtn) this.elements.midiToggleBtn.textContent = open ? 'midi ▾' : 'midi ▸';
        if (persist) {
            try {
                window.localStorage.setItem(DRAWER_KEY, open ? 'open' : 'closed');
            } catch {
                // storage blocked
            }
        }
        this.getMidiMonitor();
        await this.setMidiMonitor(open);
    },

    // Turns the device's reports on or off; says why when it can't.
    async setMidiMonitor(on) {
        const note = this.elements.midiNote;
        const showNote = (text) => {
            if (note) {
                note.textContent = text;
                note.hidden = !text;
            }
        };
        if (!this.iiiDevice.isConnected) {
            showNote(on ? 'connect a device to see its MIDI' : '');
            return;
        }
        try {
            if (this.midiMonitorSupported === undefined) {
                const probe = await this.executeLuaCapture('print(og_midi_monitor ~= nil)');
                this.midiMonitorSupported = probe.some((line) => line.trim() === 'true');
            }
            if (!this.midiMonitorSupported) {
                showNote(on ? 'the MIDI monitor needs Open Grid firmware og-0.3.8 or later' : '');
                return;
            }
            showNote('');
            await this.executeLua(`og_midi_monitor(${on ? 'true' : 'false'})`);
        } catch {
            // device busy or gone; connect() turns it on again
        }
    },

    // after (re)connecting: a new device may or may not have the monitor
    async resumeMidiMonitor() {
        this.midiMonitorSupported = undefined;
        if (this.midiDrawerOpen) await this.setMidiMonitor(true);
    },

    handleMidiLine(line) {
        const parsed = parseMidiLine(line);
        if (parsed) this.getMidiMonitor().push(...parsed);
    }
};
