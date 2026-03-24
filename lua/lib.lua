iii_version="v1.1.1"

function first(name)
  if name then fs_write_file("init.lua","fs_run_file('"..name.."')")
  else fs_remove_file("init.lua") end
end
function ls() pt(fs_list_files()) end
function rm(name) fs_remove_file(name) end
function cat(x) print(fs_read_file(x)) end
function mem() print(collectgarbage("count")) end
function gc() collectgarbage() mem() end
require = fs_run_file

-- MIDI
midi_out = function(data)
  if data.type then
    local d = msg_to_midi[data.type](data)
    midi_tx(d[1],d[2],d[3])
  end
end

msg_to_midi = {
  note_on = function(msg) return {0x90 + (msg.ch or 1)-1, msg.note, msg.vel or 100} end,
  note_off = function(msg) return {0x80 + (msg.ch or 1)-1, msg.note, msg.vel or 100} end,
  cc = function(msg) return {0xb0 + (msg.ch or 1) - 1, msg.cc, msg.val} end,
  pitchbend = function(msg) return {0xe0 + (msg.ch or 1) - 1, msg.val & 0x7f, (msg.val >> 7) & 0x7f} end,
  key_pressure = function(msg) return {0xa0 + (msg.ch or 1) - 1, msg.note, msg.val} end,
  channel_pressure = function(msg) return {0xd0 + (msg.ch or 1) - 1, msg.val} end,
  program_change = function(msg) return {0xc0 + (msg.ch or 1) - 1, msg.val} end,
  start = function(msg) return {0xfa} end,
  stop = function(msg) return {0xfc} end,
  continue = function(msg) return {0xfb} end,
  clock = function(msg) return {0xf8} end,
  song_position = function(msg) return {0xf2, msg.lsb, msg.msb} end,
  song_select = function(msg) return {0xf3, msg.val} end
}

function midi_to_msg(data)
  local msg = {}
  -- note on
  if data[1] & 0xf0 == 0x90 then
    msg = { note = data[2], vel = data[3], ch = data[1] - 0x90 + 1 }
    if data[3] > 0 then msg.type = "note_on"
    elseif data[3] == 0 then -- if velocity is zero then send note off
      msg.type = "note_off"
    end
    -- note off
  elseif data[1] & 0xf0 == 0x80 then
    msg = { type = "note_off", note = data[2], vel = data[3], ch = data[1] - 0x80 + 1 }
    -- cc
  elseif data[1] & 0xf0 == 0xb0 then
    msg = { type = "cc", cc = data[2], val = data[3], ch = data[1] - 0xb0 + 1 }
    -- pitchbend
  elseif data[1] & 0xf0 == 0xe0 then
    msg = { type = "pitchbend", val = data[2] + (data[3] << 7), ch = data[1] - 0xe0 + 1 }
    -- key pressure
  elseif data[1] & 0xf0 == 0xa0 then
    msg = { type = "key_pressure", note = data[2], val = data[3], ch = data[1] - 0xa0 + 1 }
    -- channel pressure
  elseif data[1] & 0xf0 == 0xd0 then
    msg = { type = "channel_pressure", val = data[2], ch = data[1] - 0xd0 + 1 }
    -- program change
  elseif data[1] & 0xf0 == 0xc0 then
    msg = { type = "program_change", val = data[2], ch = data[1] - 0xc0 + 1 }
    -- start
  elseif data[1] == 0xfa then
    msg.type = "start"
    -- stop
  elseif data[1] == 0xfc then
    msg.type = "stop"
    -- continue
  elseif data[1] == 0xfb then
    msg.type = "continue"
    -- clock
  elseif data[1] == 0xf8 then
    msg.type = "clock"
    -- song position pointer
  elseif data[1] == 0xf2 then
    msg = { type = "song_position", lsb = data[2], msb = data[3] }
    -- song select
  elseif data[1] == 0xf3 then
    msg = { type = "song_select", val = data[2] }
    -- active sensing (should probably ignore)
  elseif data[1] == 0xfe then
    -- do nothing
    -- system exclusive
  elseif data[1] == 0xf0 then
    msg = { type = "sysex", raw = data, }
    -- everything else
  else
    msg = { type = "other", raw = data, }
  end
  return msg
