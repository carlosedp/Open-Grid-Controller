// @ts-check
// iii's Lua API, for the editor's autocomplete and hover help and the offline
// reference. Wording follows monome.org/docs/iii/code; functions the page
// doesn't cover (fs_*, midi_tx, device_reset, og_*) are described from the
// firmware. `device` limits an entry to grids, arcs or Open Grid firmware.

/**
 * @typedef {object} ApiEntry
 * @property {string} name what is typed, e.g. "grid_led" or "metro.init"
 * @property {string} sig how it's called, e.g. "grid_led(x, y, z, rel)"
 * @property {string} doc one or two sentences
 * @property {string} group section in the reference
 * @property {'function' | 'event' | 'method' | 'property'} kind
 * @property {'grid' | 'arc' | 'og'} [device]
 */

/** @type {ApiEntry[]} */
export const III_API = [
    // grid
    {
        group: 'grid',
        kind: 'event',
        device: 'grid',
        name: 'event_grid',
        sig: 'event_grid(x, y, z)',
        doc: 'Define this function to handle key presses: x, y are the key (1-based), z is 1 on press and 0 on release.'
    },
    {
        group: 'grid',
        kind: 'function',
        device: 'grid',
        name: 'grid_led',
        sig: 'grid_led(x, y, z, rel)',
        doc: 'Sets the LED at x, y to level z (0-15). With rel true, adds z to the current level.'
    },
    {
        group: 'grid',
        kind: 'function',
        device: 'grid',
        name: 'grid_led_get',
        sig: 'grid_led_get(x, y)',
        doc: 'Returns the level of the LED at x, y.'
    },
    {
        group: 'grid',
        kind: 'function',
        device: 'grid',
        name: 'grid_led_all',
        sig: 'grid_led_all(z, rel)',
        doc: 'Sets all LEDs to level z. With rel true, adds z to each.'
    },
    {
        group: 'grid',
        kind: 'function',
        device: 'grid',
        name: 'grid_intensity',
        sig: 'grid_intensity(b)',
        doc: 'Sets the global brightness to b and refreshes.'
    },
    {
        group: 'grid',
        kind: 'function',
        device: 'grid',
        name: 'grid_refresh',
        sig: 'grid_refresh()',
        doc: 'Sends the LED levels set since the last refresh to the grid.'
    },
    {
        group: 'grid',
        kind: 'function',
        device: 'grid',
        name: 'grid_size_x',
        sig: 'grid_size_x()',
        doc: 'Returns the number of columns.'
    },
    {
        group: 'grid',
        kind: 'function',
        device: 'grid',
        name: 'grid_size_y',
        sig: 'grid_size_y()',
        doc: 'Returns the number of rows.'
    },

    // arc
    {
        group: 'arc',
        kind: 'event',
        device: 'arc',
        name: 'event_arc',
        sig: 'event_arc(n, d)',
        doc: 'Define this function to handle turns: ring n moved by delta d.'
    },
    {
        group: 'arc',
        kind: 'event',
        device: 'arc',
        name: 'event_arc_key',
        sig: 'event_arc_key(n, z)',
        doc: 'Define this function to handle the arc key: z is 1 on press and 0 on release.'
    },
    {
        group: 'arc',
        kind: 'function',
        device: 'arc',
        name: 'arc_res',
        sig: 'arc_res(n, div)',
        doc: 'Sets the resolution of ring n; 1 is the default, higher values give coarser steps.'
    },
    {
        group: 'arc',
        kind: 'function',
        device: 'arc',
        name: 'arc_led',
        sig: 'arc_led(n, x, z, rel)',
        doc: 'Sets LED x of ring n to level z. With rel true, adds z to the current level.'
    },
    {
        group: 'arc',
        kind: 'function',
        device: 'arc',
        name: 'arc_led_ring',
        sig: 'arc_led_ring(n, z, rel)',
        doc: 'Sets all LEDs of ring n to level z. With rel true, adds z to each.'
    },
    {
        group: 'arc',
        kind: 'function',
        device: 'arc',
        name: 'arc_led_all',
        sig: 'arc_led_all(z, rel)',
        doc: 'Sets all LEDs to level z. With rel true, adds z to each.'
    },
    {
        group: 'arc',
        kind: 'function',
        device: 'arc',
        name: 'arc_intensity',
        sig: 'arc_intensity(b)',
        doc: 'Sets the global brightness to b and refreshes.'
    },
    {
        group: 'arc',
        kind: 'function',
        device: 'arc',
        name: 'arc_refresh',
        sig: 'arc_refresh()',
        doc: 'Sends the LED levels set since the last refresh to the arc.'
    },

    // midi
    {
        group: 'midi',
        kind: 'event',
        name: 'event_midi',
        sig: 'event_midi(byte1, byte2, byte3)',
        doc: 'Define this function to handle incoming USB MIDI; midi_to_msg() turns the bytes into a table.'
    },
    {
        group: 'midi',
        kind: 'function',
        name: 'midi_to_msg',
        sig: 'midi_to_msg(data)',
        doc: 'Decodes a table of MIDI bytes into a labeled table, e.g. { type = "note_on", note = 60, vel = 100, ch = 1 }.'
    },
    {
        group: 'midi',
        kind: 'function',
        name: 'midi_out',
        sig: 'midi_out(msg)',
        doc: 'Sends a table to the USB MIDI port: raw bytes, or a message table like the ones midi_to_msg() returns.'
    },
    {
        group: 'midi',
        kind: 'function',
        name: 'midi_note_on',
        sig: 'midi_note_on(note, vel, ch)',
        doc: 'Sends a note on.'
    },
    {
        group: 'midi',
        kind: 'function',
        name: 'midi_note_off',
        sig: 'midi_note_off(note, vel, ch)',
        doc: 'Sends a note off.'
    },
    { group: 'midi', kind: 'function', name: 'midi_cc', sig: 'midi_cc(cc, val, ch)', doc: 'Sends a control change.' },
    {
        group: 'midi',
        kind: 'function',
        name: 'midi_tx',
        sig: 'midi_tx(byte1, byte2, byte3)',
        doc: 'Sends three raw MIDI bytes.'
    },

    // metro
    {
        group: 'metro',
        kind: 'function',
        name: 'metro.init',
        sig: 'm = metro.init(callback, time_sec, count)',
        doc: 'Creates a metro that calls callback every time_sec seconds, count times (optional; forever without it). Start it with m:start().'
    },
    {
        group: 'metro',
        kind: 'method',
        name: 'start',
        sig: 'm:start(time, count)',
        doc: 'Starts the metro, optionally with a new time and count. Call again to restart.'
    },
    { group: 'metro', kind: 'method', name: 'stop', sig: 'm:stop()', doc: 'Stops the metro.' },
    {
        group: 'metro',
        kind: 'property',
        name: 'time',
        sig: 'm.time = 0.1',
        doc: "Changes the metro's interval in seconds."
    },
    { group: 'metro', kind: 'function', name: 'metro.free', sig: 'metro.free(m.id)', doc: 'Frees the metro.' },
    { group: 'metro', kind: 'function', name: 'metro.free_all', sig: 'metro.free_all()', doc: 'Frees all metros.' },

    // slew
    {
        group: 'slew',
        kind: 'function',
        name: 'slew.new',
        sig: 'id = slew.new(callback, start_val, end_val, time_sec, q)',
        doc: 'Moves from start_val to end_val over time_sec, calling callback(value) in steps of q (default 1). Returns an id.'
    },
    {
        group: 'slew',
        kind: 'function',
        name: 'slew.to',
        sig: 'slew.to(id, end_val, time)',
        doc: 'Sends a running slew to a new destination from where it is now, optionally with a new time.'
    },
    {
        group: 'slew',
        kind: 'function',
        name: 'slew.freeze',
        sig: 'slew.freeze(id)',
        doc: 'Pauses the slew; slew.to() resumes it.'
    },
    { group: 'slew', kind: 'function', name: 'slew.allfreeze', sig: 'slew.allfreeze()', doc: 'Pauses all slews.' },
    { group: 'slew', kind: 'function', name: 'slew.stop', sig: 'slew.stop(id)', doc: 'Stops and removes the slew.' },
    {
        group: 'slew',
        kind: 'function',
        name: 'slew.allstop',
        sig: 'slew.allstop()',
        doc: 'Stops and removes all slews.'
    },

    // pset
    {
        group: 'pset',
        kind: 'function',
        name: 'pset_init',
        sig: 'pset_init(name)',
        doc: 'Sets the name of the preset files read and written below (pset_<name>_<n>.lua).'
    },
    {
        group: 'pset',
        kind: 'function',
        name: 'pset_write',
        sig: 'pset_write(index, table)',
        doc: 'Saves table as preset number index.'
    },
    {
        group: 'pset',
        kind: 'function',
        name: 'pset_read',
        sig: 'table = pset_read(index)',
        doc: 'Returns preset number index, or nil if there is none.'
    },
    {
        group: 'pset',
        kind: 'function',
        name: 'pset_delete',
        sig: 'pset_delete(index)',
        doc: 'Deletes preset number index.'
    },

    // utils
    {
        group: 'utils',
        kind: 'function',
        name: 'dostring',
        sig: 'dostring(lua_command)',
        doc: 'Runs a string as Lua code.'
    },
    {
        group: 'utils',
        kind: 'function',
        name: 'get_time',
        sig: 'get_time()',
        doc: 'Returns the time in seconds, with microsecond precision.'
    },
    {
        group: 'utils',
        kind: 'function',
        name: 'ps',
        sig: 'ps(formatted_string, ...)',
        doc: 'Prints a formatted string, like printf (string.format).'
    },
    { group: 'utils', kind: 'function', name: 'pt', sig: 'pt(table)', doc: "Prints a table's keys and values." },
    {
        group: 'utils',
        kind: 'function',
        name: 'clamp',
        sig: 'clamp(n, min, max)',
        doc: 'Returns n limited to min..max.'
    },
    {
        group: 'utils',
        kind: 'function',
        name: 'round',
        sig: 'round(n, quant)',
        doc: 'Returns n rounded to the nearest multiple of quant (default 1).'
    },
    {
        group: 'utils',
        kind: 'function',
        name: 'linlin',
        sig: 'linlin(n, slo, shi, dlo, dhi)',
        doc: 'Maps n from the range slo..shi to dlo..dhi.'
    },
    {
        group: 'utils',
        kind: 'function',
        name: 'wrap',
        sig: 'wrap(n, min, max)',
        doc: 'Returns n wrapped into min..max.'
    },

    // system
    { group: 'system', kind: 'function', name: 'help', sig: 'help()', doc: 'Prints the API summary.' },
    {
        group: 'system',
        kind: 'function',
        name: 'device_id',
        sig: 'device_id()',
        doc: 'Returns the device name, e.g. "grid".'
    },
    {
        group: 'system',
        kind: 'function',
        name: 'device_version',
        sig: 'device_version()',
        doc: 'Returns the firmware version.'
    },
    { group: 'system', kind: 'function', name: 'device_reset', sig: 'device_reset()', doc: 'Reboots the device.' },
    { group: 'system', kind: 'function', name: 'ls', sig: 'ls()', doc: 'Lists the files.' },
    { group: 'system', kind: 'function', name: 'cat', sig: 'cat(file)', doc: 'Prints a file.' },
    { group: 'system', kind: 'function', name: 'rm', sig: 'rm(file)', doc: 'Deletes a file.' },
    { group: 'system', kind: 'function', name: 'mem', sig: 'mem()', doc: 'Prints the memory Lua uses, in KB.' },
    { group: 'system', kind: 'function', name: 'gc', sig: 'gc()', doc: 'Runs a full garbage collection, then mem().' },
    {
        group: 'system',
        kind: 'function',
        name: 'require',
        sig: 'require(file)',
        doc: 'Runs a file and returns what it returns.'
    },
    {
        group: 'system',
        kind: 'function',
        name: 'first',
        sig: 'first(file)',
        doc: 'Makes file run at startup (writes init.lua); first() with no file removes init.lua.'
    },

    // files (the C functions behind ls/cat/rm/require)
    {
        group: 'files',
        kind: 'function',
        name: 'fs_list_files',
        sig: 'fs_list_files()',
        doc: 'Returns a table of file names.'
    },
    {
        group: 'files',
        kind: 'function',
        name: 'fs_read_file',
        sig: 'fs_read_file(name)',
        doc: "Returns a file's contents as a string, or false."
    },
    {
        group: 'files',
        kind: 'function',
        name: 'fs_write_file',
        sig: 'fs_write_file(name, text)',
        doc: 'Writes text to a file, replacing it.'
    },
    {
        group: 'files',
        kind: 'function',
        name: 'fs_run_file',
        sig: 'fs_run_file(name)',
        doc: 'Runs a Lua file and returns its results.'
    },
    { group: 'files', kind: 'function', name: 'fs_remove_file', sig: 'fs_remove_file(name)', doc: 'Deletes a file.' },
    {
        group: 'files',
        kind: 'function',
        name: 'fs_file_size',
        sig: 'fs_file_size(name)',
        doc: "Returns a file's size in bytes."
    },
    {
        group: 'files',
        kind: 'function',
        name: 'fs_free_space',
        sig: 'fs_free_space()',
        doc: 'Returns the free space in bytes.'
    },
    { group: 'files', kind: 'function', name: 'fs_reformat', sig: 'fs_reformat()', doc: 'Erases every file.' },

    // Open Grid firmware
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_set_mode',
        sig: 'og_set_mode(m)',
        doc: '0 = iii, 1 = monome serial (norns, serialosc). Saves the mode and reboots.'
    },
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_keypads',
        sig: 'og_keypads()',
        doc: 'Returns a bitmask of the keypad boards that answer.'
    },
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_mem',
        sig: 'og_mem(quiet)',
        doc: 'Prints the C heap in KB and returns free, largest, used, peak, total, high_water (bytes); og_mem(true) only returns them.'
    },
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_key',
        sig: 'og_key(x, y, z)',
        doc: 'Injects a key event as if pressed on the grid (1-based).'
    },
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_led',
        sig: 'og_led(r, g, b)',
        doc: 'Sets the status LED color (0-255 each); og_led() returns it to the mode color.'
    },
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_mirror',
        sig: 'og_mirror(on)',
        doc: "Streams LED frames as ^^grid(...) lines (diii's virtual grid)."
    },
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_dump_file',
        sig: 'og_dump_file(name)',
        doc: 'Prints a file as base64 lines, then its size and CRC-32 (used by backups).'
    },
    {
        group: 'open grid',
        kind: 'function',
        device: 'og',
        name: 'og_rename_file',
        sig: 'og_rename_file(old, new)',
        doc: 'Renames a file in one step, replacing new if it exists.'
    }
];

