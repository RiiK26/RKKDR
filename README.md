# RKKDR (Kernel Driver)

![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-green.svg)

**RKKDR** is a custom, modular Linux Kernel Driver designed to expose low-level hardware control directly to userspace applications. 

By operating in pure kernel-space, RKKDR completely bypasses standard software boundaries (like X11/Wayland hooks). This allows external applications to simulate true physical hardware inputs and interface directly with motherboard firmware (WMI/ACPI), making its actions virtually undetectable by software monitors.

> **⚠️ PLATFORM NOTICE:**
> This driver interacts directly with the Linux kernel subsystems (`<linux/input.h>`, WMI, ACPI) and relies on `sysfs` for userspace communication. 
> **It is ONLY compatible with Linux environments.** It is currently developed and tested on **Arch Linux**.

---

## Features & Projects

The RKKDR kernel module serves as the hardware backend for several independent standalone projects. 

For detailed usage, GUIs, and specific documentation, please visit their respective GitHub repositories:

- **[AutoClicker](https://github.com/RiiK26/AutoClicker)**: A blazing fast AutoClicker that simulates physical mouse inputs.
- **[AcerBattery](https://github.com/RiiK26/AcerBattery)**: A CLI controller that interacts with the Embedded Controller (EC) on Acer laptops to enforce physical battery charge limits (80% health mode) via WMI.

### For future Feature and Project available will be listed above here
---

## Compilation & Usage

This repository is strictly the kernel driver backend. 

### Requirements
You will need the kernel headers for your specific Linux distribution and standard build tools.
On **Arch Linux**:
```bash
sudo pacman -S linux-headers base-devel
```

### Building the Driver
To compile and inject the kernel driver into memory:
```bash
make load
```
*(Compiled binaries will be output cleanly to the `release/` directory).*

To unload the driver from memory when finished:
```bash
make unload
```

---

## License

This project is licensed under the **GNU General Public License v3.0**. See the [LICENSE](LICENSE) file for details.
