// No-op newlib retargetable locks.
//
// Some arm-none-eabi toolchains (e.g. the Arduino-Pico one PlatformIO
// installs) ship a newlib built with retargetable locking but without the
// default no-op lock functions, so linking fails. The official Arm GNU
// toolchain already provides these same no-ops as weak symbols.
//
// No-ops are correct here because only core 0 uses malloc and stdio;
// core 1 only touches I2C and the multicore-safe event queue (see grid.c).
// Everything is weak so a real implementation, if linked, wins.

struct __lock {
  char unused;
};

#define WEAK __attribute__((weak))

WEAK struct __lock __lock___sinit_recursive_mutex;
WEAK struct __lock __lock___sfp_recursive_mutex;
WEAK struct __lock __lock___atexit_recursive_mutex;
WEAK struct __lock __lock___at_quick_exit_mutex;
WEAK struct __lock __lock___malloc_recursive_mutex;
WEAK struct __lock __lock___env_recursive_mutex;
WEAK struct __lock __lock___tz_mutex;
WEAK struct __lock __lock___dd_hash_mutex;
WEAK struct __lock __lock___arc4random_mutex;

typedef struct __lock *_LOCK_T;

WEAK void __retarget_lock_init(_LOCK_T *lock) { (void)lock; }
WEAK void __retarget_lock_init_recursive(_LOCK_T *lock) { (void)lock; }
WEAK void __retarget_lock_close(_LOCK_T lock) { (void)lock; }
WEAK void __retarget_lock_close_recursive(_LOCK_T lock) { (void)lock; }
WEAK void __retarget_lock_acquire(_LOCK_T lock) { (void)lock; }
WEAK void __retarget_lock_acquire_recursive(_LOCK_T lock) { (void)lock; }
WEAK int __retarget_lock_try_acquire(_LOCK_T lock) {
  (void)lock;
  return 1;
}
WEAK int __retarget_lock_try_acquire_recursive(_LOCK_T lock) {
  (void)lock;
  return 1;
}
WEAK void __retarget_lock_release(_LOCK_T lock) { (void)lock; }
WEAK void __retarget_lock_release_recursive(_LOCK_T lock) { (void)lock; }
