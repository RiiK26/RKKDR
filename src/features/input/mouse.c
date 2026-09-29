#include "mouse.h"

#include "../../rkkdr_common.h"

#include <linux/fs.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#define LOG_PREFIX "[[KRNL]Mouse]: "

/* Module parameters for absolute positioning bounds */
static int abs_max_x = 1920;
module_param(abs_max_x, int, 0644);
MODULE_PARM_DESC(abs_max_x, "Maximum X coordinate for absolute mouse positioning");

static int abs_max_y = 1080;
module_param(abs_max_y, int, 0644);
MODULE_PARM_DESC(abs_max_y, "Maximum Y coordinate for absolute mouse positioning");

// virtual mouse
static struct input_dev* vmouse_dev;

/* ============================================================
 * Character device file operations (/dev/rkkdr_mouse)
 * ============================================================ */

static ssize_t rkkdr_mouse_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
  struct rkkdr_mouse_cmd cmd;

  if (count != sizeof(cmd)) {
    pr_warn(LOG_PREFIX "Invalid command size: %zu (expected %zu)\n", count, sizeof(cmd));
    return -EINVAL;
  }

  if (copy_from_user(&cmd, buf, sizeof(cmd)))
    return -EFAULT;

  switch (cmd.type) {
  case RKKDR_MOUSE_REL :
    pr_debug(LOG_PREFIX "Mouse REL: dx=%d dy=%d\n", cmd.value, cmd.extra);
    input_report_rel(vmouse_dev, REL_X, cmd.value);
    input_report_rel(vmouse_dev, REL_Y, cmd.extra);
    input_sync(vmouse_dev);
    break;

  case RKKDR_MOUSE_ABS :
    pr_debug(LOG_PREFIX "Mouse ABS: x=%d y=%d\n", cmd.value, cmd.extra);
    input_report_abs(vmouse_dev, ABS_X, cmd.value);
    input_report_abs(vmouse_dev, ABS_Y, cmd.extra);
    input_sync(vmouse_dev);
    break;

  case RKKDR_MOUSE_BTN :
    pr_debug(LOG_PREFIX "Mouse BTN: code=%u value=%d\n", cmd.code, cmd.value);
    input_report_key(vmouse_dev, cmd.code, cmd.value);
    input_sync(vmouse_dev);
    break;

  case RKKDR_MOUSE_SCROLL :
    pr_debug(LOG_PREFIX "Mouse SCROLL: vert=%d horiz=%d\n", cmd.value, cmd.extra);
    if (cmd.value)
      input_report_rel(vmouse_dev, REL_WHEEL, cmd.value);
    if (cmd.extra)
      input_report_rel(vmouse_dev, REL_HWHEEL, cmd.extra);
    input_sync(vmouse_dev);
    break;

  default :
    pr_warn(LOG_PREFIX "Unknown command type: %u\n", cmd.type);
    return -EINVAL;
  }

  return sizeof(cmd);
}

static int rkkdr_mouse_open(struct inode* inode, struct file* file)
{
  pr_info(LOG_PREFIX "Device opened by userspace (pid=%d)\n", current->pid);
  return 0;
}

static int rkkdr_mouse_release(struct inode* inode, struct file* file)
{
  pr_info(LOG_PREFIX "Device closed by userspace (pid=%d)\n", current->pid);
  return 0;
}

static const struct file_operations rkkdr_mouse_fops = {
  .owner   = THIS_MODULE,
  .write   = rkkdr_mouse_write,
  .open    = rkkdr_mouse_open,
  .release = rkkdr_mouse_release,
};

static struct miscdevice rkkdr_mouse_misc = {
  .minor = MISC_DYNAMIC_MINOR,
  .name  = "rkkdr_mouse",
  .fops  = &rkkdr_mouse_fops,
};

/* ============================================================
 * Feature init / exit
 * ============================================================ */

int vmouse_init(void)
{
  int error;

  // 1. Allocate virtual input device
  vmouse_dev = input_allocate_device();
  if (!vmouse_dev) {
    pr_err(LOG_PREFIX "Failed to allocate input device\n");
    return -ENOMEM;
  }

  // 2. Set device properties
  vmouse_dev->name       = "RKKDR Virtual Mouse";
  vmouse_dev->id.bustype = BUS_VIRTUAL;
  vmouse_dev->id.vendor  = 0x1234;
  vmouse_dev->id.product = 0x5678;
  vmouse_dev->id.version = 1;

  // 3. Mouse buttons
  set_bit(EV_KEY, vmouse_dev->evbit);
  set_bit(BTN_LEFT, vmouse_dev->keybit);
  set_bit(BTN_RIGHT, vmouse_dev->keybit);
  set_bit(BTN_MIDDLE, vmouse_dev->keybit);
  set_bit(BTN_SIDE, vmouse_dev->keybit);
  set_bit(BTN_EXTRA, vmouse_dev->keybit);

  // 4. Relative mouse movement (move by delta)
  set_bit(EV_REL, vmouse_dev->evbit);
  set_bit(REL_X, vmouse_dev->relbit);
  set_bit(REL_Y, vmouse_dev->relbit);
  set_bit(REL_WHEEL, vmouse_dev->relbit);
  set_bit(REL_HWHEEL, vmouse_dev->relbit);

  // 5. Absolute mouse positioning (move to exact coordinate)
  set_bit(EV_ABS, vmouse_dev->evbit);
  input_set_abs_params(vmouse_dev, ABS_X, 0, abs_max_x, 0, 0);
  input_set_abs_params(vmouse_dev, ABS_Y, 0, abs_max_y, 0, 0);

  // 6. Register the input device with the kernel
  error = input_register_device(vmouse_dev);
  if (error) {
    pr_err(LOG_PREFIX "Failed to register input device (err=%d)\n", error);
    input_free_device(vmouse_dev);
    vmouse_dev = NULL;
    return error;
  }

  // 7. Create /dev/rkkdr_mouse character device for userspace communication
  error = misc_register(&rkkdr_mouse_misc);
  if (error) {
    pr_err(LOG_PREFIX "Failed to register /dev/rkkdr_mouse (err=%d)\n", error);
    input_unregister_device(vmouse_dev);
    vmouse_dev = NULL;
    return error;
  }

  pr_info(LOG_PREFIX "Initialized. Virtual mouse (rel + abs [%dx%d]).\n", abs_max_x, abs_max_y);
  pr_info(LOG_PREFIX "Waiting for userspace commands on /dev/rkkdr_mouse\n");

  return 0;
}

void vmouse_exit(void)
{
  misc_deregister(&rkkdr_mouse_misc);

  if (vmouse_dev) {
    input_unregister_device(vmouse_dev);
    vmouse_dev = NULL;
  }

  pr_info(LOG_PREFIX "Safely shut down.\n");
}

void rkkdr_send_mouse_btn_event(unsigned int code, int value)
{
  if (vmouse_dev) {
    input_report_key(vmouse_dev, code, value);
    input_sync(vmouse_dev);
  }
}
