// The editor panel: open a script from the file menu (or a new one), save /
// save as / run, beside the console or stacked above it, hidden to a strip
// without closing. The editing itself is in editor.js. Mixed into diiiApp
// (diii.js); `this` is the app.

import { ScriptEditor } from './editor.js';
import { hasExactFiles, readDeviceFile } from './backup.js';

const STORAGE = {
    layout: 'webdiii.editorLayout', // 'side' or 'stacked'
    side: 'webdiii.editorWidth',
    stacked: 'webdiii.editorHeight'
};
// what the console keeps next to / below the editor
const CONSOLE_MIN = { side: 320, stacked: 160 };
const EDITOR_MIN = { side: 320, stacked: 120 };

function loadSetting(key) {
    try {
        return window.localStorage.getItem(key);
    } catch {
        return null; // storage blocked
    }
}

function saveSetting(key, value) {
    try {
        window.localStorage.setItem(key, String(value));
    } catch {
        // storage blocked
    }
}

// Unsaved edits of the most recently edited file, kept in the browser so a
// disconnect, a closed tab or a crash doesn't lose them.
const DRAFT_KEY = 'webdiii.draft';

function loadDraft() {
    try {
        const draft = JSON.parse(loadSetting(DRAFT_KEY) || 'null');
        return draft && typeof draft.name === 'string' && typeof draft.text === 'string' ? draft : null;
    } catch {
        return null;
    }
}

function clearDraft() {
    try {
        window.localStorage.removeItem(DRAFT_KEY);
    } catch {
        // storage blocked
    }
}

