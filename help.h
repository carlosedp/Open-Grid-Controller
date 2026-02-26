const char *help_txt = "\n\
midi\n\
  event_midi(byte1, byte2, byte3)\n\
  midi_out(msg)\n\
  midi_note_on(note, vel, ch)\n\
  midi_note_off(note, vel, ch)\n\
  midi_cc(cc, val, ch)\n\
metro\n\
  m = metro.init(callback, time_sec, count_optional)\n\
  m:start()\n\
  m:stop()\n\
slew\n\
  id = slew.new(callback, start_val, end_val, time_sec)\n\
  slew.to(id, end_val, time_optional)\n\
  slew.freeze(id)\n\
  slew.allfreeze()\n\
  slew.stop(id)\n\
  slew.allstop()\n\
pset\n\
  pset_init(name)\n\
  pset_write(index, table)\n\
  table = pset_read(index)\n\
utils\n\
  dostring(lua_command)\n\
  get_time()\n\
  ps(formatted_string, ...)\n\
  pt(table_to_print)\n\
  clamp(n, min, max)\n\
  round(n, quant)\n\
  linlin(n, slo, shi, dlo, dhi)\n\
  wrap(n, min, max)\n\
system\n\
  device_id()\n\
  first(file)\n\
  ls()\n\
  cat(file)\n\
  rm(file)\n\
  mem()\n\
  gc()\n\
  require(file)\n\
";
