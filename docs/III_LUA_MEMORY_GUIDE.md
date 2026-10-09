# Memory survival guide for monome iii Lua scripts (RP2040)

Hard-won lessons from developing **rake** (an arc iii script that hit the heap
ceiling four times and recovered every time without losing features), extended
with the **kria_iii** grid-script refactor (bit-packed storage, §3.5, and the
behavior-diff harness, §5). Written to be self-contained: hand this to any
session working on an iii script — arc or grid — with memory problems. rake
techniques were verified on-device; kria_iii additions were verified
behavior-identical on the desktop proxy (device `mem()` numbers pending) and
are marked as such.

---

## 1. The platform has THREE separate budgets

Confusing them wastes days. A change can pass one budget and die on another.

1. **Source size: ≤ 32 KB per file.** A compile-time check per uploaded file.
   Comments count. You can have **multiple files** — `fs_run_file("other.lua")`
   loads a second file, and each gets its own 32 KB. Splitting is free capacity.
2. **Resident heap** — what `mem()` shows after boot. Rake runs ~161 KB steady.
3. **The load-time allocation peak** — the real killer. The device can print
   `-- compiled ok` on upload and still die with `-- out of memory!` while
   *running*. A config can boot today and OOM after a +1 KB feature. The peak
   moments during boot are (measured, in order of allocation):
   compile of the main file → data-file load → big-table allocation → function
   definitions → **`pset_read` (parsing the saved state file was rake's single
   worst moment)**.

**Rule: `mem()` after boot understates risk.** Budget against the peak, not the
steady state.

## 2. Allocation SHAPE matters as much as size (fragmentation)

The decisive, non-obvious lesson. The RP2040 heap fragments; a build that is
**larger in total bytes can boot while a smaller one OOMs**, because the smaller
one asked for a big *contiguous* block.

Evidence from rake: a "flat bank" layout (one 320-slot Lua array ≈ 5 KB
contiguous) OOM'd at 163.8 KB measured, while a per-field layout (10 arrays of
32 each — tiny power-of-2 blocks) **booted at 165.4 KB measured**. Same data,
different shape.

**Rules:**
- Prefer **many small arrays** over one big one. Best structure found for
  per-voice/per-cell state: **per-field parallel arrays** —
  `bank[FIELD][k]` (10 arrays × 32 entries) — not per-slot tables and not one
  flat stride array. When every field is a small integer, **bit-packing beats
  even per-field arrays** — see §3.5.
- Avoid anything that needs one big block: large `table.unpack` calls (the
  argument list is a stack spike), huge single-table constructors, big
  string concatenations.
- Small allocations (short strings, small tables, bytecode) are "shape-safe" —
  they slot into fragments.

## 3. Lua table overhead is the usual hidden cost

On the device a table costs roughly **40 bytes of header/allocator overhead
before storing a single value** — and small tables round their array part up.

- **Per-item tables are poison at scale.** 96 slot-tables of 11 fields spent
  more on overhead than on data. → per-field arrays (see §2).
- **Lua rounds a table's array part up to a power of two on *incremental*
  insertion.** Filling 320 entries one-by-one allocates capacity 512 (37%
  waste). An 11-element table constructor allocates 16. Sizing dodge: build via
  a constructor/`SETLIST` (`{ n = 0, table.unpack(zeros, 1, N) }` sizes
  *exactly*) — but beware the unpack stack spike (§2); for big data prefer
  per-field arrays sized at powers of two anyway.
- **Dozens of tiny constant tables are a silent few-KB.** rake had ~60 little
  LED/scale tables (5–16 small ints each). Packing them into **byte strings**
  read with `s:byte(i)` reclaimed several KB — a string is one object, no
  per-entry overhead. Encodings that worked:
  - plain lists: one byte per value (`"\3\6\12\24\96"`)
  - signed values: store with an offset (`:byte(i) - 2`)
  - booleans/flags: `"0"`/`"1"` character strings (`:byte(i) == 49`)
  - grouped lists: `\0`-separated runs; pairs: consecutive bytes
  - brightness patterns: hex-character strings, `tonumber(s:sub(i,i), 16)`
  - **Gotcha:** Lua string indices are **negative-from-the-end**. A lookup that
    used to be a safe hash miss (`t[d]` → nil) becomes `s:byte(d+1)` reading
    the *last byte* when `d` is negative. **Range-guard first.**