/** Lua names offered without docs (keywords and the standard library). */
export const LUA_KEYWORDS = [
    'and',
    'break',
    'do',
    'else',
    'elseif',
    'end',
    'false',
    'for',
    'function',
    'goto',
    'if',
    'in',
    'local',
    'nil',
    'not',
    'or',
    'repeat',
    'return',
    'then',
    'true',
    'until',
    'while'
];

export const LUA_GLOBALS = [
    'print',
    'pairs',
    'ipairs',
    'next',
    'select',
    'type',
    'tostring',
    'tonumber',
    'pcall',
    'error',
    'assert',
    'setmetatable',
    'getmetatable',
    'rawget',
    'rawset',
    'collectgarbage',
    'string.format',
    'string.sub',
    'string.len',
    'string.rep',
    'string.byte',
    'string.char',
    'string.find',
    'string.match',
    'string.gmatch',
    'string.gsub',
    'string.lower',
    'string.upper',
    'table.insert',
    'table.remove',
    'table.concat',
    'table.sort',
    'table.unpack',
    'math.floor',
    'math.ceil',
    'math.abs',
    'math.min',
    'math.max',
    'math.random',
    'math.randomseed',
    'math.sqrt',
    'math.fmod',
    'math.huge',
    'math.pi',
    'math.sin',
    'math.cos',
    'math.tointeger'
];

/**
 * The entries that apply to a device.
 * @param {{ deviceId?: string | null, openGrid?: boolean }} device
 * @returns {ApiEntry[]}
 */
export function apiFor({ deviceId = null, openGrid = false } = {}) {
    return III_API.filter((entry) => {
        if (entry.device === 'og') return openGrid;
        if (entry.device && deviceId && entry.device !== deviceId) return false;
        return true;
    });
}

/**
 * Parameter names of a signature: "grid_led(x, y, z, rel)" -> ["x", "y", "z", "rel"].
 * @param {string} sig
 * @returns {string[]}
 */
export function paramsOf(sig) {
    const match = sig.match(/\(([^)]*)\)/);
    if (!match || !match[1].trim()) return [];
    return match[1]
        .split(',')
        .map((p) => p.trim())
        .filter((p) => p && p !== '...');
}