export const editorUiMethods = {
    getScriptEditor() {
        if (!this.scriptEditor) {
            this.scriptEditor = new ScriptEditor(this.elements.editorHost, {
                onChange: (dirty) => this.updateEditorTitle(dirty),
                onInput: () => this.saveDraftSoon(),
                onSave: () => this.editorSave()
            });
        }
        return this.scriptEditor;
    },

    saveDraftSoon() {
        clearTimeout(this.draftTimer);
        this.draftTimer = setTimeout(() => {
            const editor = this.scriptEditor;
            if (!editor?.name) return;
            if (editor.dirty)
                saveSetting(DRAFT_KEY, JSON.stringify({ name: editor.name, text: editor.text, time: Date.now() }));
            else if (loadDraft()?.name === editor.name) clearDraft();
        }, 400);
    },

    // After opening a file: if a draft of it differs from what was loaded, offer it.
    offerDraftRestore(name, loadedText) {
        const draft = loadDraft();
        if (!draft || draft.name !== name || draft.text === loadedText) return;
        const when = new Date(draft.time).toLocaleString();
        if (window.confirm(`Restore your unsaved changes to ${name} from ${when}?`)) {
            this.scriptEditor.setText(draft.text);
            this.outputLine(`restored unsaved changes to ${name}`);
        } else {
            clearDraft();
        }
    },

    // At startup: point at a draft left from an earlier session.
    announceDraft() {
        const draft = loadDraft();
        if (draft) this.outputLine(`unsaved changes to ${draft.name} are kept; open it with edit to restore them`);
    },

    async openFileAtLine(fileName, line) {
        if (this.scriptEditor?.name !== fileName || this.elements.editorPane?.hidden) {
            await this.editFile(fileName);
        }
        if (this.scriptEditor?.name !== fileName) return;
        this.setEditorCollapsed(false);
        this.scriptEditor.goToLine(line);
    },

    // "foo" -> "foo.lua"; null (and a message) for names the device can't take
    normalizeScriptName(raw) {
        let name = String(raw || '').trim();
        if (!name) return null;
        if (!/\.[a-z0-9]+$/i.test(name)) name += '.lua';
        if (name.includes('/') || name.startsWith('.')) {
            this.outputLine(`invalid file name: ${name}`);
            return null;
        }
        return name;
    },

    // Asks before throwing away unsaved edits; false to keep them.
    confirmDiscardEdits(nextName = null) {
        const editor = this.scriptEditor;
        if (!editor?.dirty || editor.name === nextName) return true;
        if (!window.confirm(`Discard unsaved changes to ${editor.name}?`)) return false;
        clearDraft();
        return true;
    },

    async editFile(fileName) {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }
        // already open with edits: show it, don't reload over them
        const open = this.scriptEditor;
        if (open?.name === fileName && open.dirty && !this.elements.editorPane?.hidden) {
            this.setEditorCollapsed(false);
            return;
        }
        if (!this.confirmDiscardEdits(fileName)) return;

        try {
            const api = this.backupApi();
            this.editorExactFiles = await hasExactFiles(api);
            const data = await readDeviceFile(api, fileName, this.editorExactFiles);
            // uploads end with a '\0' the editor shouldn't show
            const text = new TextDecoder().decode(data).replace(/\0+$/, '');
            const editor = this.getScriptEditor();
            this.showEditorPane(true);
            editor.open(fileName, text);
            this.updateEditorTitle(false);
            this.offerDraftRestore(fileName, text);
            editor.focus();
        } catch (error) {
            this.outputLine(`Edit error: ${error.message}`);
        }
    },

    async newFile() {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }
        const name = this.normalizeScriptName(window.prompt('New file name:', 'untitled.lua'));
        if (!name) return;
        if (this.fileEntries.some((entry) => entry.name === name)) {
            this.outputLine(`${name} exists; opening it`);
            await this.editFile(name);
            return;
        }
        if (!this.confirmDiscardEdits()) return;

        this.editorExactFiles = await hasExactFiles(this.backupApi()).catch(() => false);
        const editor = this.getScriptEditor();
        this.showEditorPane(true);
        editor.open(name, '', { isNew: true });
        this.offerDraftRestore(name, '');
        editor.focus();
    },

    // Uploads the editor's text as fileName. On Open Grid firmware the old file
    // stays until the new one compiles; true when the device wrote it.
    async saveEditorAs(fileName) {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return false;
        }
        const editor = this.getScriptEditor();
        const text = editor.text;
        this.outputLine(`saving ${fileName}...`);
        const written = await this.uploadChecked(fileName, text, { keepOld: Boolean(this.editorExactFiles) });

        if (written) {
            editor.markSaved(fileName, text);
            if (!editor.dirty) clearDraft();
            this.updateEditorTitle(editor.dirty);
            this.requestFileListRefresh();
        } else {
            this.outputLine(
                this.editorExactFiles
                    ? `${fileName} not saved; the file on the device is unchanged`
                    : `${fileName} not saved`
            );
        }
        return written;
    },

    async editorSave() {
        const name = this.scriptEditor?.name;
        if (name) await this.saveEditorAs(name);
    },

    async editorSaveAs() {
        const current = this.scriptEditor?.name;
        if (!current) return;
        const name = this.normalizeScriptName(window.prompt('Save as:', current));
        if (!name) return;
        const exists = this.fileEntries.some((entry) => entry.name === name);
        if (name !== current && exists && !window.confirm(`${name} exists. Replace it?`)) return;
        await this.saveEditorAs(name);
    },

    async editorRun() {
        const editor = this.scriptEditor;
        if (!editor?.name) return;
        if (editor.dirty && !(await this.saveEditorAs(editor.name))) return;
        await this.enqueueRunFile(editor.name);
    },

    closeEditor() {
        const editor = this.scriptEditor;
        if (editor?.dirty) {
            if (!window.confirm(`Close ${editor.name} without saving?`)) return;
            clearDraft();
        }
        editor?.close();
        this.showEditorPane(false);
        this.elements.replInput?.focus();
    },

    updateEditorTitle(dirty) {
        const name = this.scriptEditor?.name || '';
        if (this.elements.editorTitle) this.elements.editorTitle.textContent = name;
        if (this.elements.editorDirty) this.elements.editorDirty.hidden = !dirty;
        if (this.elements.editorExpandTitle) this.elements.editorExpandTitle.textContent = dirty ? `${name} ●` : name;
    },

    // --- layout --------------------------------------------------------------

    editorLayout() {
        return loadSetting(STORAGE.layout) === 'stacked' ? 'stacked' : 'side';
    },

    showEditorPane(show) {
        const pane = this.elements.editorPane;
        if (!pane) return;
        pane.hidden = !show;
        this.setEditorCollapsed(false);
        if (show) this.applyEditorLayout();
        else if (this.elements.editorResizer) this.elements.editorResizer.hidden = true;
    },

    applyEditorLayout() {
        const layout = this.editorLayout();
        this.elements.workArea?.classList.toggle('stacked', layout === 'stacked');
        if (this.elements.editorLayoutBtn) {
            this.elements.editorLayoutBtn.textContent = layout === 'stacked' ? '⇆' : '⇅';
            this.elements.editorLayoutBtn.title =
                layout === 'stacked' ? 'editor beside the console' : 'editor above the console';
        }
        if (this.elements.editorResizer) this.elements.editorResizer.hidden = this.isEditorCollapsed();
        if (this.elements.editorResizer) {
            this.elements.editorResizer.setAttribute(
                'aria-orientation',
                layout === 'stacked' ? 'horizontal' : 'vertical'
            );
        }
        // each layout keeps its own size; without one, editor and console share the space
        const size = Number.parseInt(loadSetting(STORAGE[layout]) || '', 10);
        if (Number.isFinite(size)) this.setEditorSize(size, false);
        else if (this.elements.editorPane) this.elements.editorPane.style.flex = '';
    },

    toggleEditorLayout() {
        saveSetting(STORAGE.layout, this.editorLayout() === 'stacked' ? 'side' : 'stacked');
        this.applyEditorLayout();
        this.scriptEditor?.focus();
    },

    isEditorCollapsed() {
        return Boolean(this.elements.editorPane?.classList.contains('collapsed'));
    },

    setEditorCollapsed(collapsed) {
        const pane = this.elements.editorPane;
        if (!pane) return;
        pane.classList.toggle('collapsed', collapsed);
        if (this.elements.editorResizer) this.elements.editorResizer.hidden = collapsed || pane.hidden;
        if (!collapsed) this.scriptEditor?.focus();
    },

    setEditorSize(size, persist = true) {
        const pane = this.elements.editorPane;
        const area = this.elements.workArea;
        if (!pane || !area) return;
        const layout = this.editorLayout();
        const room = layout === 'stacked' ? area.clientHeight : area.clientWidth;
        const max = Math.max(EDITOR_MIN[layout], room - CONSOLE_MIN[layout]);
        const clamped = Math.round(Math.max(EDITOR_MIN[layout], Math.min(max, size)));
        pane.style.flex = `0 0 ${clamped}px`;
        if (persist) saveSetting(STORAGE[layout], clamped);
    },

    startEditorResize(event) {
        const pane = this.elements.editorPane;
        const resizer = this.elements.editorResizer;
        if (!pane || !resizer) return;
        event.preventDefault();
        const stacked = this.editorLayout() === 'stacked';
        const start = stacked ? event.clientY : event.clientX;
        const rect = pane.getBoundingClientRect();
        const startSize = stacked ? rect.height : rect.width;
        const bodyClass = stacked ? 'is-resizing-editor-rows' : 'is-resizing-explorer';
        resizer.setPointerCapture(event.pointerId);
        resizer.classList.add('dragging');
        document.body.classList.add(bodyClass);

        // stacked: the editor is on top, so dragging down grows it;
        // side by side: it's on the right, so dragging left grows it
        const move = (e) =>
            this.setEditorSize(stacked ? startSize + (e.clientY - start) : startSize - (e.clientX - start));
        const end = () => {
            resizer.classList.remove('dragging');
            document.body.classList.remove(bodyClass);
            resizer.removeEventListener('pointermove', move);
            resizer.removeEventListener('pointerup', end);
            resizer.removeEventListener('pointercancel', end);
        };
        resizer.addEventListener('pointermove', move);
        resizer.addEventListener('pointerup', end);
        resizer.addEventListener('pointercancel', end);
    }
};
