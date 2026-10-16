// @ts-check
// Turns a 3-byte USB MIDI message into words for the MIDI monitor.

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/** Controller names worth showing; others are just "cc N". */
const CC_NAMES = /** @type {Record<number, string>} */ ({
    0: 'bank select',
    1: 'mod wheel',
    2: 'breath',
    4: 'foot',
    5: 'portamento time',
    7: 'volume',
    10: 'pan',
    11: 'expression',
    32: 'bank select lsb',
    64: 'sustain',
    65: 'portamento',
    66: 'sostenuto',
    67: 'soft pedal',
    120: 'all sound off',
    121: 'reset controllers',
    123: 'all notes off'
});

/**
 * Note number -> name, middle C (60) = C4.
 * @param {number} note
 */
export function noteName(note) {
    return `${NOTE_NAMES[note % 12]}${Math.floor(note / 12) - 1}`;
}

/**
 * @typedef {object} MidiMessage
 * @property {string} kind e.g. "note_on", "cc", "clock"
 * @property {number | null} ch channel 1-16, null for system messages
 * @property {string} text what happened, e.g. "note on C4 (60) vel 100"
 * @property {boolean} realtime clock and the like: frequent, hidden by default
 */

/**
 * @param {number} b1 status byte (or a sysex data byte)
 * @param {number} b2
 * @param {number} b3
 * @returns {MidiMessage}
 */
export function decodeMidi(b1, b2, b3) {
    const ch = (b1 & 0x0f) + 1;
    /** @param {string} kind @param {string} text @param {number | null} [channel] */
    const msg = (kind, text, channel = ch) => ({ kind, ch: channel, text, realtime: false });
    /** @param {string} kind @param {string} text */
    const system = (kind, text) => ({ kind, ch: null, text, realtime: kind === 'clock' || kind === 'active_sensing' });

    switch (b1 & 0xf0) {
        case 0x90:
            return b3 === 0
                ? msg('note_off', `note off ${noteName(b2)} (${b2}) (vel 0)`)
                : msg('note_on', `note on ${noteName(b2)} (${b2}) vel ${b3}`);
        case 0x80:
            return msg('note_off', `note off ${noteName(b2)} (${b2}) vel ${b3}`);
        case 0xb0: {
            const name = CC_NAMES[b2];
            return msg('cc', `cc ${b2}${name ? ` ${name}` : ''} = ${b3}`);
        }
        case 0xc0:
            return msg('program', `program ${b2 + 1}`);
        case 0xd0:
            return msg('channel_pressure', `pressure ${b2}`);
        case 0xa0:
            return msg('key_pressure', `key pressure ${noteName(b2)} (${b2}) = ${b3}`);
        case 0xe0:
            return msg('pitchbend', `pitchbend ${((b3 << 7) | b2) - 8192}`);
        default:
            break;
    }

    switch (b1) {
        case 0xf8:
            return system('clock', 'clock');
        case 0xfa:
            return system('start', 'start');
        case 0xfb:
            return system('continue', 'continue');
        case 0xfc:
            return system('stop', 'stop');
        case 0xfe:
            return system('active_sensing', 'active sensing');
        case 0xff:
            return system('reset', 'reset');
        case 0xf2:
            return system('song_position', `song position ${(b3 << 7) | b2}`);
        case 0xf3:
            return system('song_select', `song select ${b2}`);
        case 0xf1:
            return system('mtc', `time code ${b2}`);
        case 0xf6:
            return system('tune', 'tune request');
        case 0xf0:
        case 0xf7:
            return system('sysex', 'sysex');
        default:
            // sysex bytes after the first packet carry no status byte
            return system('sysex', 'sysex (data)');
    }
}
