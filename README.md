# DigiPools (Digitakt MK1 — OS 1.54)

**DigiPools** is a custom SRC machine mod for the **Elektron Digitakt (MK1)** running firmware **OS 1.54**, built on the [elekloader](https://github.com/irpina/elekloader) ecosystem. It turns the Digitakt's Chromatic mode keyboard into a multi-sample pool player, allowing you to trigger and play different samples from the project's RAM sample pool (slots 1..127) directly across the 16 trig keys.

---

> [!IMPORTANT]
> **Copyright & Legal Notice**:
> This repository contains **NO** copyrighted Elektron firmware binaries or intellectual property. DigiPools is an open-source modification built for the elekloader framework. Users must supply their own officially licensed Digitakt OS 1.54 firmware (`Digitakt_OS1.54.syx`) to generate a patched OS file on their own machine.

---

## Features

- **New Machine: DIGIPOOLS (ID 10)**
  - Accessible via `FUNC + SRC` (displayed as `DIGIPOOLS`, short title `POOL`).
  - Utilizes the stock ONESHOT parameter architecture (TUNE, PLAY, BIT REDUCTION, SAMP, START, LEN, LOOP, LEV).
  - Uses the stock high-fidelity sample playback engine with filters, overdrive, envelopes, LFOs, and FX sends.
- **Chromatic Mode Multi-Sample Playback**
  - In Chromatic mode (`KEYBOARD` / `FUNC + TRK`), each trig key (or chromatic note) triggers a different sample from the RAM sample pool.
  - Natural octave pitch is preserved when shifting octaves UP/DOWN.
- **Instant Key-to-Sample Selection**
  - To assign which key plays which sample:
    1. Enter **Chromatic mode** (`FUNC + TRK`).
    2. Open the **SRC** page.
    3. **Hold down any trig key** (1-16).
    4. Turn **Encoder D (SAMP)** to select the desired sample from the pool.
    5. Release the key.
  - While holding a key:
    - Knob D displays `K<key>:<slot>` (e.g., `K1:4`).
    - An on-screen pop-up banner appears at the top of the OLED display: `KEY <key> -> SMP <slot>`.
  - When no key is held, Encoder D adjusts the track's default base sample.
- **Sequencer Integration (Live & Step Recording)**
  - When recording live (`REC + PLAY`) or using Step Recording (`FUNC + Trig` in grid mode), trigs are automatically recorded with a parameter lock on `SAMP` corresponding to the exact sample played, preserving the recorded pitch and octave tuning across sequencer steps.
  - Sequenced patterns play back identically whether played live or triggered by the sequencer.

---

## Prerequisites

To build from source:
1. **Python 3.9+**
2. **ColdFire / m68k toolchain**: `m68k-elf-gcc` (e.g. `brew install m68k-elf-gcc` on macOS, or pre-built ELF toolchain).
3. **elekloader**: Clone [elekloader](https://github.com/irpina/elekloader) alongside this repository or set `ELEKLOADER_DIR=/path/to/elekloader`.
4. **Stock Firmware**: Download official **Digitakt OS 1.54** (`Digitakt_OS1.54.syx`) from [Elektron Support](https://www.elektron.se/support-downloads/).

---

## Building

1. Copy the official `Digitakt_OS1.54.syx` into this project directory (or point to it using `STOCK_OS`):
   ```bash
   cp /path/to/Digitakt_OS1.54.syx ./Digitakt_OS1.54.syx
   ```

2. Build and patch the firmware:
   ```bash
   make
   ```
   *(or `make STOCK_OS=/path/to/Digitakt_OS1.54.syx`)*

The patched firmware will be generated at:
```
out/Digitakt_OS1.54_DigiPools.syx
```

---

## Web Browser Method (No Compiler Needed)

If you have the compiled mod file `digipools-1.0.elemod` (available on the [Releases](../../releases) page):
1. Open the [elekloader Web App](https://irpina.github.io/elekloader/) in your browser.
2. Drag and drop your official `Digitakt_OS1.54.syx` file into the page.
3. Add `digipools-1.0.elemod` (along with core and machine-pages if prompted).
4. Click **Build** to download your customized `Digitakt_OS1.54_DigiPools.syx`.
*(All processing happens entirely in your browser; your files are never uploaded to any server).*

---

## Flashing Instructions

Flash the resulting `out/Digitakt_OS1.54_DigiPools.syx` file to your Digitakt MK1 using **Elektron Transfer**:

1. Connect your Digitakt MK1 via USB.
2. Open Elektron Transfer and connect to your Digitakt.
3. Drag and drop `Digitakt_OS1.54_DigiPools.syx` into the OS Upgrade tab in Elektron Transfer.
4. Wait for the firmware update to complete and the unit to reboot.
5. On boot, the startup screen will display version `DPLS`.
6. To restore to stock at any time, simply re-flash the official stock `Digitakt_OS1.54.syx` file via Elektron Transfer.
