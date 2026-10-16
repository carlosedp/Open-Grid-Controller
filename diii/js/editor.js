// @ts-check
// The script editor: one file at a time, with Lua highlighting, search
// (Ctrl+F), bracket matching and indent guides (prism-code-editor, vendored).
// It only holds text; saving and running are up to the app (editor-ui.js).

import {
    createEditor,
    cursorPosition,
    defaultKeymap,
    editHistory,
    editorCommands,
    highlightBracketPairs,
    highlightSelectionMatches,
    indentGuides,
    matchBrackets,
    searchWidget
} from '../vendor/prism-code-editor.js';

/**
 * @typedef {object} EditorHooks
 * @property {(dirty: boolean) => void} [onChange] the text started or stopped differing from the saved one
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
        this.savedText = '';
        /** @type {import('../vendor/prism-code-editor.js').PrismEditor | null} */
        this.editor = null;
        this.wasDirty = false;

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
     */
    open(name, text) {
        this.editor?.remove();
        this.editor = createEditor(
            this.host,
            {
                language: 'lua',
                value: text,
                tabSize: 2,
                insertSpaces: true,
                lineNumbers: true,
                onUpdate: () => this.notifyIfChanged()
            },
            editorCommands(defaultKeymap),
            editHistory(),
            searchWidget(),
            highlightSelectionMatches(),
            matchBrackets(),
            highlightBracketPairs(),
            indentGuides(),
            cursorPosition()
        );
        this.name = name;
        this.savedText = text;
        this.wasDirty = false;
        this.hooks.onChange?.(false);
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
        return this.editor !== null && this.text !== this.savedText;
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
