#include "screen.h"

#include "../../rkkdr_common.h"

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/screen_info.h>
#include <linux/sysfb.h>
#include <linux/uaccess.h>

#define LOG_PREFIX "[[KRNL]Screen]: "

// Cached screen info — refreshed on each read/ioctl
static struct rkkdr_screen_info cached_info;
static bool                     fb_available;

/*
 * Read screen info from sysfb_primary_display.
 * This is the modern kernel 7.x way to access display information,
 * replacing the deprecated registered_fb[] array.
 *
 * sysfb_primary_display contains the boot-time screen_info struct which
 * holds the linear framebuffer dimensions set up by EFI/BIOS/DRM.
 */
static void refresh_screen_info(void)
{
  struct screen_info* si = &sysfb_primary_display.screen;

  // Check if framebuffer info is valid
  if (si->lfb_width == 0 || si->lfb_height == 0) {
    fb_available = false;
    return;
  }

  cached_info.width          = si->lfb_width;
  cached_info.height         = si->lfb_height;
  cached_info.virtual_width  = si->lfb_width;
  cached_info.virtual_height = si->lfb_height;
  cached_info.bpp            = si->lfb_depth;
  cached_info.line_length    = si->lfb_linelength;
  cached_info.fb_size        = __screen_info_lfb_size(si, screen_info_video_type(si));

  fb_available               = true;
}

/* ============================================================
 * Character device file operations (/dev/rkkdr_screen)
 * ============================================================ */

static ssize_t rkkdr_screen_read(struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
  pr_debug(LOG_PREFIX "Read request from userspace (pid=%d, count=%zu)\n", current->pid, count);

  // Refresh on each read
  refresh_screen_info();

  if (!fb_available) {
    pr_warn(LOG_PREFIX "No framebuffer info available\n");
    return -ENODEV;
  }

  // Standard read semantics with offset tracking
  if (*ppos >= sizeof(cached_info))
    return 0;

  if (count > sizeof(cached_info) - *ppos)
    count = sizeof(cached_info) - *ppos;

  if (copy_to_user(buf, (char*) &cached_info + *ppos, count))
    return -EFAULT;

  *ppos += count;

  pr_debug(LOG_PREFIX "Sent screen info: %ux%u @ %ubpp\n", cached_info.width, cached_info.height, cached_info.bpp);

  return count;
}

static long rkkdr_screen_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
  pr_debug(LOG_PREFIX "ioctl request from userspace (pid=%d, cmd=0x%x)\n", current->pid, cmd);

  switch (cmd) {
  case RKKDR_SCREEN_GET_INFO :
    refresh_screen_info();
    if (!fb_available) {
      pr_warn(LOG_PREFIX "ioctl GET_INFO: no framebuffer info available\n");
      return -ENODEV;
    }
    if (copy_to_user((void __user*) arg, &cached_info, sizeof(cached_info)))
      return -EFAULT;
    pr_debug(
      LOG_PREFIX "ioctl GET_INFO: %ux%u @ %ubpp "
                 "(line_length: %u, fb_size: %llu)\n",
      cached_info.width, cached_info.height, cached_info.bpp, cached_info.line_length, cached_info.fb_size
    );
    return 0;

  default :
    pr_warn(LOG_PREFIX "Unknown ioctl cmd: 0x%x\n", cmd);
    return -ENOTTY;
  }
}

static int rkkdr_screen_open(struct inode* inode, struct file* file)
{
  pr_info(LOG_PREFIX "Device opened by userspace (pid=%d)\n", current->pid);
  return 0;
}

static int rkkdr_screen_release(struct inode* inode, struct file* file)
{
  pr_info(LOG_PREFIX "Device closed by userspace (pid=%d)\n", current->pid);
  return 0;
}

static const struct file_operations rkkdr_screen_fops = {
  .owner          = THIS_MODULE,
  .read           = rkkdr_screen_read,
  .unlocked_ioctl = rkkdr_screen_ioctl,
  .open           = rkkdr_screen_open,
  .release        = rkkdr_screen_release,
};

static struct miscdevice rkkdr_screen_misc = {
  .minor = MISC_DYNAMIC_MINOR,
  .name  = "rkkdr_screen",
  .fops  = &rkkdr_screen_fops,
};

/* ============================================================
 * Feature init / exit
 * ============================================================ */

int screen_init(void)
{
  int error;

  // Try to read framebuffer info on load
  refresh_screen_info();

  if (fb_available) {
    pr_info(
      LOG_PREFIX "Display detected: %ux%u @ %ubpp "
                 "(line_length: %u, fb_size: %llu bytes)\n",
      cached_info.width, cached_info.height, cached_info.bpp, cached_info.line_length, cached_info.fb_size
    );
  }
  else {
    pr_warn(
      LOG_PREFIX "No display info detected at init. "
                 "Screen reading will be unavailable.\n"
    );
  }

  // Register character device regardless
  error = misc_register(&rkkdr_screen_misc);
  if (error) {
    pr_err(LOG_PREFIX "Failed to register /dev/rkkdr_screen (err=%d)\n", error);
    return error;
  }

  pr_info(LOG_PREFIX "Initialized. Waiting for userspace commands on /dev/rkkdr_screen\n");

  return 0;
}

void screen_exit(void)
{
  misc_deregister(&rkkdr_screen_misc);
  pr_info(LOG_PREFIX "Safely shut down.\n");
}