- **Generate packed strings with a script** (from the source data/JSON), never
  hand-type them; verify by rendering/diffing output.

## 3.5 Bit-packing: one integer per cell (kria_iii, proxy-verified)

The endpoint of §2/§3 for **mutable** per-cell state where every field is a
small integer: pack all fields of a cell into ONE Lua integer via bitfields.
kria_iii replaced 40 nested tables (9 fields × 4 tracks × 16 steps = 640
stored values) with a single flat 64-int array — **−9.4 KB proxy heap**, and
the source got *smaller* despite adding accessors, because uniform data
collapses duplicated code (see below).

This does not contradict §2's warning against flat stride arrays: packing
shrinks the data ~10×, so the one contiguous block is a 64-slot array
(~0.5 KB) — well inside "shape-safe", unlike rake's 320-slot value array.
If a packed array would still be large (hundreds of slots), chunk it per
track/ring instead.

Layout example (9 fields, 27 bits): trigger 1 bit @0, note 3 @1, octave 3 @4,
length 3 @7, prob 3 @10, ratchet-count 3 @13, ratchet-mask 5 @16, alt 3 @21,
velocity 3 @24. Put shifts and masks in byte strings (§3), index cells as
`t*16+s-16`:

```lua
SH="\0\1\4\7\10\13\16\21\24" MKS="\1\7\7\7\7\7\31\7\7"
function gf(f,t,s) return (st[t*16+s-16]>>SH:byte(f))&MKS:byte(f) end
function sf(f,t,s,v) local k=t*16+s-16 local m=MKS:byte(f) local h=SH:byte(f)
  st[k]=(st[k]&~(m<<h))|((v&m)<<h) end
```

What packing buys beyond the table overhead:
- **Masked bulk ops.** "Copy fields X,Y,Z from track A to B" and "clear field
  X on track T" become one masked loop each (`dst=(dst&~m)|(src&m)`), replacing
  a hand-written per-field loop per page/mode. kria_iii's 7 copy branches and
  5 clear branches each collapsed to one call.
- **One packed default constant.** The blank cell is a single int expression
  (`STD=1<<1|5<<4|...`); init and pattern-blank become `st[i]=STD` fills.
- **Serialization reads the whole word** — shift/mask straight into
  `string.char`, no per-field table walks.
