[<img width="200" height="67" alt="ChatGPT Image Aug 12, 2026, 02_04_12 PM" img align="right"  src="https://github.com/user-attachments/assets/7eb7b583-5d57-4884-ae90-ad7127d733c4" alt="Right aligned" />](https://ko-fi.com/O8Z424G15Y)

# EmulationStation davmod — dArkOSen Edition fcamod

![Platform](https://img.shields.io/badge/Platform-R36S-blue)
![OS](https://img.shields.io/badge/OS-dArkOSen-green)
![Shell](https://img.shields.io/badge/Bash-Script-yellow)
![License](https://img.shields.io/badge/License-Free-lightgrey)

**_Dev\_Note:_** — _@Jason_2x truly made this project possible by showing me the way. Without his assistance it would have 1/10th the features and I'd still be compiling on device!_

A custom fork of [christianhaitian/EmulationStation-fcamod](https://github.com/christianhaitian/EmulationStation-fcamod) (branch `351v`) targeting **dArkOSen** on the **R36S** handheld.
Built automatically via GitHub Actions using the official Mali RK3326 libraries.

---

## ✨ What's new compared to upstream

### **_SYSTEM WIDE_**
- added Batocera/AmberElec theme support!
  - fully backwards compatible with existing themes
  - if using a theme with OS choices (ie ArkOS, Batocera, Rockinx) always choose Batocera, Amberelec or Pan4elec, not ArkOS
  - tested on over 52 themes including OGA and fcamod originals
  - use [ThemeGallery](https://github.com/djparentx/ThemeGallery/releases) to find supported themes

---

### **Distro Version**
- click Distro Version to check for dArkOSen updates directly from the main menu

---

### 🔋 Customizable Battery icon (by Jason)

- **Icon pack selector** — **START > UI SETTINGS > BATTERY ICON**:

  | Pack | Look |
  |------|------|
  | `Default` | Simple green / red / yellow (charging) icons |
  | `Colorful` | Graduated colors by level (full → good → medium → low → charging) |
  | `Hearts` | Heart-shaped icons |
  | `Stock` | Original monochrome icons from christianhaitian |

- Powered by [knubat/BatteryPlus](https://github.com/Mikhailzrick/knubat.components) — a voltage-based percentage daemon for RK3326 handhelds, with its own settings menu (see below).

---

### 📶🔵 WiFi & Bluetooth icons (by Jason)

- Live status icons in the status bar, each with an **off / active / connected** state (WiFi also has **sharing** and **service** states for SSH/Samba/Filebrowser).
- Icon appearance updates automatically every 5 seconds via a background daemon (`es-status-daemon`) — no polling from ES itself.
- Toggle each icon independently in **START > UI SETTINGS**.
- **Icon pack selector** — **START > UI SETTINGS > NETWORK ICON**: `Default`, `Mario`, `Pokémon`, `Solstice`, `Zelda`.
- Switching either the **battery** or the **network/Bluetooth** icon pack applies **instantly**, with no EmulationStation restart required.
- Status bar spacing is unified: the gap between the WiFi, Bluetooth and battery icons, and between the battery icon and its percentage, all match.

---

### 🖥️ Display Settings
- **Gamma slider** — adjust screen gamma (0.4 → 1.8) in real time, in **START > DISPLAY SETTINGS**.
- **Temperature slider** — adjust screen temperature globally, affects all apps and emnulators
- **Night Filter** — a second screen temperature setting that runs on a schedule
- Mipmap rendering fix for crisp status bar icons at any size.

---

### 🖥️ UI Settings
- **Play Timer** — set to OFF/15/30/45/60/90/120 minutes
  - tracks actual game play, not screen on time
  - when the timer ends it auto-saves (when possible) and exits the game
  - launching a game after the limit is reached shows "Come back tomorrow!"
  - popup warning in Retroarch 5 minutes before the limit is reached
  - vibration warning every minute of last 5 minutes
  - [optional] audio warning every minute of the last five minutes
  - when enabled routes audio through dmix, may affect performance
  - ducks the game volume so you can hear it clearly
  - daily timer resets at midnight
  - controls are hidden in kiosk or kids mode

---

### 📅 Date & Time
- Real-time clock in **START > ADVANCED SETTINGS > DATE & TIME**.
- Set date, time and timezone directly from ES.

---

### ⚡ Performance Settings w/Benchmarking
New menu — **START > PERFORMANCE SETTINGS**:

| Entry | Description |
|-------|-------------|
| **CPU Grade** | Silicon quality as read by the kernel |
| **CPU Temp** | Real-time CPU temperature |
| **CPU Cores** | Enable/disable CPU cores on the fly (1–4) |
| **CPU Governor** | performance / ondemand / schedutil / powersave |
| **CPU Freq** | Set CPU max frequency |
| **GPU Freq** | Set GPU max frequency |
| **RAM Freq** | Set RAM max frequency |
| **ZRAM Size** | Enable and set ZRAM |

- all frequency settings can be set to restore after boot

---

### 🌐 Network Settings menu (w/help from Jason)
New menu between **UI Settings** and **Sound Settings** — **START > NETWORK SETTINGS**:
- Tuned NetworkManager config (IPv6 off, background scan, buffer sizes) and a PSK fix for persistent WiFi connections

| Entry | Description |
|-------|-------------|
| **Hostname** | Editable; shown only when SSH or Samba is active |
| **IP Address** | Shown only when WiFi is connected |
| **Gateway** | Shown only when WiFi is connected |
| **DNS** | Shown only when WiFi is connected |
| **Wifi Enabled** | Toggle WiFi on/off instantly |
| **Scan WiFi Networks** | Scan for nearby WiFi networks |
| **Activate Existing Connection** | Connect to a known or saved network |
| **Delete Existing Connection** | Delete a known or saved network |
| **Remote Services** | Toggle remote sharing on/off near instantly |
| **Remote Services Auto-Start** | Start remote services at boot |
| **Root Samba Access** | Allows root access to files via SMB sharing |
| **WiFi Monitor Service** | A WiFi keep alive service that also turns off WiFi if disconnected for a long period |
| **Bluetooth Manager** | Launches `/opt/system/BT Manager.sh` |

---

### 🎮 Last 20 Played Games (by Jason)
New menu — **START > GAME COLLECTION SETTINGS > LAST 20 PLAYED GAMES**:

- Lists the **20 most recently played games**, most recent first, across every system.
- Three columns: **game name**, **system**, and **session | total** play time.
- Play time is tracked per game via two new metadata fields (`gametime`, `lastsession`) written on every exit.
- Selecting a game **relaunches it on its most recent savestate** — manual slots (`0`–`9`) take priority over the auto-state, injected through RetroArch's `--entryslot`.
- A `*` marks entries that have a savestate available to resume from.
- RetroArch launcher scripts are filtered out; slot injection is skipped for MAME cores and non-RetroArch emulators (PICO-8, PPSSPP standalone), which still launch normally.

---

### 🔋 BatteryPlus Settings menu (by Jason)
New menu — **START > ASDVANCED > BATTERYPLUS SETTINGS**:

| Entry | Description |
|-------|-------------|
| **BatteryPlus Status** | Daemon active/inactive |
| **Calibration** | Current calibration state |
| **Battery Level** | Live % from `/tmp/battery.percent` |
| **BatteryPlus Daemon** | Toggle on/off |
| **BatteryPlus Mode** | `voltage` or `pmic` |
| **Reset Calibration** | Delete learned voltage anchors |

---

### 📡 Sound Settings
- **Verbal Play Timer Warning** — a verbal warning for the play timer limit expiry, affects performance
- **Battery Voice Warning** has been fixed — affects performance when enabled
- **Boot Volume (%)** — set a consistent boot volume regardless of the value at shutdown

---

### ☁️ Scraper
- additional systems added to Ports: PC Windows, Xbox, Android, PS4 and Switch
  - increased chance of finding a match
  - increased chance of auto-scraping finding the wrong match
  - choose manually for best results
 
---

### ☁️ SaveSync
New menu — **START > ADVANCED SETTINGS > SAVESYNC SETTINGS**. Syncs your saves and savestates with a remote share:

| Entry | Description |
|-------|-------------|
| **Enable SaveSync** | Toggles the `savesync.service` daemon |
| **Synchronize Now** | Manual sync on demand |
| **Rebuild Folder Cache** | Rescans the folders to sync |
| **Credentials** | Server, share, user and password |
| **Protocol** | `SMB` (Windows / Samba), `NFS` (Linux / NAS), `SSHFS` (SSH / SFTP), `WebDAV` (Nextcloud / ownCloud) |
| **Log** | View the sync log |

Entries below the toggle appear only while SaveSync is enabled, and missing dependencies for the selected protocol are checked automatically.

---

### 🌍 Translations (w/contributions from Jason)
All new strings translated into **17 languages**:
`br` `de` `el` `es` `fr` `it` `ja` `ko` `pl` `pt` `ru` `sv` `ua` `uk` `vi` `zh-CN` `zh-TW`

---

### 🔄 Auto-build (by Jason)
Every push builds automatically via GitHub Actions:
- `aarch64-linux-gnu-g++` cross-compiler
- Official **Mali RK bifrost G31** libraries (not Mesa)
- `-O3 -march=armv8-a+crc -mtune=cortex-a35 -ffast-math`
- ScreenScraper credentials injected via GitHub Secrets

---

## 📋 Requirements

- R36S running **dArkOSen**
- No internet connection required — everything is included in the zip
- Application of the rootfs overlay containing all the additional scripts and binaries

---

## 🚀 Installation

1. Download the latest **`emulationstation-roms-tools`** zip from [GitHub Actions](https://github.com/djparentx/EmulationStation-fcamod-dArkOS-EN/actions)
2. Extract and copy all contents to: `roms/tools/`
3. Launch `install-es.sh` from the **Tools** section on your device
4. Select **Install ES-dArkOSen** — the installer will:
   - Back up the original ES binary
   - Install the new binary + all resources (icons, locales, battery/network packs, splash)
   - Install and start `es-status-daemon`
   - Install `BatteryPlus` and enable the service
   - Apply launch optimizations

---

## 🙏 Thanks

- [christianhaitian](https://github.com/christianhaitian) for the base EmulationStation fork
- [jason3x](https://github.com/jason3x) for created a branche EmulationStation dArkOSen and a lot of menu
- [lcdyk0517](https://github.com/lcdyk0517) for key latency improvements
- [Mikhailzrick](https://github.com/Mikhailzrick) for BatteryPlus

---

## ☕ Support the project

[<img width="200" height="67" alt="ChatGPT Image Aug 12, 2026, 02_04_12 PM" img align="left"  src="https://github.com/user-attachments/assets/7eb7b583-5d57-4884-ae90-ad7127d733c4" />](https://ko-fi.com/O8Z424G15Y)

[![Ko-fi](https://img.shields.io/badge/☕_Buy_me_a_coffee-jason3x-red?style=for-the-badge)](https://ko-fi.com/jason3x)
