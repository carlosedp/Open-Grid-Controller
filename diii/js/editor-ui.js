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

export const editorUiMethods = {
    getScriptEditor() {
        if (!this.scriptEditor) {
            this.scriptEditor = new ScriptEditor(this.elements.editorHost, {
                onChange: (dirty) => this.updateEditorTitle(dirty),
                onSave: () => this.editorSave()
            });
        }
        return this.scriptEditor;
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
        return window.confirm(`Discard unsaved changes to ${editor.name}?`);
    },

    async editFile(fileName) {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
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
        let written = false;
        const observe = (line) => {
            if (line === `-- file written: ${fileName}`) written = true;
        };
        this.outputObservers = this.outputObservers || new Set();
        this.outputObservers.add(observe);

        try {
            this.outputLine(`saving ${fileName}...`);
            await this.sendScriptTextToiii(fileName, text, { keepOld: Boolean(this.editorExactFiles) });
        } catch (error) {
            this.outputLine(`Save error: ${error.message}`);
        } finally {
            this.outputObservers.delete(observe);
        }

        if (written) {
            editor.markSaved(fileName, text);
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
        if (editor?.dirty && !window.confirm(`Close ${editor.name} without saving?`)) return;
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