- **Multi-field state changes are one store** (e.g. "trigger off also resets
  ratchets": `st[k]=st[k]&~MASK|BITS`).

**Gotchas (these WILL bite):**
- **Lua's 0 is truthy.** A boolean field read returns 0/1, so every former
  boolean site needs `==1` (or `==0` for negation). `not gf(1,t,s)==1` parses
  as `(not gf(...))==1` — always false. Convert `not tr[t][s]` sites by hand
  to `gf(1,t,s)==0` BEFORE any mechanical rewrite.
- **Range-check what you pack.** A raw byte from an old save can exceed the
  field width; mask on pack (`&31`) or it corrupts neighboring fields.
- **Keep the on-flash format unchanged.** Pack/unpack at the pset boundary
  and old saves stay compatible; assert a lossless save→load→save roundtrip
  (`cap(); rst(d); cap()==d`) in the test harness.
- Mechanical conversion order for a live codebase: rewrite **all write sites
  by hand first**, grep to confirm zero assignments remain, then regex the
  reads (`field[a][b]` → `gf(n,a,b)`), then grep for stragglers. Guards like
  `or 1` after a former table read are harmless leftovers (accessor never
  returns nil) — safe to keep during conversion.

**Uniform data begets uniform code.** Once every parameter is "field f of
cell (t,s)", per-page handlers and renderers that differed only in which
array they touched can merge into one parameterized function (field id +
playhead source + style flag). kria_iii merged its note/alt/velocity page
renderers this way. This is where the *source/bytecode* savings come from —
recall §6: function definitions cost as much as data.

Sizing note: on the proxy, packing 640 values into 64 ints paid ~9 KB. Do NOT
convert per-track scalar arrays (a handful of 4-entry tables) this way — the
mutation-through-table-reference calling conventions they enable (passing
`aph`/`als` as arguments) are worth more than the ~2 KB they cost.

## 4. Reclaim levers, ranked by measured payoff in rake

1. **Restructure per-item tables → per-field arrays** (biggest; also fixes
   shape). Doubled pattern capacity (16→32 slots) *and* lowered total RAM.
   If the fields are all small ints, go further: **bit-pack into one int per
   item** (§3.5) — kria_iii's single biggest lever at −9.4 proxy KB.
2. **Make rarely-used permanent tables transient** (~4 KB). rake's pset
   snapshot was a ~60-entry global hash alive all session but only needed
   during `pset_write`. Build it as a **local**, let it be garbage after use.
   Run `collectgarbage()` before the use so the transient starts on a clean
   heap. Grid scripts often have equivalents: UI scratch, preset staging.
3. **Pack tiny constant tables into byte strings** (several KB; see §3).
4. **Derive instead of store.** Any table computable from another in one
   expression should not exist (`beats = pulses/24` killed a float table).
   Check small int tables for a **linear pattern** before packing them into
   strings: kria_iii's octave-offset tables were `{[2]=36,[3]=24,...}` and
   `{-24,-12,0,...}` — both just `(k-c)*12`, so they became arithmetic and no
   storage at all.
5. **Move data AND functions into a second file** loaded via `fs_run_file`.
   This relieves the 32 KB source budget (it does NOT reduce resident heap —
   bytecode is resident either way) and lowers the single-file compile spike.
   Functions there can reference main-file globals freely (resolved at call
   time), so load order doesn't matter for definitions.
6. **`collectgarbage()` at the load-time choke points**: right after each
   `fs_run_file`, right **before `pset_read`** (rake's peak moment, ~5 KB of
   headroom recovered), and at the end of `init()` before the frame loop.

## 5. Measurement methodology (this is what actually worked)

**Desktop proxy.** Build a harness on desktop Lua that stubs the iii C API
(`arc_led`/`grid_led`, `metro`, `midi_*`, `pset_*`, `get_time`, `clamp`, …) and
`dofile`s the script. After load: `collectgarbage(); collectgarbage("count")`.
Absolute KB differ from the device (64-bit vs 32-bit) but the number **tracks
relatively** and brackets the cliff:

- Keep a table of proxy-figure → device-outcome pairs (rake's: 166.4 booted,
  167.2 OOM'd — the cliff bracketed to under 1 KB).
- **Compare against the last-known-good commit**: `git show <good>:file.lua`,
  load both in the harness, diff the counts. Never guess a feature's cost.
- **Trust the proxy's ordering for size** — if the new build measures above a
  config that OOM'd, it will OOM. But the proxy **cannot see shape** (§2): a
  build measuring *below* a booted config can still die if it added a big
  contiguous allocation. Check both.

**Behavioral A/B diff (kria_iii — what makes big refactors safe).** Memory
refactors touch hundreds of access sites; eyeballing is hopeless. Make the
proxy harness prove behavior is *identical*, not just plausible:

- Stubs log **every observable output** (LED state, MIDI bytes, pset writes)
  while a scripted exercise drives every page, edit gesture, copy/clear,
  transport mode, and save/load path. Determinism: seed `math.random`, make
  the `get_time` stub a counter, fire metro callbacks from the script.
- Run the exercise on the last-known-good build and on the refactor;
  **`diff` the logs — they must be byte-identical.** Any divergence is a bug
  or an intended change you must be able to explain.
- **Log LED state as a frame buffer at each `grid_refresh`**, not as
  individual `grid_led` calls. Only the buffer state at refresh is visible on
  hardware, so this is the correct equivalence — and it lets you reorder draw
  loops (row-major vs column-major) when merging renderers. Keep MIDI logging
  order-sensitive; order is real there.
- **Validate new exercise coverage on the known-good build first** (old build
  vs old build via git), so a baseline regenerated after extending the test
  still chains back to the original.
- When refactoring serialization/storage, **assert the lossless roundtrip**
  (`d=cap(); rst(d); assert(cap()==d)`) and log a hex dump of the state
  string — it catches field-order and off-by-one errors instantly.

**On-device breadcrumbs.** When an OOM does happen, bisect the boot:
`print("hp1") mem()` after compile, after data load, after big allocations,
after function defs, after `pset_read`, after final GC. The last breadcrumb
printed before `-- out of memory!` names the dying phase. Remove them after
diagnosis (they cost source bytes).

**Budget rule that emerged:** know your cliff number on the proxy scale, and
keep the measured figure at least ~2 KB under it. Measure BEFORE flashing;
if within a KB of the line, reclaim first, then add the feature.

## 6. Boot/steady profile of a healthy script (rake, for calibration)

| phase | mem() | delta |
|---|---|---|
| after main-file compile + pset_init | 140.0 | — |
| after data file (byte strings) | 140.4 | +0.4 |
| after banks (10×32 per-field ×3) | 148.3 | +7.9 |
| after all function definitions | 156.1 | +7.8 |
| after `pset_read` (**peak**) | 163.4 | +7.3 |
| steady after final GC | 157.8 | −5.6 |

Notable: packed **data is nearly free** (+0.4); **function definitions cost as
much as the data structures** (~8 KB — every global closure + bytecode);
**the pset parse is the peak**.

kria_iii calibration (desktop proxy, same harness before/after): the §3.5
bit-pack + string-pack + dedup pass took proxy load heap 133.4 → 124.0 KB and
source 31,221 → 29,463 bytes, with logged behavior byte-identical. Reference
implementation of the harness and exercise: `tools/harness.lua` +
`tools/exercise.lua` in the kria_iii repo.

## 7. Odds and ends that bit us

- The frame loop should allocate **nothing** (preallocate and reuse all
  scratch); GC pauses cause audible/visible hitches and churn fragments.
- String ops in hot paths are fine **if read-only** (`:byte`, `:sub` on
  constants); never *build* strings per frame.
- pcall-guard every callback (`metro`, `event_*`) so errors print instead of
  the script dying silently — an OOM mid-callback otherwise looks like a hang.
- After renaming a script, its `pset_init` name changes → old saved state is
  orphaned; the old pset file still occupies flash and its parse cost vanishes
  (a rename is accidentally a fresh-state test).
- Re-running a script without a power cycle allocates fresh timers/state each
  time (stock iii scripts behave the same) — always reset before re-running,
  or memory numbers lie.
- 60 fps × ~100 iterations of simple per-frame work is fine CPU-wise on the
  RP2040; memory, not CPU, is the binding constraint at this scale.

## 8. Suggested attack plan for a script with memory issues

1. Build/borrow the desktop harness; get the proxy number for the current tree
   and for the last commit that booted (if any). Bracket the cliff.
2. If it OOMs on-device now: add `mem()` breadcrumbs, flash once, identify the
   dying phase.
3. Inventory the heap: count tables (`grep`-able constructors), find per-item
   table populations, permanent-but-rarely-used tables, derivable tables,
   incremental fills of big arrays.
4. Apply §4 levers in order of payoff; **measure after each** on the proxy.
   For invasive restructures (per-field arrays, §3.5 bit-packing, renderer
   merges), build the behavioral A/B log diff (§5) BEFORE touching code and
   keep every step byte-identical against the last-known-good build.
5. Re-check the 32 KB source budget after edits (comments count); split into a
   second `fs_run_file` file if needed.
6. Add `collectgarbage()` at the §4.6 choke points if absent.
7. Verify behavior with a regression suite run under the harness — memory
   refactors (especially table→string and per-field restructures) touch many
   access sites; tests catch the strays. Flash, confirm boot, record `mem()`.
