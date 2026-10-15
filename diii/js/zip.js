// @ts-check
// Minimal zip files for backups: writes uncompressed ("stored") archives and
// reads stored or deflated ones (deflate through the browser's
// DecompressionStream), so a backup re-zipped by another tool still restores.

/** @typedef {{ name: string, data: Uint8Array, date?: Date }} ZipEntry */

const CRC_TABLE = (() => {
    const table = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
        table[n] = c >>> 0;
    }
    return table;
})();

/**
 * CRC-32 as used by zip (and og_dump_file).
 * @param {Uint8Array} data
 * @returns {number}
 */
export function crc32(data) {
    let crc = 0xffffffff;
    for (let i = 0; i < data.length; i++) crc = CRC_TABLE[(crc ^ data[i]) & 0xff] ^ (crc >>> 8);
    return (crc ^ 0xffffffff) >>> 0;
}

/**
 * @param {Date} date
 * @returns {{ time: number, day: number }} MS-DOS time and date fields
 */
function dosDateTime(date) {
    const year = Math.max(1980, date.getFullYear());
    return {
        time: (date.getHours() << 11) | (date.getMinutes() << 5) | (date.getSeconds() >> 1),
        day: ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate()
    };
}

/**
 * Builds an uncompressed zip archive.
 * @param {ZipEntry[]} entries
 * @returns {Uint8Array}
 */
export function createZip(entries) {
    const encoder = new TextEncoder();
    /** @type {Uint8Array[]} */
    const parts = [];
    /** @type {Uint8Array[]} */
    const central = [];
    let offset = 0;

    for (const entry of entries) {
        const name = encoder.encode(entry.name);
        const crc = crc32(entry.data);
        const { time, day } = dosDateTime(entry.date || new Date());

        const local = new DataView(new ArrayBuffer(30));
        local.setUint32(0, 0x04034b50, true); // local file header
        local.setUint16(4, 20, true); // version needed
        local.setUint16(6, 0x0800, true); // flags: UTF-8 names
        local.setUint16(8, 0, true); // method: stored
        local.setUint16(10, time, true);
        local.setUint16(12, day, true);
        local.setUint32(14, crc, true);
        local.setUint32(18, entry.data.length, true);
        local.setUint32(22, entry.data.length, true);
        local.setUint16(26, name.length, true);
        local.setUint16(28, 0, true);
        parts.push(new Uint8Array(local.buffer), name, entry.data);

        const dir = new DataView(new ArrayBuffer(46));
        dir.setUint32(0, 0x02014b50, true); // central directory header
        dir.setUint16(4, 20, true); // version made by
        dir.setUint16(6, 20, true);
        dir.setUint16(8, 0x0800, true);
        dir.setUint16(10, 0, true);
        dir.setUint16(12, time, true);
        dir.setUint16(14, day, true);
        dir.setUint32(16, crc, true);
        dir.setUint32(20, entry.data.length, true);
        dir.setUint32(24, entry.data.length, true);
        dir.setUint16(28, name.length, true);
        dir.setUint32(42, offset, true); // local header offset
        central.push(new Uint8Array(dir.buffer), name);

        offset += 30 + name.length + entry.data.length;
    }

    const centralSize = central.reduce((sum, part) => sum + part.length, 0);
    const end = new DataView(new ArrayBuffer(22));
    end.setUint32(0, 0x06054b50, true); // end of central directory
    end.setUint16(8, entries.length, true);
    end.setUint16(10, entries.length, true);
    end.setUint32(12, centralSize, true);
    end.setUint32(16, offset, true);

    return concat([...parts, ...central, new Uint8Array(end.buffer)]);
}

/**
 * Reads the files of a zip archive (stored or deflated) and checks their CRCs.
 * @param {Uint8Array} bytes
 * @returns {Promise<ZipEntry[]>}
 */
export async function readZip(bytes) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    let endPos = -1;
    for (let i = bytes.length - 22; i >= Math.max(0, bytes.length - 22 - 0xffff); i--) {
        if (view.getUint32(i, true) === 0x06054b50) {
            endPos = i;
            break;
        }
    }
    if (endPos < 0) throw new Error('not a zip file');

    const count = view.getUint16(endPos + 10, true);
    let pos = view.getUint32(endPos + 16, true);
    const decoder = new TextDecoder();
    /** @type {ZipEntry[]} */
    const entries = [];

    for (let i = 0; i < count; i++) {
        if (view.getUint32(pos, true) !== 0x02014b50) throw new Error('damaged zip directory');
        const method = view.getUint16(pos + 10, true);
        const crc = view.getUint32(pos + 16, true);
        const compressedSize = view.getUint32(pos + 20, true);
        const nameLength = view.getUint16(pos + 28, true);
        const extraLength = view.getUint16(pos + 30, true);
        const commentLength = view.getUint16(pos + 32, true);
        const localOffset = view.getUint32(pos + 42, true);
        const name = decoder.decode(bytes.subarray(pos + 46, pos + 46 + nameLength));
        pos += 46 + nameLength + extraLength + commentLength;

        if (name.endsWith('/')) continue; // folder

        const dataStart =
            localOffset + 30 + view.getUint16(localOffset + 26, true) + view.getUint16(localOffset + 28, true);
        const raw = bytes.subarray(dataStart, dataStart + compressedSize);

        let data;
        if (method === 0) data = raw;
        else if (method === 8) data = await inflateRaw(raw);
        else throw new Error(`${name}: unsupported zip compression (${method})`);

        if (crc32(data) !== crc) throw new Error(`${name}: checksum mismatch`);
        entries.push({ name, data });
    }
    return entries;
}

/**
 * @param {Uint8Array} data
 * @returns {Promise<Uint8Array>}
 */
async function inflateRaw(data) {
    const stream = new Blob([data.slice()]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
    return new Uint8Array(await new Response(stream).arrayBuffer());
}

/**
 * @param {Uint8Array[]} parts
 * @returns {Uint8Array}
 */
function concat(parts) {
    const out = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
    let offset = 0;
    for (const part of parts) {
        out.set(part, offset);
        offset += part.length;
    }
    return out;
}