end

-- midi helper shortcuts
midi_note_on = function(note, vel, ch)
  midi_tx((ch or 1)-1 + 0x90, note or 66, vel or 100)
end
midi_note_off = function(note, vel, ch)
  midi_tx((ch or 1)-1 + 0x80, note or 66, vel or 100)
end
midi_cc = function(cc, val, ch)
  midi_tx((ch or 1)-1 + 0xb0, cc or 1, val or 100)
end


-- METRO

metro = {}
metro.__index = metro
metro.metros = {}
metro.available = {}
metro.assigned = {}

function metro.init(arg, arg_time, arg_count)
  local event = 0
  local time = arg_time or 1
  local count = arg_count or -1
  if type(arg) == "table" then
    event = arg.event
    time = arg.time or 1
    count = arg.count or -1
  else
    event = arg
  end
  local id = nil
  for i, val in pairs(metro.available) do
    if val == true then
      id = i
      break
    end
  end
  if id ~= nil then
    metro.assigned[id] = true
    metro.available[id] = false
    local m = metro.metros[id]
    m.event = event
    m.time = time
    m.count= count
    return m
  end
  print("metro.init: nothing available")
  return nil
end

function metro.free(id)
  metro.metros[id]:stop()
  metro.available[id] = true
  metro.assigned[id] = false
end

function metro.free_all()
  for i=1,12 do metro.free(i) end
end

function metro.new(id)
  local m = {}
  m.props = {
    id = id,
    time = 1,
    count = -1,
    event = nil,
    init_stage = 1
  }
  setmetatable(m, metro)
  return m
end

function metro:start(time, count, stage)
  if type(time) == "table" then
    if time.time then self.props.time = time.time end
    if time.count then self.props.count = time.count end
    if time.stage then self.props.init_stage = time.stage end
  else
    if time then self.props.time = time end
    if count then self.props.count = count end
    if stage then self.props.init_stage = stage end
  end
  self.is_running = true
  metro_set(self.props.id, self.props.time, self.props.count)
end

function metro:stop()
  metro_set(self.props.id, 0)
  self.is_running = false
end

metro.__newindex = function(self, idx, val)
  if idx == "time" then
    self.props.time = val
    if self.is_running then metro_set(self.props.id, self.props.time) end
  elseif idx == 'count' then self.props.count = val
  elseif idx == 'init_stage' then self.props.init_stage = val
  else -- FIXME: dunno if this is even necessary / a good idea to allow
    rawset(self, idx, val)
  end
end

metro.__index = function(self, idx)
  if type(idx) == "number" then return metro.metros[idx]
  elseif idx == "start" then return metro.start
  elseif idx == "stop" then return metro.stop
  elseif idx == 'id' then return self.props.id
  elseif idx == 'count' then return self.props.count
  elseif idx == 'time' then return self.props.time
  elseif idx == 'init_stage' then return self.props.init_stage
  elseif self.props.idx then return self.props.idx
  else return rawget(self, idx)
  end
end

setmetatable(metro, metro)

for i=1,12 do
  metro.metros[i] = metro.new(i)
end

for i=1,12 do
  metro.available[i] = true
  metro.assigned[i] = false
end

metro_event = function(idx, stage)
  local m = metro.metros[idx]
  if m then
    if m.event then m.event(stage) end
    if m.count > -1 then
      if (stage > m.count) then
        m.is_running = false
      end
    end
  end
end


-- utils

ps = function(...) print(string.format(...)) end
pt = function(t) for k,v in pairs(t) do print(k .. '\t' .. tostring(v)) end end

function round(number, quant)
  if quant == 0 then return number
  else return math.floor(number/(quant or 1) + 0.5) * (quant or 1) end
end

function wrap(n, min, max)
  if max < min then
    local temp = min
    min = max
    max = temp
  end
  if n >= min and n <= max then
    return n
  end
  local d = max - min + 1
  local y = (n - min) % d
  return y + min
end

