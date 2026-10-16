// @ts-check
// Tab completion for the REPL line: which word is being typed, and what it
// can become. Where the candidates come from (API list, the device's
// globals) is up to the caller (console.js).

/**
 * The name being typed just before the cursor, e.g. "grid_l", "metro.in",
 * "m:st"; `base` is what comes before the last "." or ":" ("metro", "m").
 * @param {string} text
 * @param {number} cursor
 * @returns {{ start: number, word: string, base: string | null, sep: '.' | ':' | null, member: string }}
 */
export function wordBefore(text, cursor) {
    const before = text.slice(0, cursor);
    const match = before.match(/[A-Za-z_][\w]*(?:[.:][A-Za-z_]?\w*)*$|[.:]$/);
    const word = match && /^[A-Za-z_]/.test(match[0]) ? match[0] : '';
    const sepAt = Math.max(word.lastIndexOf('.'), word.lastIndexOf(':'));
    return {
        start: cursor - word.length,
        word,
        base: sepAt >= 0 ? word.slice(0, sepAt) : null,
        sep: sepAt >= 0 ? /** @type {'.' | ':'} */ (word[sepAt]) : null,
        member: sepAt >= 0 ? word.slice(sepAt + 1) : word
    };
}

/**
 * The longest start all strings share.
 * @param {string[]} strings
 */
export function commonPrefix(strings) {
    if (!strings.length) return '';
    let prefix = strings[0];
    for (const s of strings) {
        while (!s.startsWith(prefix)) prefix = prefix.slice(0, -1);
    }
    return prefix;
}

/**
 * Completes `prefix` from `candidates`: the text to put in its place (the
 * shared start of all matches, or the whole name for a single match), and
 * the sorted matches.
 * @param {string} prefix
 * @param {Iterable<string>} candidates
 * @returns {{ completion: string, matches: string[] }}
 */
export function complete(prefix, candidates) {
    const matches = [...new Set(candidates)].filter((c) => c.startsWith(prefix) && c !== '').sort();
    if (!matches.length) return { completion: prefix, matches };
    return { completion: matches.length === 1 ? matches[0] : commonPrefix(matches), matches };
}

/**
 * Matches laid out for the console, a few per line, cut after `max`.
 * @param {string[]} matches
 * @param {number} [max]
 * @returns {string[]}
 */
export function formatMatches(matches, max = 60) {
    const shown = matches.slice(0, max);
    const width = Math.max(...shown.map((m) => m.length)) + 2;
    const perLine = Math.max(1, Math.floor(72 / width));
    const lines = [];
    for (let i = 0; i < shown.length; i += perLine) {
        lines.push(
            shown
                .slice(i, i + perLine)
                .map((m) => m.padEnd(width))
                .join('')
                .trimEnd()
        );
    }
    if (matches.length > max) lines.push(`... and ${matches.length - max} more`);
    return lines;
}
