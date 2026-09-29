#include "autoclicker.h"

#include "../input/mouse.h"

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/workqueue.h>

// Module parameters (these create files in
// /sys/module/RKKDR/parameters/)
static bool enable = false;
module_param(enable, bool, 0644);
MODULE_PARM_DESC(enable, "Enable the auto-clicker (1 = ON, 0 = OFF)");

static int interval_ms = 100;
module_param(interval_ms, int, 0644);
MODULE_PARM_DESC(interval_ms, "Interval between clicks in milliseconds");

static bool hold_click = false;
module_param(hold_click, bool, 0644);
MODULE_PARM_DESC(hold_click, "Keep the left mouse button held while enabled");

// The background worker that will fire the clicks
static struct delayed_work click_work;
static bool                click_held = false;

// This function runs in the background and simulates the click
static void autoclick_worker_func(struct work_struct* work)
{
  if (enable && hold_click) {
    if (!click_held) {
      rkkdr_send_mouse_btn_event(BTN_LEFT, 1);
      click_held = true;
    }
  }
  else if (click_held) {
    rkkdr_send_mouse_btn_event(BTN_LEFT, 0);
    click_held = false;
  }
  else if (enable) {
    // Send Mouse DOWN
    rkkdr_send_mouse_btn_event(BTN_LEFT, 1);

    // Small delay to simulate human click duration
    msleep(20);

    // Send Mouse UP
    rkkdr_send_mouse_btn_event(BTN_LEFT, 0);
  }

  // Safety limit: if the interval is too small, it could lock up the kernel
  // worker thread
  int safe_interval = interval_ms;
  if (safe_interval < 10) {
    safe_interval = 10;
  }

  // Schedule the next click (if enabled, otherwise sleep and check again)
  // Even if disabled, we keep the loop alive so it can wake up when 'enable' is
  // changed
  int wait_time = enable ? safe_interval : 500;
  schedule_delayed_work(&click_work, msecs_to_jiffies(wait_time));
}

int autoclicker_init(void)
{
  // Start the background clicking loop
  INIT_DELAYED_WORK(&click_work, autoclick_worker_func);
  schedule_delayed_work(&click_work, msecs_to_jiffies(500));

  pr_info("[[KRNL]AutoClicker]: initialized. Disabled by default.\n");
  return 0;
}

void autoclicker_exit(void)
{
  // Stop the background worker
  cancel_delayed_work_sync(&click_work);

  if (click_held) {
    rkkdr_send_mouse_btn_event(BTN_LEFT, 0);
    click_held = false;
  }

  pr_info("[[KRNL]AutoClicker]: safely shut down.\n");
}
