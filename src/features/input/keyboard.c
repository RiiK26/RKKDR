#include "keyboard.h"

#include "../../rkkdr_common.h"

#include <linux/fs.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#define LOG_PREFIX "[[KRNL]Keyboard]: "

// virtual keyboard device
static struct input_dev* vkeyboard_dev;

/* ============================================================
 * Character device file operations (/dev/rkkdr_keyboard)
 * ============================================================ */

static ssize_t rkkdr_keyboard_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
  struct rkkdr_keyboard_cmd cmd;

  if (count != sizeof(cmd)) {
    pr_warn(LOG_PREFIX "Invalid command size: %zu (expected %zu)\n", count, sizeof(cmd));
    return -EINVAL;
  }

  if (copy_from_user(&cmd, buf, sizeof(cmd)))
    return -EFAULT;

  switch (cmd.type) {
  case RKKDR_KEYBOARD_KEY :
    pr_debug(LOG_PREFIX "Keyboard: code=%u value=%d\n", cmd.code, cmd.value);
    input_report_key(vkeyboard_dev, cmd.code, cmd.value);
    input_sync(vkeyboard_dev);
    break;

  default :
    pr_warn(LOG_PREFIX "Unknown command type: %u\n", cmd.type);
    return -EINVAL;
  }

  return sizeof(cmd);
}

static int rkkdr_keyboard_open(struct inode* inode, struct file* file)
{
  pr_info(LOG_PREFIX "Device opened by userspace (pid=%d)\n", current->pid);
  return 0;
}

static int rkkdr_keyboard_release(struct inode* inode, struct file* file)
{
  pr_info(LOG_PREFIX "Device closed by userspace (pid=%d)\n", current->pid);
  return 0;
}

static const struct file_operations rkkdr_keyboard_fops = {
  .owner   = THIS_MODULE,
  .write   = rkkdr_keyboard_write,
  .open    = rkkdr_keyboard_open,
  .release = rkkdr_keyboard_release,
};

static struct miscdevice rkkdr_keyboard_misc = {
  .minor = MISC_DYNAMIC_MINOR,
  .name  = "rkkdr_keyboard",
  .fops  = &rkkdr_keyboard_fops,
};

/* ============================================================
 * Feature init / exit
 * ============================================================ */

int vkeyboard_init(void)
{
  int error;
  int i;

  // 1. Allocate virtual input device
  vkeyboard_dev = input_allocate_device();
  if (!vkeyboard_dev) {
    pr_err(LOG_PREFIX "Failed to allocate input device\n");
    return -ENOMEM;
  }

  // 2. Set device properties
  vkeyboard_dev->name       = "RKKDR Virtual Keyboard";
  vkeyboard_dev->id.bustype = BUS_VIRTUAL;
  vkeyboard_dev->id.vendor  = 0x1234;
  vkeyboard_dev->id.product = 0x5678;
  vkeyboard_dev->id.version = 1;

  // 3. Enable keyboard keys (all standard keys up to BTN_MISC boundary)
  set_bit(EV_KEY, vkeyboard_dev->evbit);
  for (i = KEY_ESC; i < BTN_MISC; i++)
    set_bit(i, vkeyboard_dev->keybit);

  // 4. Register the input device with the kernel
  error = input_register_device(vkeyboard_dev);
  if (error) {
    pr_err(LOG_PREFIX "Failed to register input device (err=%d)\n", error);
    input_free_device(vkeyboard_dev);
    vkeyboard_dev = NULL;
    return error;
  }

  // 5. Create /dev/rkkdr_keyboard character device for userspace communication
  error = misc_register(&rkkdr_keyboard_misc);
  if (error) {
    pr_err(LOG_PREFIX "Failed to register /dev/rkkdr_keyboard (err=%d)\n", error);
    input_unregister_device(vkeyboard_dev);
    vkeyboard_dev = NULL;
    return error;
  }

  pr_info(LOG_PREFIX "Initialized.\n");
  pr_info(LOG_PREFIX "Waiting for userspace commands on /dev/rkkdr_keyboard\n");

  return 0;
}

void vkeyboard_exit(void)
{
  misc_deregister(&rkkdr_keyboard_misc);

  if (vkeyboard_dev) {
    input_unregister_device(vkeyboard_dev);
    vkeyboard_dev = NULL;
  }

  pr_info(LOG_PREFIX "Safely shut down.\n");
}
