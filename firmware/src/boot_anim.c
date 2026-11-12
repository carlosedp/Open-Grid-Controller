#include "boot_anim.h"

#include <math.h>
#include <stddef.h>

#include "pico/time.h"

#include "grid.h"

#define FRAME_MS 16

#define PARTICLES 28
#define PARTICLE_FRAMES 64
#define PLASMA_FRAMES 76

// Wait for the next frame without blocking USB.
static void wait_frame(absolute_time_t *next, void (*idle)(void)) {
  *next = delayed_by_ms(*next, FRAME_MS);
  while (!time_reached(*next)) {
    if (idle) idle();
  }
}

static uint32_t rng_state;

// xorshift32: different every boot, nothing to seed from but the clock.
static uint32_t rng(void) {
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return rng_state = x;
}

// 0..1
static float rng_unit(void) { return (float)(rng() >> 8) / 16777216.0f; }

static void fade_all(uint8_t step) {
  for (uint8_t y = 0; y < OG_GRID_H; y++) {
    for (uint8_t x = 0; x < OG_GRID_W; x++) {
      const uint8_t z = og_grid_led_get(x, y);
      og_grid_led(x, y, z > step ? (uint8_t)(z - step) : 0);
    }
  }
}

void og_boot_anim_particles(void (*idle)(void)) {
  float px[PARTICLES], py[PARTICLES], vx[PARTICLES], vy[PARTICLES];
  rng_state = time_us_32() | 1u;
  const float cx = (OG_GRID_W - 1) / 2.0f, cy = (OG_GRID_H - 1) / 2.0f;
  for (int i = 0; i < PARTICLES; i++) {
    const float a = rng_unit() * 6.2831853f;
    const float v = 0.15f + rng_unit() * 0.45f;  // keys per frame
    px[i] = cx;
    py[i] = cy;
    vx[i] = cosf(a) * v;
    vy[i] = sinf(a) * v * 0.8f;  // the grid is wider than tall
  }

  og_grid_led_all(0);
  absolute_time_t next = get_absolute_time();
  for (int f = 0; f < PARTICLE_FRAMES; f++) {
    fade_all(3);  // trails
    if (f < 3) {  // the flash at the center
      const uint8_t z = (uint8_t)(15 - f * 4);
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 2; dx++) og_grid_led((uint8_t)(7 + dx), (uint8_t)(3 + dy), z);
      }
    }
    // Sparks dim as they fly, so the burst dies out before the edges.
    const uint8_t spark = (uint8_t)(15 - f * 10 / PARTICLE_FRAMES);
    for (int i = 0; i < PARTICLES; i++) {
      px[i] += vx[i];
      py[i] += vy[i];
      vx[i] *= 0.96f;                 // drag
      vy[i] = vy[i] * 0.96f + 0.01f;  // a little gravity
      const int x = (int)lroundf(px[i]), y = (int)lroundf(py[i]);
      if (x < 0 || x >= OG_GRID_W || y < 0 || y >= OG_GRID_H) continue;
      og_grid_led((uint8_t)x, (uint8_t)y, spark);
    }
    og_grid_refresh();
    wait_frame(&next, idle);
  }
  for (int f = 0; f < 6; f++) {  // let the trails go out
    fade_all(3);
    og_grid_refresh();
    wait_frame(&next, idle);
  }
  og_grid_led_all(0);
  og_grid_refresh();
}

void og_boot_anim_plasma(void (*idle)(void)) {
  og_grid_led_all(0);
  absolute_time_t next = get_absolute_time();
  for (int f = 0; f < PLASMA_FRAMES; f++) {
    const float t = (float)f * 0.18f;
    // Swell in, then ebb away, so the grid ends dark.
    const float env = sinf((float)f / (PLASMA_FRAMES - 1) * 3.1415927f);
    for (uint8_t y = 0; y < OG_GRID_H; y++) {
      for (uint8_t x = 0; x < OG_GRID_W; x++) {
        const float fx = (float)x, fy = (float)y;
        const float dx = fx - 7.5f - 4.0f * sinf(t * 0.7f), dy = fy - 3.5f;
        const float v = sinf(fx * 0.45f + t) + sinf(fy * 0.6f - t * 0.8f) +
                        sinf((fx + fy) * 0.3f + t * 1.3f) +
                        sinf(sqrtf(dx * dx + dy * dy) * 0.7f - t);
        // v is in -4..4; show the crests only, so the waves read as bands.
        const float z = (v + 1.0f) * 0.2f;
        const float lit = z < 0.0f ? 0.0f : (z > 1.0f ? 1.0f : z);
        og_grid_led(x, y, (uint8_t)lroundf(lit * env * 15.0f));
      }
    }
    og_grid_refresh();
    wait_frame(&next, idle);
  }
  og_grid_led_all(0);
  og_grid_refresh();
}
