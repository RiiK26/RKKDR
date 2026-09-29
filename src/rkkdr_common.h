/*
 * RKKDR Common Header
 * Shared definitions between kernel module and userspace programs.
 *
 * This header uses Linux UAPI types (__u32, __s32, etc.) which work
 * in both kernel space and userspace on Linux.
 */
#ifndef RKKDR_COMMON_H
#define RKKDR_COMMON_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* ============================================================
 * Keyboard Command Interface (/dev/rkkdr_keyboard)
 * ============================================================ */

/* Keyboard command types */
#define RKKDR_KEYBOARD_KEY 0 /* Keyboard key press/release */

/*
 * Command struct written to /dev/rkkdr_keyboard
 *
 * For RKKDR_KEYBOARD_KEY:
 *   code  = Linux key code (KEY_A, KEY_ENTER, etc.)
 *   value = 1 for press, 0 for release
 */
struct rkkdr_keyboard_cmd
{
  __u32 type;
  __u32 code;
  __s32 value;
};

/* ============================================================
 * Mouse Command Interface (/dev/rkkdr_mouse)
 * ============================================================ */

/* Mouse command types */
#define RKKDR_MOUSE_REL    1 /* Relative mouse movement */
#define RKKDR_MOUSE_ABS    2 /* Absolute mouse positioning */
#define RKKDR_MOUSE_BTN    3 /* Mouse button press/release */
#define RKKDR_MOUSE_SCROLL 4 /* Mouse scroll wheel */

/*
 * Command struct written to /dev/rkkdr_mouse
 *
 * For RKKDR_MOUSE_REL:
 *   code  = unused (set to 0)
 *   value = X delta (positive = right, negative = left)
 *   extra = Y delta (positive = down, negative = up)
 *
 * For RKKDR_MOUSE_ABS:
 *   code  = unused (set to 0)
 *   value = X position (0 to screen width)
 *   extra = Y position (0 to screen height)
 *
 * For RKKDR_MOUSE_BTN:
 *   code  = button code (BTN_LEFT, BTN_RIGHT, BTN_MIDDLE)
 *   value = 1 for press, 0 for release
 *   extra = unused (set to 0)
 *
 * For RKKDR_MOUSE_SCROLL:
 *   code  = unused (set to 0)
 *   value = vertical scroll (positive = up, negative = down)
 *   extra = horizontal scroll (positive = right, negative = left)
 */
struct rkkdr_mouse_cmd
{
  __u32 type;
  __u32 code;
  __s32 value;
  __s32 extra;
};

/* ============================================================
 * Screen Info Interface (/dev/rkkdr_screen)
 * ============================================================ */

struct rkkdr_screen_info
{
  __u32 width;
  __u32 height;
  __u32 virtual_width;
  __u32 virtual_height;
  __u32 bpp;
  __u32 line_length;
  __u64 fb_size;
};

// ioctl magic number for RKKDR
#define RKKDR_IOC_MAGIC 'R'

// Screen ioctls
#define RKKDR_SCREEN_GET_INFO _IOR(RKKDR_IOC_MAGIC, 1, struct rkkdr_screen_info)

#endif /* RKKDR_COMMON_H */
