// "update ready": tells when a newer diii was published (diii.vfmod.com) than
// the copy this browser runs, and reloads into it.
//
// Each published build gets its own service-worker cache name (dist.py), so a
// new build means a new sw.js: the browser installs it in the background, and
// since it activates at once (skipWaiting), the next load comes from it. This
// page keeps running the old files until then, so it offers the reload.
// Mixed into diiiApp (diii.js); `this` is the app.

const CHECK_INTERVAL_MS = 60 * 60 * 1000;

export const appUpdateMethods = {
    async watchAppUpdates() {
        if (!('serviceWorker' in navigator)) return;
        // on the first visit the worker takes over without being an update
        const hadController = Boolean(navigator.serviceWorker.controller);

        let registration;
        try {
            registration = await navigator.serviceWorker.register('sw.js');
        } catch (error) {
            console.error('Service worker registration failed:', error);
            return;
        }

        navigator.serviceWorker.addEventListener('controllerchange', () => {
            if (hadController) this.showAppUpdate();
        });

        // browsers look for a new sw.js when the page loads; also look when
        // coming back to the tab and every hour while it stays open
        const check = () => registration.update().catch(() => {});
        document.addEventListener('visibilitychange', () => {
            if (document.visibilityState === 'visible') check();
        });
        setInterval(check, CHECK_INTERVAL_MS);
    },

    showAppUpdate() {
        const button = this.elements.appUpdateBtn;
        if (!button || !button.hidden) return;
        button.hidden = false;
        this.outputLine('a newer diii is ready: click "update ready" in the header to load it');
    },

    // unsaved editor changes: the page asks before leaving (and the draft stays)
    reloadForAppUpdate() {
        window.location.reload();
    }
};
