// Bundles the parts of prism-code-editor diii uses into vendor/, which is
// committed: diii itself has no build step, so the site and `make diii` work
// without `npm install`. Rerun after changing the version in package.json:
//
//   npm run vendor

import { build } from 'esbuild';
import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const out = join(root, 'vendor');
// the package's "exports" hides package.json, so go to the folder directly
const pkgDir = join(root, 'node_modules', 'prism-code-editor');
const { version } = JSON.parse(await readFile(join(pkgDir, 'package.json'), 'utf8'));

const entry = `
export { createEditor } from 'prism-code-editor';
import 'prism-code-editor/prism/languages/lua';
import 'prism-code-editor/languages/lua';
export { editorCommands, editHistory, defaultKeymap } from 'prism-code-editor/commands';
export { searchWidget, highlightSelectionMatches } from 'prism-code-editor/search';
export { matchBrackets } from 'prism-code-editor/match-brackets';
export { highlightBracketPairs } from 'prism-code-editor/highlight-brackets';
export { indentGuides } from 'prism-code-editor/guides';
export { cursorPosition } from 'prism-code-editor/cursor';
export { autoComplete, registerCompletions, fuzzyFilter } from 'prism-code-editor/autocomplete';
export { editorHoverDescriptions } from 'prism-code-editor/hover';
`;

await mkdir(out, { recursive: true });
await build({
    stdin: { contents: entry, resolveDir: root, loader: 'js' },
    bundle: true,
    minify: true,
    format: 'esm',
    target: 'es2022',
    legalComments: 'none',
    banner: { js: `/* prism-code-editor ${version} (MIT), bundled by tools/vendor-editor.mjs */` },
    outfile: join(out, 'prism-code-editor.js')
});

const css = await Promise.all(
    ['layout.css', 'guides.css', 'search.css', 'scrollbar.css', 'autocomplete.css', 'autocomplete-icons.css'].map((f) =>
        readFile(join(pkgDir, 'dist', f), 'utf8')
    )
);
await writeFile(
    join(out, 'prism-code-editor.css'),
    `/* prism-code-editor ${version} (MIT): layout, guides, search, scrollbar, autocomplete */\n${css.join('\n')}`
);
await copyFile(join(pkgDir, 'LICENSE'), join(out, 'prism-code-editor.LICENSE'));
console.log(`vendor/prism-code-editor.{js,css} from prism-code-editor ${version}`);
