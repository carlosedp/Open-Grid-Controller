// @ts-check
// Changing which script init.lua starts, without touching anything else in
// it. iii's first() rewrites init.lua as a single fs_run_file() line, which
// would throw away whatever else a user put there.

import { luaQuote } from './backup.js';

/** @param {string} value */
const escapeRegExp = (value) => value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

/**
 * fs_run_file('name') / fs_run_file("name"); any name when none is given.
 * @param {string} [name]
 */
const callPattern = (name) =>
    new RegExp(`fs_run_file\\s*\\(\\s*(['"])${name ? escapeRegExp(name) : `[^'"]+`}\\1\\s*\\)`, 'g');

/**
 * Whether text[index] is inside a `--` comment on its line.
 * @param {string} text
 * @param {number} index
 */
function inLineComment(text, index) {
    const lineStart = text.lastIndexOf('\n', index - 1) + 1;
    return text.slice(lineStart, index).includes('--');
}

/**
 * The code without comments and surrounding whitespace.
 * @param {string} text
 */
function codeOnly(text) {
    return text
        .replace(/--\[\[[\s\S]*?\]\]/g, '')
        .replace(/--.*$/gm, '')
        .trim();
}

/**
 * True when init.lua does nothing but start one script (what first() writes),
 * so replacing or removing it loses nothing.
 * @param {string} text
 */
export function onlyStartsScript(text) {
    return /^fs_run_file\s*\(\s*(['"])[^'"]+\1\s*\)\s*;?$/.test(codeOnly(text));
}

/**
 * Comments out the calls that start `name`; the rest stays as it was.
 * @param {string} text
 * @param {string} name
 * @returns {{ text: string, changed: boolean }}
 */
export function disableStartupScript(text, name) {
    let changed = false;
    const out = text.replace(callPattern(name), (match, _quote, offset) => {
        if (inLineComment(text, offset)) return match;
        changed = true;
        return `--[[ disabled by diii: ${match} ]]`;
    });
    return { text: out, changed };
}

/**
 * Makes init.lua start `name`: replaces the first active fs_run_file() call,
 * or adds one at the end.
 * @param {string} text
 * @param {string} name
 * @returns {string}
 */
export function setStartupScript(text, name) {
    const call = `fs_run_file(${luaQuote(name)})`;
    let replaced = false;
    const out = text.replace(callPattern(), (match, _quote, offset) => {
        if (replaced || inLineComment(text, offset)) return match;
        replaced = true;
        return call;
    });
    if (replaced) return out;
    return `${text.replace(/\s*$/, '')}\n${call}\n`;
}
