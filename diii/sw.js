// Bump CACHE when the list below changes; old caches are deleted on activate.
const CACHE = 'diii-v3';

// Relative to this file, so the app works from any path (e.g. GitHub Pages
// project sites), not only from the site root.
const APP_SHELL_FILES = [
  './',
  './index.html',
  './style.css',
  './diii.js',
  './js/connection.js',
  './js/console.js',
  './js/device.js',
  './js/files.js',
  './js/grid.js',
  './js/layout.js',
  './js/lua.js',
  './js/uploads.js',
  './manifest.webmanifest',
  './icons/logo.svg',
  './icons/icon-192.png',
  './icons/icon-512.png'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE)
      .then((cache) => cache.addAll(APP_SHELL_FILES))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((key) => key !== CACHE).map((key) => caches.delete(key))))
      .then(() => self.clients.claim())
  );
});

// Stale-while-revalidate: answer from the cache right away (fast, works
// offline) and refresh the cached copy in the background, so a new release
// shows up on the next load instead of never.
self.addEventListener('fetch', (event) => {
  if (event.request.method !== 'GET') return;

  const requestUrl = new URL(event.request.url);
  if (requestUrl.origin !== self.location.origin) return;

  event.respondWith(
    caches.open(CACHE).then(async (cache) => {
      const cached = await cache.match(event.request, { ignoreSearch: true });
      const network = fetch(event.request)
        .then((response) => {
          if (response.ok) cache.put(event.request, response.clone());
          return response;
        })
        .catch(() => cached || Response.error());

      if (cached) {
        event.waitUntil(network);
        return cached;
      }
      return network;
    })
  );
});
