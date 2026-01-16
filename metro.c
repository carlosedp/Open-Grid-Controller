#include <stdio.h>

#include "pico/critical_section.h"
#include "tusb.h"

#include "metro.h"
#include "vm.h"

#define METRO_USE_CRITICAL_SECTION 0

#if METRO_USE_CRITICAL_SECTION
#define METRO_ENTER_CRITICAL_SECTION(cs) critical_section_enter_blocking(cs)
#define METRO_EXIT_CRITICAL_SECTION(cs) critical_section_exit(cs)
#define METRO_INIT_CRITICAL_SECTION(cs) critical_section_init(cs)
#else
#define METRO_ENTER_CRITICAL_SECTION(cs)
#define METRO_EXIT_CRITICAL_SECTION(cs)
#define METRO_INIT_CRITICAL_SECTION(cs)
#endif

#define METRO_DEBUG_PRINTS 0

// shared state between IRQ and main thread
struct metro_state {
  // the count of ticks remaining to process
  // negative numbers indicate to continue indefinitely
  volatile int count;
  volatile int stages;
  // set when a tick is pending for the main thread
  volatile bool pending;
};

struct metro {
  struct repeating_timer timer;
#if METRO_USE_CRITICAL_SECTION
  critical_section_t *crit_sec;
#endif
  int index;
  int stages;
  bool running;
  struct metro_state state;
};

const struct metro_state metro_state_default = {
    .count = -1,
    .pending = false,
};

struct metro metros[METRO_COUNT];

void metro_init() {
  for (int i = 0; i < METRO_COUNT; ++i) {
    struct metro *m = &metros[i];
    m->index = i;
    m->state = metro_state_default;
    m->running = false;
    METRO_INIT_CRITICAL_SECTION(m->crit_sec);
  }
}

void metro_task() {
  for (int i = 0; i < METRO_COUNT; ++i) {
    struct metro *m = &metros[i];

    if (m->running) {
      bool is_pending = false;
      int current_count = 0;
      int stages;

      //===========================================
      METRO_ENTER_CRITICAL_SECTION(m->crit_sec);
      if (m->state.pending) {
        m->state.pending = false;
        current_count = m->state.count;
        stages = m->state.stages;
        is_pending = true;
      }
      METRO_EXIT_CRITICAL_SECTION(m->crit_sec);
      //===========================================

      if (is_pending) {
        if (current_count == 0) {
          m->running = false;
        }

        if (current_count >= 0) {
          current_count = stages - current_count;
        }

        vm_handle_metro(m->index, current_count);
      }
    }
  }
}

// assumption: will be called from an interrupt handler
bool metro_cb(struct repeating_timer *rt) {
  struct metro *m = rt->user_data;
  struct metro_state *state = &m->state;
  bool should_continue = true;

  //===========================================
  METRO_ENTER_CRITICAL_SECTION(m->crit_sec);
  if (state->count > 0) {
    state->count--;
    should_continue = state->count > 0;
  }
  m->state.pending = true;
  METRO_EXIT_CRITICAL_SECTION(m->crit_sec);
  //===========================================

  return should_continue;
}

// assumption: will be called from the main thread
void metro_set_with_count(int index, double s, int count) {
  struct metro *m = &metros[index];
  if (s > 0) {
    int64_t us = (int64_t)(s * 1000000);
    if (m->running) {
      printf("metro already running\r\n");
      m->timer.delay_us = (int64_t)us;
    } else {
      bool ok = add_repeating_timer_us(us, &metro_cb, m, &m->timer);

      if (!ok) {
        const char *str = "no more timers!";
        tud_cdc_n_write(0, str, strlen(str));
        tud_cdc_n_write_flush(0);
      }

      METRO_ENTER_CRITICAL_SECTION(m->crit_sec);
      m->state = metro_state_default;
      m->state.count = count;
      m->state.stages = count;
      METRO_EXIT_CRITICAL_SECTION(m->crit_sec);

      m->running = true;
    }
  } else {
    if (m->running) {
#if METRO_DEBUG_PRINTS
      printf("metro(%d) stopped; metro address = 0x%08x, timer address = "
             "0x%08x\r\n",
             index, (unsigned int)m, (unsigned int)&m->timer);
#endif
      cancel_repeating_timer(&m->timer);
      m->running = false;
    }
  }
}

void metro_set(int index, double s) { metro_set_with_count(index, s, -1); }

void metro_cleanup() {
  for (int i = 0; i < METRO_COUNT; ++i) {
    struct metro *m = &metros[i];
    if (m->running) {
      cancel_repeating_timer(&m->timer);
      m->running = false;
    }
  }
}
