// @ts-check
// Lua autocomplete and hover help for the editor, from iii's API list
// (iii-api.js), Lua's keywords and library, and the words already in the file.

import { LUA_GLOBALS, LUA_KEYWORDS, paramsOf } from './iii-api.js';

/** @typedef {import('./iii-api.js').ApiEntry} ApiEntry */

/**
 * What the completion popup shows for one option (prism-code-editor's Completion).
 * @typedef {object} Completion
 * @property {string} label
 * @property {string} [detail]
 * @property {string} [icon]
 * @property {string} [insert]
 * @property {number[]} [tabStops]
 * @property {number} [boost]
 * @property {() => (Node | string)[]} [renderDocs]
 */

/**
 * Inserting a function fills in its parameters as tab stops (Tab moves on);
 * an event inserts the whole function definition.
 * @param {ApiEntry} entry
 * @returns {Pick<Completion, 'insert' | 'tabStops'>}
 */
export function snippetFor(entry) {
    const params = paramsOf(entry.sig);
    if (entry.kind === 'event') {
        const head = `function ${entry.name}(${params.join(', ')})\n  `;
        return { insert: `${head}\nend`, tabStops: [head.length] };
    }
    if (entry.kind !== 'function' && entry.kind !== 'method') return {};
    if (!params.length) return { insert: `${entry.name}()`, tabStops: [entry.name.length + 2] };

    /** @type {number[]} */
    const tabStops = [];
    let text = `${entry.name}(`;
    params.forEach((param, i) => {
        if (i) text += ', ';
        tabStops.push(text.length, text.length + param.length);
        text += param;
    });
    return { insert: `${text})`, tabStops };
}

/**
 * The tooltip contents for an entry: signature, then description.
 * @param {ApiEntry} entry
 * @returns {Node[]}
 */
export function renderEntryDocs(entry) {
    const sig = document.createElement('code');
    sig.className = 'api-sig';
    sig.textContent = entry.sig;
    const doc = document.createElement('div');
    doc.className = 'api-doc';
    doc.textContent = entry.doc;
    return [sig, doc];
}

/**
 * Whether the cursor is in a comment or a string on its line (no completion there).
 * @param {string} lineBefore
 */
function inCommentOrString(lineBefore) {
    let quote = '';
    for (let i = 0; i < lineBefore.length; i++) {
        const c = lineBefore[i];
        if (quote) {
            if (c === '\\') i++;
            else if (c === quote) quote = '';
        } else if (c === '"' || c === "'") quote = c;
        else if (c === '-' && lineBefore[i + 1] === '-') return true;
    }
    return Boolean(quote);
}

/**
 * Options for what's being typed at the end of `lineBefore`.
 * @param {ApiEntry[]} api
 * @param {string} lineBefore
 * @param {string} documentText words in the file are offered too
 * @param {boolean} explicit Ctrl+Space: offer options even before anything is typed
 * @returns {{ query: string, options: Completion[] } | null}
 */
export function completionsAt(api, lineBefore, documentText, explicit) {
    if (inCommentOrString(lineBefore)) return null;
    const query = (lineBefore.match(/(?:[A-Za-z_]\w*[.:])?[A-Za-z_]?\w*$/) || [''])[0];
    if (!query && !explicit) return null;
    if (/^\d/.test(query)) return null;

    // after "obj:" only methods make sense (m:start, m:stop)
    const colon = query.indexOf(':');
    if (colon >= 0) {
        const prefix = query.slice(0, colon + 1);
        return {
            query,
            options: api
                .filter((entry) => entry.kind === 'method')
                .map((entry) => {
                    const { insert, tabStops } = snippetFor(entry);
                    return {
                        label: prefix + entry.name,
                        detail: entry.group,
                        icon: 'function',
                        insert: insert && prefix + insert,
                        tabStops: tabStops?.map((n) => n + prefix.length),
                        renderDocs: () => renderEntryDocs(entry)
                    };
                })
        };
    }

    /** @type {Completion[]} */
    const options = api
        .filter((entry) => entry.kind !== 'method' && entry.kind !== 'property')
        .map((entry) => ({
            label: entry.name,
            detail: entry.group,
            icon: entry.kind === 'event' ? 'event' : 'function',
            boost: 2,
            ...snippetFor(entry),
            renderDocs: () => renderEntryDocs(entry)
        }));

    const known = new Set(options.map((o) => o.label));
    for (const name of LUA_GLOBALS) {
        if (!known.has(name)) options.push({ label: name, detail: 'lua', icon: 'function', boost: 1 });
        known.add(name);
    }
    for (const word of LUA_KEYWORDS) {
        options.push({ label: word, icon: 'keyword' });
        known.add(word);
    }
    // names the script already uses (its own functions and variables)
    const words = new Set(documentText.match(/[A-Za-z_]\w{2,}/g) || []);
    words.delete(query);
    for (const word of words) {
        if (!known.has(word)) options.push({ label: word, icon: 'variable', boost: -1 });
    }
    return { query, options };
}

/**
 * The API entry a hovered token names; `before` is the text just before it
 * on the line, so "metro.init" and "m:start" are told apart from "init" and "start".
 * @param {Map<string, ApiEntry>} byName
 * @param {string} text
 * @param {string} before
 * @returns {ApiEntry | null}
 */
export function entryForToken(byName, text, before) {
    const qualified = before.match(/([A-Za-z_]\w*)([.:])$/);
    if (qualified) {
        if (qualified[2] === '.') return byName.get(`${qualified[1]}.${text}`) || null;
        const entry = byName.get(text);
        return entry && entry.kind === 'method' ? entry : null;
    }
    const entry = byName.get(text);
    return entry && entry.kind !== 'method' && entry.kind !== 'property' ? entry : null;
}
