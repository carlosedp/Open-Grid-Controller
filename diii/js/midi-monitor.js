// @ts-check
// The MIDI monitor's list: decoded messages in and out, filters, pause, and a
// tempo read from MIDI clock. Rows are added once per animation frame, so a
// burst of messages doesn't stall the page; the oldest go past MAX_ROWS.

import { decodeMidi } from './midi-decode.js';

const MAX_ROWS = 500;

/**
 * @typedef {object} MonitorElements
 * @property {HTMLElement} list
 * @property {HTMLElement} status one-line summary (counts, tempo)
 */

export class MidiMonitor {
    /** @param {MonitorElements} elements */
    constructor(elements) {
        this.elements = elements;
        this.paused = false;
        this.filters = { in: true, out: true, clock: false };
        this.counts = { in: 0, out: 0, clock: 0 };
        /** @type {number[]} arrival times of recent clock messages */
        this.clockTimes = [];
        /** @type {HTMLElement[]} */
        this.pending = [];
        this.frame = 0;
        /** @type {ReturnType<typeof setTimeout> | 0} */
        this.statusTimer = 0;
        /**
         * Counts from the device (og_midi_counts, og-0.3.9+): totals since it
         * booted, and the totals at the last clear. null: count our own rows.
         * @type {{ in: number, out: number, bpm: number | null, base: { in: number, out: number } } | null}
         */
        this.device = null;
        this.startTime = 0;
        this.updateStatus();
    }

    /**
     * One message from the device ("i" received, "o" sent).
     * @param {'i' | 'o'} dir
     * @param {number} b1
     * @param {number} b2
     * @param {number} b3
     * @param {number} [time] ms, performance.now() by default
     */
    push(dir, b1, b2, b3, time = performance.now()) {
        const message = decodeMidi(b1, b2, b3);
        if (message.kind === 'clock') {
            this.counts.clock++;
            this.clockTimes.push(time);
            if (this.clockTimes.length > 49) this.clockTimes.shift();
        }
        this.counts[dir === 'i' ? 'in' : 'out']++;
        this.scheduleStatus();

        if (this.paused) return;
        if (!this.filters[dir === 'i' ? 'in' : 'out']) return;
        if (message.realtime && !this.filters.clock) return;

        if (!this.startTime) this.startTime = time;
        this.pending.push(this.renderRow(dir, message, [b1, b2, b3], time - this.startTime));
        this.scheduleFlush();
    }

    /**
     * @param {'i' | 'o'} dir
     * @param {import('./midi-decode.js').MidiMessage} message
     * @param {number[]} bytes
     * @param {number} elapsed ms since the first row
     */
    renderRow(dir, message, bytes, elapsed) {
        const row = document.createElement('div');
        row.className = `midi-row midi-${dir === 'i' ? 'in' : 'out'} midi-kind-${message.kind}`;
        /** @param {string} cls @param {string} text */
        const cell = (cls, text) => {
            const span = document.createElement('span');
            span.className = cls;
            span.textContent = text;
            row.append(span);
        };
        cell('midi-time', (elapsed / 1000).toFixed(3));
        cell('midi-dir', dir === 'i' ? '← in' : '→ out');
        cell('midi-ch', message.ch ? `ch ${message.ch}` : '');
        cell('midi-text', message.text);
        cell('midi-raw', bytes.map((b) => b.toString(16).padStart(2, '0')).join(' '));
        return row;
    }

    scheduleFlush() {
        if (this.frame) return;
        this.frame = requestAnimationFrame(() => {
            this.frame = 0;
            const list = this.elements.list;
            const follow = list.scrollHeight - list.scrollTop - list.clientHeight < 24;
            list.append(...this.pending.splice(0));
            while (list.childElementCount > MAX_ROWS) list.firstElementChild?.remove();
            if (follow) list.scrollTop = list.scrollHeight;
        });
    }

    scheduleStatus() {
        if (this.statusTimer) return;
        this.statusTimer = setTimeout(() => {
            this.statusTimer = 0;
            this.updateStatus();
        }, 200);
    }

    /** Tempo from the last clocks (24 per quarter note); null without a recent clock. */
    bpm() {
        const t = this.clockTimes;
        if (t.length < 13 || performance.now() - t[t.length - 1] > 1000) return null;
        const perClock = (t[t.length - 1] - t[0]) / (t.length - 1);
        return perClock > 0 ? 60000 / (perClock * 24) : null;
    }

    /**
     * A "^^midic" summary: totals since the device booted, tempo x10 (0: no clock).
     * @param {number} totalIn
     * @param {number} totalOut
     * @param {number} bpmX10
     */
    setDeviceCounts(totalIn, totalOut, bpmX10) {
        // first summary, or the device restarted (its totals went back): count from here
        const base =
            !this.device || totalIn < this.device.base.in || totalOut < this.device.base.out
                ? { in: totalIn, out: totalOut }
                : this.device.base;
        this.device = { in: totalIn, out: totalOut, bpm: bpmX10 ? bpmX10 / 10 : null, base };
        this.updateStatus();
    }

    updateStatus() {
        if (this.device) {
            const d = this.device;
            const parts = [`in ${d.in - d.base.in}`, `out ${d.out - d.base.out}`];
            if (d.bpm) parts.push(`clock ${d.bpm.toFixed(1)} bpm`);
            if (this.paused) parts.push('paused');
            this.elements.status.textContent = parts.join(' · ');
            return;
        }
        const bpm = this.bpm();
        const parts = [`in ${this.counts.in}`, `out ${this.counts.out}`];
        if (this.counts.clock) parts.push(bpm ? `clock ${bpm.toFixed(1)} bpm` : `clock ${this.counts.clock}`);
        if (this.paused) parts.push('paused');
        this.elements.status.textContent = parts.join(' · ');
    }

    /** @param {boolean} paused */
    setPaused(paused) {
        this.paused = paused;
        this.updateStatus();
    }

    /**
     * @param {'in' | 'out' | 'clock'} name
     * @param {boolean} on
     */
    setFilter(name, on) {
        this.filters[name] = on;
    }

    clear() {
        this.pending = [];
        this.elements.list.replaceChildren();
        this.counts = { in: 0, out: 0, clock: 0 };
        this.clockTimes = [];
        this.startTime = 0;
        if (this.device) this.device.base = { in: this.device.in, out: this.device.out };
        this.updateStatus();
    }
}

/**
 * "^^midic(12,3,1200)" -> [12, 3, 1200]; null for other lines.
 * @param {string} line
 * @returns {[number, number, number] | null}
 */
export function parseMidiCountsLine(line) {
    const match = line.match(/^\^\^midic\((\d+),(\d+),(\d+)\)/);
    return match ? [Number(match[1]), Number(match[2]), Number(match[3])] : null;
}

/**
 * "^^midi(i,90,3c,64)" -> ["i", 0x90, 0x3c, 0x64]; null for other lines.
 * @param {string} line
 * @returns {['i' | 'o', number, number, number] | null}
 */
export function parseMidiLine(line) {
    const match = line.match(/^\^\^midi\(([io]),([0-9a-f]{2}),([0-9a-f]{2}),([0-9a-f]{2})\)/i);
    if (!match) return null;
    return [
        /** @type {'i' | 'o'} */ (match[1]),
        parseInt(match[2], 16),
        parseInt(match[3], 16),
        parseInt(match[4], 16)
    ];
}
