-- og_bench: repeatable memory and speed numbers for comparing firmware
-- builds (roadmap M4). Needs Open Grid firmware og-0.2.1 or later.
--
-- Upload with diii (don't set it as `first`), then in the REPL:
--   require("og_bench.lua")      -- defines og_bench()
--   og_bench()                   -- measure what is loaded now
--   og_bench("kria_iii.lua")     -- run that script first, then measure
--
-- Prints one line to paste into the M4 table in docs/MULTIMODE-ROADMAP.md.
-- The redraw test draws on the grid; a running script repaints it after.
-- Stop transport/sequencers first so the timings aren't disturbed.

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
  local free, largest, _, peak = og_mem(true)

  -- plain interpreter speed: a function call plus a little arithmetic
  local loop_us = time_us(20000, function(i) local x = i * 3 // 7 end)

  -- library code every MIDI script uses (allocates a table per call)
  local data = { 0x90, 60, 100 }
  local midi_us = time_us(2000, function() midi_to_msg(data) end)

  -- a full redraw, like most grid scripts do on every change
  local w, h = grid_size_x(), grid_size_y()
  local redraw_us = time_us(50, function(i)
    for y = 1, h do
      for x = 1, w do grid_led(x, y, (x + y + i) % 16) end
    end
    grid_refresh()
  end)

  print(string.format(
    "og_bench %s %s: lua %.1f KB | heap free %.1f KB, largest %.1f KB, peak %.1f KB | "
      .. "loop %.2f us, midi_to_msg %.1f us, redraw %.2f ms",
    device_version(), script or "-", lua_kb, kb(free), kb(largest), kb(peak),
    loop_us, midi_us, redraw_us / 1000))
end
