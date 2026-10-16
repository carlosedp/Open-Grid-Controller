// Serves diii for development: http://localhost:<port> (Web Serial needs
// localhost or HTTPS). No dependencies, so it runs without `npm install`.
//
//   node tools/serve.mjs [port]      (default 8000, or $PORT)

import { createServer } from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import { extname, join, normalize, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const port = Number(process.argv[2] || process.env.PORT || 8000);

const types = {
    '.html': 'text/html; charset=utf-8',
    '.js': 'text/javascript; charset=utf-8',
    '.mjs': 'text/javascript; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.json': 'application/json',
    '.webmanifest': 'application/manifest+json',
    '.svg': 'image/svg+xml',
    '.png': 'image/png',
    '.ico': 'image/x-icon',
    '.map': 'application/json'
};

createServer(async (req, res) => {
    const urlPath = decodeURIComponent(new URL(req.url || '/', 'http://localhost').pathname);
    let file = normalize(join(root, urlPath));
    if (file !== root && !file.startsWith(root.endsWith(sep) ? root : root + sep)) {
        res.writeHead(403).end();
        return;
    }
    try {
        if ((await stat(file)).isDirectory()) file = join(file, 'index.html');
        const body = await readFile(file);
        // no-store: edits show up on reload without fighting the browser cache
        res.writeHead(200, {
            'Content-Type': types[extname(file)] || 'application/octet-stream',
            'Cache-Control': 'no-store'
        });
        res.end(body);
    } catch {
        res.writeHead(404, { 'Content-Type': 'text/plain' }).end('not found');
    }
}).listen(port, '127.0.0.1', () => {
    console.log(`diii at http://localhost:${port}  (Ctrl+C to stop)`);
});
