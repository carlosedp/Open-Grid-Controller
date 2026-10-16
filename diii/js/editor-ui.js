// The editor panel: open a script from the file menu, save / save as / run.
// The editing itself is in editor.js. Mixed into diiiApp (diii.js); `this`
// is the app.

import { ScriptEditor } from './editor.js';
import { hasExactFiles, readDeviceFile } from './backup.js';

const EDITOR_WIDTH_KEY = 'webdiii.editorWidth';

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

    async editFile(fileName) {
        if (!this.iiiDevice.isConnected) {
            this.outputLine('Error: Not connected to usb device');
            return;
        }
        const editor = this.getScriptEditor();
        if (editor.dirty && editor.name !== fileName) {
            if (!window.confirm(`Discard unsaved changes to ${editor.name}?`)) return;
        }

        try {
            const api = this.backupApi();
            this.editorExactFiles = await hasExactFiles(api);
            const data = await readDeviceFile(api, fileName, this.editorExactFiles);
            // uploads end with a '\0' the editor shouldn't show
            const text = new TextDecoder().decode(data).replace(/\0+$/, '');
            this.showEditorPane(true);
            editor.open(fileName, text);
            this.updateEditorTitle(false);
            editor.focus();
        } catch (error) {
            this.outputLine(`Edit error: ${error.message}`);
        }
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
        let name = window.prompt('Save as:', current)?.trim();
        if (!name) return;
        if (!/\.[a-z0-9]+$/i.test(name)) name += '.lua';
        if (name.includes('/') || name.startsWith('.')) {
            this.outputLine(`Save error: invalid file name ${name}`);
            return;
        }
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
        if (this.elements.editorTitle) this.elements.editorTitle.textContent = this.scriptEditor?.name || '';
        if (this.elements.editorDirty) this.elements.editorDirty.hidden = !dirty;
    },

    showEditorPane(show) {
        const pane = this.elements.editorPane;
        if (!pane) return;
        pane.hidden = !show;
        if (this.elements.editorResizer) this.elements.editorResizer.hidden = !show;
        if (show) {
            let width = null;
            try {
                width = Number.parseInt(window.localStorage.getItem(EDITOR_WIDTH_KEY) || '', 10);
            } catch {
                // ignore localStorage failures
            }
            if (Number.isFinite(width)) this.setEditorWidth(width, false);
        }
    },

    setEditorWidth(width, persist = true) {
        const pane = this.elements.editorPane;
        const container = this.elements.splitContainer;
        if (!pane || !container) return;
        // leave the console at least 320 px
        const max = Math.max(320, container.clientWidth - 320 - (this.getExplorerWidth?.() || 0));
        const clamped = Math.round(Math.max(320, Math.min(max, width)));
        pane.style.flex = `0 0 ${clamped}px`;
        if (persist) {
            try {
                window.localStorage.setItem(EDITOR_WIDTH_KEY, String(clamped));
            } catch {
                // ignore localStorage failures
            }
        }
    },

    startEditorResize(event) {
        const pane = this.elements.editorPane;
        const resizer = this.elements.editorResizer;
        if (!pane || !resizer) return;
        event.preventDefault();
        const startX = event.clientX;
        const startWidth = pane.getBoundingClientRect().width;
        resizer.setPointerCapture(event.pointerId);
        resizer.classList.add('dragging');
        document.body.classList.add('is-resizing-explorer');

        const move = (e) => this.setEditorWidth(startWidth - (e.clientX - startX));
        const end = () => {
            resizer.classList.remove('dragging');
            document.body.classList.remove('is-resizing-explorer');
            resizer.removeEventListener('pointermove', move);
            resizer.removeEventListener('pointerup', end);
            resizer.removeEventListener('pointercancel', end);
        };
        resizer.addEventListener('pointermove', move);
        resizer.addEventListener('pointerup', end);
        resizer.addEventListener('pointercancel', end);
    }
};
