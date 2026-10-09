# changelog

## v1.1.1

- NEW iii_vesion tracking, rewrite lib.lua if out of date


## v1.1.0

This is a substantial update to the initial version released with the arc in early 2025.

### file system

NEW: a file system is now maintained on the device.

`lib.lua` contains the basic system library. this file can be modified. if it
gets damaged, do a "clean" boot and erase the file. the default file will be
rewritten at startup.

`init.lua` is then run. this can either be your main lua script, or a simple
selector for the script you want to run. use the helper command `first('this.lua')` to have
`init.lua` run `this.lua` at startup.

### serial commands

note: some of these changed in v1.1.0, listing in full for reference

^^i re-init lua
^^c clean re-init, skip lib.lua and init.lua
^^r restart hardware
^^b bootloader

less common, handled by diii for ease:
^^s start data block
^^f end data: filename
^^w end data: file write
^^p print current file
^^g print current filename

### lua

FIX: print any datatype
CHANGE: get_time() returns seconds in float instead of ms (int) with us
precision
NEW: device_id() returns device name
NEW: device_version() returns device version
CHANGE: arg order for linlin(n, slo, shi, dlo, dhi)

### helpers

require
cat
ls
gc
mem

### fs

fs_read_file
fs_run_file
fs_write_file
fs_remove_file
fs_list_files
fs_file_size
fs_free_space
fs_reformat


### metro

CHANGE: lua api now mirrors norns
CHANGE: args are in seconds (float) instead of ms (int) with us precision

```
-- run tick_function every half second 10 times
m = metro.init(tick_function, 0.5, 10)
m:start()
m.time = 0.25
m:stop()
metro.free(m)
```

### midi

CHANGE: event_midi() was midi_rx()
CHANGE: midi_out(msg) was midi_send(msg)

### grid

CHANGE: event_grid(x,y,z) was grid(x,y,z)
NEW: grid_intensity(z)
NEW: all LED commands take optional last argument, _rel_ (boolean) to add/subtract relative to existing value rather than overwriting

### arc

CHANGE: event_arc(n,d) was arc(n,d)
CHANGE: event_arc_key(z) was arc_key(z)
CHANGE: arc_led_ring(n,z) was arc_led_all(n,z)
NEW: arc_led_all(z)
NEW: arc_intensity(z)
NEW: all LED commands take optional last argument, _rel_ (boolean) to add/subtract relative to existing value rather than overwriting

### slew

CHANGE: args are in seconds (float) instead of ms (int) with us precision

### pset

CHANGE: now saves files which can be downloaded from the device
CHANGE: pset_init(name) sets the file saving pattern: pset_name_xx.lua
