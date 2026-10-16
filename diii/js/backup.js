// @ts-check
// Backup and restore of every file on the device, as one zip.
//
// Files are read byte for byte on any iii firmware: with og_dump_file() (Open
// Grid) as base64 lines plus "size crc32"; elsewhere a few KB at a time, each
// piece turned into hex by Lua on the device (readFileInChunks). cat() lost
// blank lines (diii drops empty output lines) and, on stock iii, everything
// past its 16 KB output buffer.
// Restore uploads each file through the normal upload path, so every file is
// compile-checked on the device before it replaces the old one.

import { createZip, crc32, readZip } from './zip.js';

/**
 * What backup and restore need from the app.
 * @typedef {object} DeviceApi
 * @property {(lua: string) => Promise<string[]>} capture  run Lua, return what it printed
 * @property {(name: string, text: string) => Promise<void>} upload  write a file through ^^s/^^w
 * @property {(line: string) => void} log  write a line to the console
 */

/** Files the firmware manages itself; restoring an old copy would only get in the way. */
const SKIP_ON_RESTORE = new Set(['lib.lua']);

/**
 * @param {string} value
 * @returns {string} a Lua string literal
 */
export function luaQuote(value) {
    return `'${String(value).replace(/\\/g, '\\\\').replace(/'/g, "\\'").replace(/\r/g, '\\r').replace(/\n/g, '\\n')}'`;
}

/**
 * Decodes og_dump_file() output and checks its size and CRC.
 * @param {string} name
 * @param {string[]} lines base64 lines, then "size crc32"
 * @returns {Uint8Array}
 */
export function decodeDump(name, lines) {
    const trailer = String(lines[lines.length - 1] || '').trim();
    const match = trailer.match(/^(\d+) ([0-9a-f]{8})$/i);
    if (!match) throw new Error(`${name}: incomplete transfer`);

    const binary = atob(lines.slice(0, -1).join(''));
    const data = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) data[i] = binary.charCodeAt(i);

    if (data.length !== Number(match[1]) || crc32(data) !== parseInt(match[2], 16)) {
        throw new Error(`${name}: transfer damaged (size or checksum mismatch)`);
    }
    return data;
}

/**
 * Whether the device has og_dump_file() (Open Grid firmware), i.e. exact file
 * reads, and uploads that keep the old file when the new one doesn't compile.
 * @param {DeviceApi} device
 * @returns {Promise<boolean>}
 */
export async function hasExactFiles(device) {
    const probe = await device.capture('print(og_dump_file ~= nil)');
    return probe.some((line) => line.trim() === 'true');
}

// Bytes per piece for readFileInChunks: twice that in hex is printed per
// command, well inside stock iii's 16 KB output buffer (which only empties
// between commands).
const CHUNK_BYTES = 3072;

/**
 * Reads a file on firmware without og_dump_file(): its size, then one command
 * per CHUNK_BYTES that prints that piece as hex (Lua encodes it on the device,
 * so blank lines, '\r' and '\0' come through), each checked for length.
 * Slower than og_dump_file (Lua reads the whole file for every piece), but
 * exact, and works for any size.
 * @param {DeviceApi} device
 * @param {string} name
 * @returns {Promise<Uint8Array>}
 */
export async function readFileInChunks(device, name) {
    const quoted = luaQuote(name);
    const size = Number.parseInt((await device.capture(`print(fs_file_size(${quoted}) or -1)`))[0], 10);
    if (!Number.isFinite(size) || size < 0) throw new Error(`${name}: not found`);

    const data = new Uint8Array(size);
    for (let start = 0; start < size; start += CHUNK_BYTES) {
        const expected = Math.min(CHUNK_BYTES, size - start);
        const lines = await device.capture(
            `do local s = fs_read_file(${quoted}) if s then print((s:sub(${start + 1}, ${start + expected}):gsub(".", function(c) return string.format("%02x", c:byte()) end))) end end`
        );
        const hex = lines.join('').trim();
        if (hex.length !== expected * 2 || /[^0-9a-f]/i.test(hex)) {
            throw new Error(`${name}: transfer damaged at byte ${start}`);
        }
        for (let i = 0; i < expected; i++) data[start + i] = Number.parseInt(hex.substr(i * 2, 2), 16);
    }
    return data;
}

/**
 * Reads one file exactly: og_dump_file() when there is one, else in chunks.
 * @param {DeviceApi} device
 * @param {string} name
 * @param {boolean} exact result of hasExactFiles() (og_dump_file available)
 * @returns {Promise<Uint8Array>}
 */
export async function readDeviceFile(device, name, exact) {
    if (exact) return decodeDump(name, await device.capture(`og_dump_file(${luaQuote(name)})`));
    return readFileInChunks(device, name);
}

/**
 * Reads every file on the device into a zip.
 * @param {DeviceApi} device
 * @returns {Promise<{ zip: Uint8Array, count: number, exact: boolean }>}
 */
export async function backupDevice(device) {
    const exact = await hasExactFiles(device);
    if (!exact) device.log('backup: this firmware has no og_dump_file(); reading in pieces (slower)');

    const names = (await device.capture('for _, n in ipairs(fs_list_files()) do print(n) end'))
        .map((line) => line.trim())
        .filter((name) => name && !name.startsWith('.'));

    /** @type {import('./zip.js').ZipEntry[]} */
    const entries = [];
    for (const name of names) {
        device.log(`backup: ${name}`);
        entries.push({ name, data: await readDeviceFile(device, name, exact) });
    }

    return { zip: createZip(entries), count: entries.length, exact };
}

/**
 * Lists what a backup would restore.
 * @param {Uint8Array} zipBytes
 * @returns {Promise<{ restore: { name: string, text: string }[], skipped: string[] }>}
 */
export async function readBackup(zipBytes) {
    const decoder = new TextDecoder();
    /** @type {{ name: string, text: string }[]} */
    const restore = [];
    /** @type {string[]} */
    const skipped = [];

    for (const entry of await readZip(zipBytes)) {
        const name = entry.name;
        if (name.includes('/') || name.startsWith('.') || SKIP_ON_RESTORE.has(name)) {
            skipped.push(name);
            continue;
        }
        // uploads add the trailing '\0' back themselves
        restore.push({ name, text: decoder.decode(entry.data).replace(/\0+$/, '') });
    }
    return { restore, skipped };
}

/**
 * Uploads the files of a backup.
 * @param {DeviceApi} device
 * @param {{ name: string, text: string }[]} files
 * @returns {Promise<void>}
 */
export async function restoreFiles(device, files) {
    for (const file of files) {
        device.log(`restore: ${file.name}`);
        await device.upload(file.name, file.text);
    }
}
