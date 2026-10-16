// Types for the parts of vendor/prism-code-editor.js diii uses (written by
// hand; the bundle itself has none). Keep in sync with tools/vendor-editor.mjs.

export interface EditorOptions {
    language: string;
    value: string;
    tabSize?: number;
    insertSpaces?: boolean;
    lineNumbers?: boolean;
    readOnly?: boolean;
    wordWrap?: boolean;
    onUpdate?(value: string): void;
}

export interface PrismEditor {
    readonly container: HTMLDivElement;
    readonly textarea: HTMLTextAreaElement;
    readonly value: string;
    readonly focused: boolean;
    setOptions(options: Partial<EditorOptions>): void;
    remove(): void;
}

export type EditorExtension = object;

export function createEditor(
    container: HTMLElement,
    options: EditorOptions,
    ...extensions: EditorExtension[]
): PrismEditor;
export function editorCommands(keymap: object): EditorExtension;
export const defaultKeymap: object;
export function editHistory(limit?: number): EditorExtension;
export function searchWidget(): EditorExtension;
export function highlightSelectionMatches(): EditorExtension;
export function matchBrackets(rainbowBrackets?: boolean): EditorExtension;
export function highlightBracketPairs(): EditorExtension;
export function indentGuides(): EditorExtension;
export function cursorPosition(): EditorExtension;