function linlin(n, slo, shi, dlo, dhi)
  if n <= slo then return dlo
  elseif n >= shi then return dhi
  else return (n-slo) / (shi-slo) * (dhi-dlo) + dlo
  end
end

function clamp(n, min, max)
  return math.min(max,(math.max(n,min)))
end

function _quote(val, ...)
  -- stringify any data so lua can load() it
  if ... ~= nil then
    local t = {_quote(val)} -- capture 1st arg
    for _,v in ipairs{...} do -- capture varargs
      table.insert(t, _quote(v))
    end
    return table.concat(t, ',')
  elseif type(val) == 'string' then return string.format('%q',val)
  elseif type(val) == 'number' then return string.format('%.6g',val):gsub("%.?0+$","") -- 6 sig figures
  elseif type(val) ~= 'table' then return tostring(val)
  else -- recur per table element
    local t = {}
    for k,v in pairs(val) do
      -- FIXME: float indices not handled right, so we can strip trailing zero
      local fstr = (type(k)=='number') and '[%d]' or '[%q]'
      local f = string.format(fstr, k)
      table.insert(t, f .. '=' .. _quote(v))
    end
    return string.format('{%s}', table.concat(t, ','))
  end
end

pset_name = "pset_"
pset_slot = {}

function pset_init(name)
  pset_name = "pset_"..name.."_"
  pset_slot = {}
  for _,v in pairs(fs_list_files()) do
    if v:sub(1,#pset_name) == pset_name then
      local b = v:sub(1+#pset_name)
      pset_slot[tonumber(b:sub(1,-5))] = true
    end
  end
end

function pset_write(n, data)
  if n < 1 then return end
  local d = "return " .. _quote(data)
  fs_write_file(pset_name..n..".lua",d)
  pset_slot[n] = true
end

function pset_read(n)
  if n < 1 then return nil end
  if not pset_slot[n] then return nil end
  return fs_run_file(pset_name..n..".lua")
end

function pset_delete(n)
  fs_remove_file(pset_name..n..".lua")
  pset_slot[n] = nil
end

-- SLEW
slew = {}
slew.n = 1 -- upward counting id number
slew.s = {} -- data table
-- TODO: do we want autostop (stop slew on completion)
slew.new = function(cb, v_start, v_end, time, q)
  slew.s[slew.n] = { f=cb, s=v_start, e=v_end, t=(time or 0.1), q=(q or 1)}
  local v = slew.s[slew.n]
  v.t = math.floor(v.t*100)/100
  v.c = v.t * 100
  if v.c < 2 then
    v.n = v.e
    v.run = false
    --print("slew: one stage only")
  else
    v.d = (v.e - v.s) / v.c
    v.n = v.s
    v.l = v.n
    v.run = true
  end
  v.f(v.n)
  slew.n = slew.n + 1
  return slew.n - 1
end
slew.to = function(id, v_end, time)
  if slew.s[id] then
    local v = slew.s[id]
    if time then v.t = time end
    v.e = v_end
    v.c = v.t * 100
    if v.c < 2 then
      v.n = v.e
      v.run = false
      v.f(v.n)
      --print("slew: single")
    else
      v.s = v.n
      v.d = (v.e - v.s) / v.c
      v.l = v.n
      v.run = true
    end
  end
end
slew.freeze = function(id)
  if slew.s[id] then slew.s[id].run = false end
end
slew.stop = function(id)
  if slew.s[id] then slew.s[id] = nil end
end
slew.allfreeze = function()
  for k,_ in pairs(slew.s) do slew.s[k].run = false end
end
slew.allstop = function()
  for k,_ in pairs(slew.s) do slew.s[k] = nil end
end
slew.process = function()
  for k,v in pairs(slew.s) do
    if v.run then
      v.n = v.n + v.d
      local r = round(v.n,v.q)
      if r ~= v.l then
        v.l = r
        v.f(r)
      end
      v.c = v.c - 1
      if v.c < 1 then
        v.run = false
      end
    end
  end
end
slew.init = function()
  if not slew.id then
    slew.id = metro.init(slew.process,0.01)
    slew.id:start()
  end
end
slew.deinit = function()
  if slew.id then metro.free(slew.id) end
end
slew.init()
