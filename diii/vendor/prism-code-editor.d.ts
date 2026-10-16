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
    /** [0] holds overlays; [n] is line n */
    readonly lines: HTMLCollectionOf<HTMLDivElement>;
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

export interface CompletionContext {
    before: string;
    lineBefore: string;
    pos: number;
    language: string;
    explicit: boolean;
}
export interface CompletionResult {
    from: number;
    to?: number;
    options: object[];
}
export type CompletionSource = (context: CompletionContext, editor: PrismEditor) => CompletionResult | null | undefined;
export type CompletionFilter = (query: string, option: string) => [number, number[]] | undefined;
export const fuzzyFilter: CompletionFilter;
export function autoComplete(config: {
    filter: CompletionFilter;
    preferAbove?: boolean;
    closeOnBlur?: boolean;
    explicitOnly?: boolean;
}): EditorExtension;
export function registerCompletions(languages: string[], definition: { sources: CompletionSource[] }): void;
export function editorHoverDescriptions(
    callback: (
        types: string[],
        language: string,
        text: string,
        element: HTMLSpanElement,
        editor: PrismEditor
    ) => (string | Node)[] | null | undefined,
    options?: { above?: boolean; maxWidth?: string; maxHeight?: string }
): EditorExtension;
