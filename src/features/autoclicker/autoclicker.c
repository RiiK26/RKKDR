#include "autoclicker.h"

#include "../input/mouse.h"

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/workqueue.h>

// Module parameters (these create files in
// /sys/module/RKKDR/parameters/)
static bool enable      = false;
static int  interval_ms = 100;
static bool hold_click  = false;

// The background worker that will fire the clicks
static struct delayed_work click_work;
static bool                click_held      = false;
static bool                autoclick_ready = false;

static int autoclick_set_bool(const char* val, const struct kernel_param* kp)
{
  int ret = param_set_bool(val, kp);

  if (!ret && READ_ONCE(autoclick_ready)) {
    mod_delayed_work(system_wq, &click_work, 0);
  }

  return ret;
}

static const struct kernel_param_ops autoclick_bool_ops = {
  .set = autoclick_set_bool,
  .get = param_get_bool,
};

module_param_cb(enable, &autoclick_bool_ops, &enable, 0644);
MODULE_PARM_DESC(enable, "Enable the auto-clicker (1 = ON, 0 = OFF)");

module_param(interval_ms, int, 0644);
MODULE_PARM_DESC(interval_ms, "Interval between clicks in milliseconds");

module_param_cb(hold_click, &autoclick_bool_ops, &hold_click, 0644);
MODULE_PARM_DESC(hold_click, "Keep the left mouse button held while enabled");

// This function runs in the background and simulates the click
static void autoclick_worker_func(struct work_struct* work)
{
  bool enabled = READ_ONCE(enable);
  bool holding = READ_ONCE(hold_click);

  if (enabled && holding) {
    if (!click_held) {
      rkkdr_send_mouse_btn_event(BTN_LEFT, 1);
      click_held = true;
    }
  }
  else if (click_held) {
    rkkdr_send_mouse_btn_event(BTN_LEFT, 0);
    click_held = false;
  }
  else if (enabled) {
    // Send Mouse DOWN
    rkkdr_send_mouse_btn_event(BTN_LEFT, 1);

    // Small delay to simulate human click duration
    msleep(20);

    // Send Mouse UP
    rkkdr_send_mouse_btn_event(BTN_LEFT, 0);
  }

  // Safety limit: if the interval is too small, it could lock up the kernel
  // worker thread
  int safe_interval = READ_ONCE(interval_ms);
  if (safe_interval < 10) {
    safe_interval = 10;
  }

  if (enabled && !holding) {
    schedule_delayed_work(&click_work, msecs_to_jiffies(safe_interval));
  }
}

int autoclicker_init(void)
{
  // Start the background clicking loop
  INIT_DELAYED_WORK(&click_work, autoclick_worker_func);
  WRITE_ONCE(autoclick_ready, true);
  schedule_delayed_work(&click_work, 0);

  pr_info("[[KRNL]AutoClicker]: initialized. Disabled by default.\n");
  return 0;
}

void autoclicker_exit(void)
{
  // Stop the background worker
  WRITE_ONCE(autoclick_ready, false);
  cancel_delayed_work_sync(&click_work);

  if (click_held) {
    rkkdr_send_mouse_btn_event(BTN_LEFT, 0);
    click_held = false;
  }

  pr_info("[[KRNL]AutoClicker]: safely shut down.\n");
}
