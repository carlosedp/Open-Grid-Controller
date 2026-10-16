// @ts-check
// The offline iii reference: every function with its signature and
// description, grouped and searchable. Built from iii-api.js, so it works
// without a connection to monome.org.

/** @typedef {import('./iii-api.js').ApiEntry} ApiEntry */

/**
 * Fills `list` with the entries matching `query` (name, signature or description).
 * @param {HTMLElement} list
 * @param {ApiEntry[]} entries
 * @param {string} query
 */
export function renderReference(list, entries, query) {
    const q = query.trim().toLowerCase();
    const matches = entries.filter(
        (e) =>
            !q || e.name.toLowerCase().includes(q) || e.sig.toLowerCase().includes(q) || e.doc.toLowerCase().includes(q)
    );

    list.replaceChildren();
    if (!matches.length) {
        const empty = document.createElement('p');
        empty.className = 'reference-empty';
        empty.textContent = `nothing matches "${query}"`;
        list.append(empty);
        return;
    }

    /** @type {Map<string, ApiEntry[]>} */
    const groups = new Map();
    for (const entry of matches) groups.set(entry.group, [...(groups.get(entry.group) || []), entry]);

    for (const [group, items] of groups) {
        const heading = document.createElement('h3');
        heading.className = 'reference-group';
        heading.textContent = group;
        list.append(heading);
        for (const entry of items) {
            const row = document.createElement('div');
            row.className = 'reference-entry';
            const sig = document.createElement('code');
            sig.textContent = entry.sig;
            const doc = document.createElement('p');
            doc.textContent = entry.doc;
            row.append(sig, doc);
            list.append(row);
        }
    }
}
