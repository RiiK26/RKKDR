#include "features/autoclicker/autoclicker.h"
#include "features/battery/battery.h"
#include "features/input/keyboard.h"
#include "features/input/mouse.h"
#include "features/screen/screen.h"

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Syakir");
MODULE_DESCRIPTION("RKKDR - Kernel-level input automation & screen access");
MODULE_VERSION("0.3");

static int __init my_kernel_driver_init(void)
{
  int ret;

  printk(KERN_INFO "[KERNEL_DRIVER]: Module loading...\n");

  ret = rkkdr_battery_init();
  if (ret && ret != -ENODEV) {
    pr_err("[KERNEL_DRIVER]: rkkdr_battery_init failed (%d)\n", ret);
    return ret;
  }

  ret = autoclicker_init();
  if (ret) {
    pr_err("[KERNEL_DRIVER]: autoclicker_init failed (%d)\n", ret);
    rkkdr_battery_exit();
    return ret;
  }

  ret = vkeyboard_init();
  if (ret) {
    pr_err("[KERNEL_DRIVER]: vkeyboard_init failed (%d)\n", ret);
    autoclicker_exit();
    rkkdr_battery_exit();
    return ret;
  }

  ret = vmouse_init();
  if (ret) {
    pr_err("[KERNEL_DRIVER]: vmouse_init failed (%d)\n", ret);
    vkeyboard_exit();
    autoclicker_exit();
    rkkdr_battery_exit();
    return ret;
  }

  ret = screen_init();
  if (ret) {
    pr_err("[KERNEL_DRIVER]: screen_init failed (%d)\n", ret);
    vmouse_exit();
    vkeyboard_exit();
    autoclicker_exit();
    rkkdr_battery_exit();
    return ret;
  }

  printk(KERN_INFO "[KERNEL_DRIVER]: All features initialized successfully and ready to use.\n");
  return 0;
}

static void __exit my_kernel_driver_exit(void)
{
  screen_exit();
  vmouse_exit();
  vkeyboard_exit();
  autoclicker_exit();
  rkkdr_battery_exit();
  printk(KERN_INFO "[KERNEL_DRIVER]: Module unloaded.\n");
}

module_init(my_kernel_driver_init);
module_exit(my_kernel_driver_exit);
