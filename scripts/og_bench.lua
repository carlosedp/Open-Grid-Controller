-- og_bench: repeatable memory and speed numbers for comparing firmware
-- builds (roadmap M4). Needs Open Grid firmware og-0.2.2 or later.
--
-- "high water" is the most heap ever claimed since boot; when it reaches
-- the total, something has run out of memory (often compiling a script).
--
-- Upload with diii (don't set it as `first`), then in the REPL:
--   require("og_bench.lua")      -- defines og_bench()
--   og_bench()                   -- measure what is loaded now
--   og_bench("kria_iii.lua")     -- run that script first, then measure
--
-- Prints one line to paste into the M4 table in docs/MULTIMODE-ROADMAP.md.
-- The redraw test draws on the grid; a running script repaints it after.
-- Stop transport/sequencers first so the timings aren't disturbed.
--
-- Timings:
--   loop         a Lua function call plus a little arithmetic
--   midi_to_msg  one call, with the collector paused: the function itself
--   alloc+gc     midi_to_msg in a long loop, including the garbage
--                collection its tables cause: what a busy script pays
--   gc           one full collection of everything currently loaded
--   redraw       128 grid_led() calls plus grid_refresh()

local function time_us(n, fn)
  local t0 = get_time()
  for i = 1, n do fn(i) end
  return (get_time() - t0) / n * 1e6
end

local function kb(bytes) return bytes / 1024 end

function og_bench(script)
  if script then fs_run_file(script) end

  collectgarbage("collect")
  collectgarbage("collect")
  local lua_kb = collectgarbage("count")
  local free, largest, _, _, total, high_water = og_mem(true)

  local loop_us = time_us(20000, function(i) local x = i * 3 // 7 end)

  -- few enough calls that the garbage fits in RAM with the collector off
  local data = { 0x90, 60, 100 }
  collectgarbage("stop")
  local midi_us = time_us(200, function() midi_to_msg(data) end)
  collectgarbage("restart")
  collectgarbage("collect")

  local churn_us = time_us(2000, function() midi_to_msg(data) end)

  local t0 = get_time()
  collectgarbage("collect")
  local gc_ms = (get_time() - t0) * 1000

  local w, h = grid_size_x(), grid_size_y()
  local redraw_us = time_us(50, function(i)
    for y = 1, h do
      for x = 1, w do grid_led(x, y, (x + y + i) % 16) end
    end
    grid_refresh()
  end)

  print(string.format(
    "og_bench %s %s: lua %.1f KB | heap free %.1f KB, largest %.1f KB, "
      .. "high water %.1f of %.1f KB | loop %.2f us, midi_to_msg %.1f us, "
      .. "alloc+gc %.1f us, gc %.2f ms, redraw %.2f ms",
    device_version(), script or "-", lua_kb, kb(free), kb(largest),
    kb(high_water or 0), kb(total),
    loop_us, midi_us, churn_us, gc_ms, redraw_us / 1000))
end
