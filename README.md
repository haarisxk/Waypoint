# Waypoint: Kernel-Mode Virtual HID Mouse Driver for Windows

[![License: Custom](https://img.shields.io/badge/License-Custom-blue.svg)](LICENSE)
![Platform: Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-lightgrey.svg)
![Built With: KMDF](https://img.shields.io/badge/Built%20With-KMDF%20v1.15-green.svg)

**Waypoint** is a professional, stealth-focused **kernel-mode virtual mouse driver** designed for Windows 10 and Windows 11. Built utilizing the **Kernel-Mode Driver Framework (KMDF)** and the **Virtual HID Framework (VHF)**, Waypoint provides precise, hardware-level input simulation. It operates entirely at the ring-0 kernel level, effectively bypassing the need for traditional user-mode API hooking, memory manipulation, or code injection.

---



## Overview & Core Features

- **KMDF Virtual HID Mouse implementation**: Leverages modern WDF architectures to instantiate a fully compliant virtual HID device supporting relative X/Y mouse movement, scroll wheel, and standard multi-button clicks.
- **Hardware Identity Spoofing & Obfuscation**: Dynamically randomizes hardware IDs (Vendor ID / Product ID) and USB string descriptors on every system boot. This allows the virtual device to seamlessly spoof and emulate legitimate peripherals (e.g., Logitech, Razer, Corsair mice) for extreme stealth.
- **Stealth Timing & Entropy (Anti-Detection)**: Features advanced kernel-side timing jitter and randomized micro-delays. HID report timestamps perfectly mirror the messy, realistic hardware interrupt behavior of physical mouse sensors, defeating heuristic analysis by avoiding static polling patterns and occasionally dropping polling cycles.
- **Covert Shared-Memory IPC**: Uses a high-performance, lockless ring buffer allocated in non-paged pool memory, mapped directly to user-space via MDL. This achieves zero-latency inter-process communication (IPC) between user-mode applications and the kernel driver, completely avoiding the easily detectable footprint of continuous IOCTL dispatching.

---

## Primary Use Cases

- **Automated UI Testing & QA**: Accurately simulate programmatic hardware-level mouse inputs for software testing pipelines where standard application-level inputs (such as `SendInput` or `mouse_event`) fail, drop, or are blocked by the target application's security.
- **Accessibility & Assistive Technology**: Act as a highly reliable, hardware-level abstraction layer for custom physical controllers, eye-tracking/head-tracking software, and bespoke assistive interfaces that must interact with the Windows OS natively.
- **Remote Desktop & Cloud Virtualization**: Provide immediate, low-latency absolute and relative mouse inputs crucial for custom remote desktop solutions (RDP), cloud gaming environments, and hypervisor virtualization.
- **Machine Learning & AI Automation**: Empower computer vision models, robotic process automation (RPA) tools, and AI agents to organically navigate the operating system and complex interfaces using indistinguishable, human-like mouse movements.

---

## System Requirements

- **Operating System**: Windows 10 or Windows 11 (x64 architectures only)
- **Development Environment**: Visual Studio 2022 (with Desktop development with C++)
- **Driver Kit**: Windows Driver Kit (WDK) `10.0.26100.0` (or the specific version matching your Windows SDK)

---

## Installation & Setup

> [!WARNING]
> **Important Notice:** The Waypoint Driver utilizes a **self-signed** custom Root Certificate Authority. Before the Windows driver stack can successfully load the `.sys` binary, you must install the provided code-signing certificate into the Trusted Root Certification Authorities store and explicitly enable Windows Test Signing mode.

### Step 1: Install the Certificate
Ensure the `Waypoint.cer` code-signing certificate is trusted by the operating system.
1. Right-click the automated installation script and select **Run as Administrator**:
   ```cmd
   cert\Install-Certificate.cmd
   ```

### Step 2: Enable Test Mode
To permit the loading of self-signed kernel drivers, configure the Windows bootloader:
1. Open an **Administrator** Command Prompt.
2. Execute the following command:
   ```cmd
   bcdedit /set testsigning on
   ```
3. **Reboot your system** to apply the configuration.

### Step 3: Install the Driver
1. Open an **Administrator** Command Prompt in the repository's root directory.
2. Use the Windows Device Console utility (`devcon.exe`) to install the `.inf` driver package:
   ```cmd
   "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe" update "x64\Release\Waypoint.inf" "Root\WaypointHID"
   ```

### Step 4: Run the Controller
Launch the user-mode configuration and demonstration application to begin interacting with the driver:
```cmd
x64\Release\WaypointCtl.exe
```

---

## Building from Source

To compile the Waypoint driver binary (`.sys`) and controller from source code, ensure Visual Studio 2022 and the WDK are properly configured.

Launch a command prompt in the root directory and execute the automated build script:
```cmd
build.cmd
```
*(This batch script utilizes MSBuild, Inf2Cat, and SignTool to automate the compilation, catalog generation, and digital code-signing processes.)*

---

## Directory Structure

```text
Waypoint/
├── cert/         # Certificate installation scripts and PKI public keys
├── controller/   # User-mode interface application (.exe) C source code
├── driver/       # Kernel-mode KMDF driver (.sys) C source code
├── shared/       # Shared C headers defining the IPC communication structures
└── tools/        # Additional PowerShell utility scripts (e.g. certificate generation)
```

---

## Support & Feedback

If you encounter any compilation errors, driver crashes (BSOD), or runtime bugs, please open a ticket on the [GitHub Issues tab](https://github.com/haarisxk/Waypoint/issues). 

> [!TIP]
> When reporting an issue, please include your Windows OS version, WDK version, terminal output logs, or any associated bugcheck stop codes to expedite troubleshooting!

---

## Disclaimer

**Developer**: Haaris Khan

Users are free to utilize this software however they choose, but they do so entirely at their own risk. The developer assumes no liability for how this software is implemented or deployed. If this software is utilized in competitive video games or alongside anti-cheat systems and results in account bans, penalties, or damages, that is solely the responsibility of the end user.
