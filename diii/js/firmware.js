// @ts-check
// Firmware versions and the latest release on GitHub, for diii's update
// helper. Releases carry the UF2s from `make dist`, named
// iii-open-grid-og-X.Y.Z-<board>.uf2.

export const RELEASES_PAGE = 'https://github.com/carlosedp/Open-Grid-Controller/releases';
const LATEST_RELEASE_API = 'https://api.github.com/repos/carlosedp/Open-Grid-Controller/releases/latest';

/**
 * "og-0.3.6" (also "og-0.3.6-200mhz") -> [0, 3, 6]; null when it isn't an
 * Open Grid version (stock iii reports e.g. "v260324").
 * @param {string | null | undefined} version
 * @returns {number[] | null}
 */
export function parseVersion(version) {
    const match = String(version || '').match(/^og-(\d+)\.(\d+)\.(\d+)/);
    return match ? match.slice(1, 4).map(Number) : null;
}

/**
 * Whether `latest` is newer than `current` (both "og-X.Y.Z").
 * @param {string} latest
 * @param {string | null | undefined} current
 */
export function isNewer(latest, current) {
    const a = parseVersion(latest);
    const b = parseVersion(current);
    if (!a || !b) return false;
    for (let i = 0; i < 3; i++) if (a[i] !== b[i]) return a[i] > b[i];
    return false;
}

/**
 * @typedef {object} Release
 * @property {string} version e.g. "og-0.3.6"
 * @property {string} page the release's web page (notes)
 * @property {Record<string, string>} uf2 board -> download URL
 */

/**
 * Reads a GitHub release (API JSON) into what diii needs.
 * @param {{ tag_name?: string, html_url?: string, assets?: { name: string, browser_download_url: string }[] }} json
 * @returns {Release | null}
 */
export function parseRelease(json) {
    /** @type {Record<string, string>} */
    const uf2 = {};
    let version = parseVersion(json.tag_name) ? String(json.tag_name).match(/^og-\d+\.\d+\.\d+/)?.[0] || '' : '';
    for (const asset of json.assets || []) {
        const match = asset.name.match(/^iii-open-grid-(og-\d+\.\d+\.\d+)-([a-z0-9_]+)\.uf2$/);
        if (!match) continue;
        uf2[match[2]] = asset.browser_download_url;
        version ||= match[1];
    }
    if (!version || !Object.keys(uf2).length) return null;
    return { version, page: json.html_url || RELEASES_PAGE, uf2 };
}

/**
 * The latest release, or null when there is none (or it has no UF2s).
 * Throws when GitHub can't be reached (offline).
 * @param {typeof fetch} [fetchFn]
 * @returns {Promise<Release | null>}
 */
export async function fetchLatestRelease(fetchFn = fetch) {
    const response = await fetchFn(LATEST_RELEASE_API, { headers: { Accept: 'application/vnd.github+json' } });
    if (response.status === 404) return null;
    if (!response.ok) throw new Error(`GitHub answered ${response.status}`);
    return parseRelease(await response.json());
}
