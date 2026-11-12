#pragma once

// Startup light patterns that show which mode the grid booted into, as on
// monome's grids: "particles" (an explosion) for monome serial, "plasma"
// (waves) for iii. Each takes about 1.2 s and ends with the grid dark.
//
// Call after og_grid_start() (core 1 pushes the frames to the boards). `idle`
// runs repeatedly between frames so USB keeps enumerating; may be NULL.

void og_boot_anim_particles(void (*idle)(void));
void og_boot_anim_plasma(void (*idle)(void));
