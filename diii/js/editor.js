// @ts-check
// The script editor: one file at a time, with Lua highlighting, search
// (Ctrl+F), bracket matching and indent guides (prism-code-editor, vendored).
// It only holds text; saving and running are up to the app (editor-ui.js).

import {
    autoComplete,
    createEditor,
    cursorPosition,
    defaultKeymap,
    editHistory,
    editorCommands,
    editorHoverDescriptions,
    fuzzyFilter,
    highlightBracketPairs,
    highlightSelectionMatches,
    indentGuides,
    matchBrackets,
    registerCompletions,
    searchWidget
} from '../vendor/prism-code-editor.js';
import { completionsAt, entryForToken, renderEntryDocs } from './completions.js';

/** @type {import('./iii-api.js').ApiEntry[]} */
let currentApi = [];
/** @type {Map<string, import('./iii-api.js').ApiEntry>} */
let apiByName = new Map();

/**
 * Sets the iii functions autocomplete and hover help offer (they depend on
 * the connected device).
 * @param {import('./iii-api.js').ApiEntry[]} api
 */
export function setEditorApi(api) {
    currentApi = api;
    apiByName = new Map(api.map((entry) => [entry.name, entry]));
}

registerCompletions(['lua'], {
    sources: [
        (context, editor) => {
            const result = completionsAt(currentApi, context.lineBefore, editor.value, context.explicit);
            return result && { from: context.pos - result.query.length, options: result.options };
        }
    ]
});

/**
 * Hover help: the signature and description of the iii function under the pointer.
 * @param {string[]} _types
 * @param {string} _language
 * @param {string} text
 * @param {HTMLSpanElement} element
 */
function hoverHelp(_types, _language, text, element) {
    let before = '';
    for (let node = element.previousSibling; node && before.length < 40; node = node.previousSibling) {
        before = (node.textContent || '') + before;
    }
    const entry = entryForToken(apiByName, text, before);
    return entry ? renderEntryDocs(entry) : null;
}

/**
 * @typedef {object} EditorHooks
 * @property {(dirty: boolean) => void} [onChange] the text started or stopped differing from the saved one
 * @property {() => void} [onInput] the text changed (every edit)
 * @property {() => void} [onSave] Ctrl+S / Cmd+S
 */

export class ScriptEditor {
    /**
     * @param {HTMLElement} host element the editor fills
     * @param {EditorHooks} [hooks]
     */
    constructor(host, hooks = {}) {
        this.host = host;
        this.hooks = hooks;
        /** @type {string | null} */
        this.name = null;
        /** text on the device; null for a new file that isn't there yet
         * @type {string | null} */
        this.savedText = '';
        /** @type {import('../vendor/prism-code-editor.js').PrismEditor | null} */
        this.editor = null;
        this.wasDirty = false;
        /** long lines wrap instead of scrolling sideways (kept across files) */
        this.wordWrap = false;

        host.addEventListener('keydown', (event) => {
            if ((event.ctrlKey || event.metaKey) && !event.altKey && event.key.toLowerCase() === 's') {
                event.preventDefault();
                this.hooks.onSave?.();
            }
        });
    }

    /**
     * Shows a file; a new editor each time, so undo history doesn't cross files.
     * @param {string} name
     * @param {string} text
     * @param {{ isNew?: boolean }} [options] isNew: not on the device yet (unsaved until saved)
     */
    open(name, text, { isNew = false } = {}) {
        this.editor?.remove();
        this.editor = createEditor(
            this.host,
            {
                language: 'lua',
                value: text,
                tabSize: 2,
                insertSpaces: true,
                lineNumbers: true,
                wordWrap: this.wordWrap,
                onUpdate: () => {
                    this.notifyIfChanged();
                    this.hooks.onInput?.();
                }
            },
            editorCommands(defaultKeymap),
            editHistory(),
            searchWidget(),
            highlightSelectionMatches(),
            matchBrackets(),
            highlightBracketPairs(),
            indentGuides(),
            cursorPosition(),
            autoComplete({ filter: fuzzyFilter }),
            editorHoverDescriptions(hoverHelp, { maxWidth: '420px' })
        );
        this.name = name;
        this.savedText = isNew ? null : text;
        this.wasDirty = isNew;
        this.hooks.onChange?.(isNew);
    }

    close() {
        this.editor?.remove();
        this.editor = null;
        this.name = null;
        this.savedText = '';
        this.wasDirty = false;
    }

    get text() {
        return this.editor ? this.editor.value : '';
    }

    get dirty() {
        return this.editor !== null && (this.savedText === null || this.text !== this.savedText);
    }

    /**
     * Records that `text` is now what the device has as `name`.
     * @param {string} name
     * @param {string} text
     */
    markSaved(name, text) {
        this.name = name;
        this.savedText = text;
        this.notifyIfChanged();
    }

    /**
     * Replaces the text (e.g. with a restored draft); it counts as an edit.
     * @param {string} text
     */
    setText(text) {
        this.editor?.setOptions({ value: text });
        this.notifyIfChanged();
    }

    /**
     * Selects line `line` (1-based) and scrolls it into view.
     * @param {number} line
     */
    goToLine(line) {
        if (!this.editor) return;
        const lines = this.text.split('\n');
        const n = Math.max(1, Math.min(Math.floor(line) || 1, lines.length));
        let start = 0;
        for (let i = 0; i < n - 1; i++) start += lines[i].length + 1;
        const textarea = this.editor.textarea;
        textarea.focus();
        textarea.setSelectionRange(start, start + lines[n - 1].length);
        // lines[0] is the editor's overlay; line n is lines[n]
        this.editor.lines[n]?.scrollIntoView({ block: 'center' });
    }

    /**
     * Wraps long lines (or not) in the open file and the ones opened later.
     * @param {boolean} on
     */
    setWordWrap(on) {
        this.wordWrap = on;
        this.editor?.setOptions({ wordWrap: on });
    }

    focus() {
        this.editor?.textarea.focus();
    }

    notifyIfChanged() {
        const dirty = this.dirty;
        if (dirty === this.wasDirty) return;
        this.wasDirty = dirty;
        this.hooks.onChange?.(dirty);
    }
}
