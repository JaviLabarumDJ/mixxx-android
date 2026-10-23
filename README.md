# Mixxx DJ for Android 🎧📱

<div align="center">

![Android Version](https://img.shields.io/badge/Android-10.0%2B%20(API%2029--35)-brightgreen?logo=android&logoColor=white)
![Architecture](https://img.shields.io/badge/Arch-ARM64%20(arm64--v8a)-blue?logo=arm)
![Framework](https://img.shields.io/badge/Qt-6.8%20%7C%20QML-41cd52?logo=qt)
[![Latest Release](https://img.shields.io/github/v/release/JaviLabarumDJ/mixxx-android?color=success&label=Release&logo=github)](https://github.com/JaviLabarumDJ/mixxx-android/releases/tag/v1.4.2-android)
[![Download APK](https://img.shields.io/badge/Download-APK%20(249%20MB)-007ec6?logo=android&logoColor=white)](https://github.com/JaviLabarumDJ/mixxx-android/releases/download/v1.4.2-android/mixxx-android-v1.4.2.apk)
![License](https://img.shields.io/badge/License-GPL%20v2-orange.svg)

**The world's premier open-source DJ software, fully ported, modernized, and touch-optimized for Android smartphones, tablets, and standalone hardware setups.**

*Maintained and enhanced by [JaviLabarumDJ](https://github.com/JaviLabarumDJ)*

</div>

---

## 🌟 Overview

**Mixxx DJ for Android** brings the full power of desktop Mixxx directly to your mobile device. Rather than a stripped-down companion app, this is a native C++/Qt 6.8 build running the complete Mixxx 2.7 real-time audio engine, 32-bit floating point DSP, and low-latency audio processing on Android ARM64 architecture.

Whether you're performing live sets using a USB DJ controller, mixing on the go with custom touch controls, or using your device as a standalone visual deck screen, Mixxx Android delivers studio-grade DJ capabilities in your pocket.

---

<!-- START_AI_GENERATED_DISCLAIMER -->
<!-- This section was written with the assistance of an AI Agent (Claude). -->

## 🆕 What's New in v1.4.2

* **Rekordbox "date added"** (by [morpheasgr](https://github.com/morpheasgr)): the *Added* column of Rekordbox sticks and stick backups shows the day each track was added to the Rekordbox collection, and tracks that **⬇ COPY**, **+ ALL** or **+ PLAYLIST** bring into the library keep that date.
* **Playlist order:** a new **#** column shows each track's position in playlists, history and Rekordbox playlists, and a playlist opens in its own order. A whole USB drive opens newest first.
* **Smoother app:** the waveforms only redraw at full rate while the track moves, so the interface no longer repaints the whole screen 60 times a second when idle (UI CPU on a Lenovo tablet went from 74% to about 1%). The app no longer runs in developer mode with debug logging, and the log is no longer written to disk line by line, which caused audio and UI stutters.
* **Choose your top bar:** in **Settings → Interface → 🧭 TOP BAR**, pick which views (DJ, WAVE, LIB, CTL) and buttons (LOAD 1/2, FX, XY, SPLIT, REC, 4D) appear at the top of the screen. At least one view always stays visible.

## 🗂️ What was new in v1.4.1

* **Beatgrid corrections from the deck** (by [morpheasgr](https://github.com/morpheasgr)): tap a deck's **BPM box** (or its **Beatgrid / GRID** button) to fix the grid of the loaded track: **½ BEAT** for beats detected on the off-beats, **SET DOWNBEAT HERE**, fine **‹ EARLIER / LATER ›** nudges, **BPM ÷2 / ×2**, **MATCH OTHER DECK**, **UNDO**, and lock or unlock the grid. Changes are saved with the track.
* **Settings fixes:** the audio profile cards no longer cut off their descriptions, the streaming cache path no longer runs under its button, and the analysis buttons show their full text.
* **Playlists:** creating a playlist from the keyboard and the CREATE button no longer creates it twice.
* **Icons:** the refresh symbol, which showed as an empty box on Android, is now 🔄.

## 🗂️ What was new in v1.4.0

This release merges the work of **[morpheasgr (Angelos Exarchos)](https://github.com/morpheasgr)** from [the morpheasgr/mixxx-android fork (android-stable)](https://github.com/morpheasgr/mixxx-android/tree/android-stable). Special thanks to Angelos for the massive advances in Rekordbox playlist copies, library backups, and controller navigation!

* **Copy Rekordbox playlists to the device:** a playlist of a USB drive can be copied to the tablet with **⬇ COPY**, keeping its order, beatgrids, cues, keys and colors, so you can play it without the drive. An existing playlist or folder with the same name is never replaced, and the free space is checked first.
* **Library backup and restore:** *Settings → Music Library* saves the library (tracks, BPM, keys, cues, playlists, crates, history) and the settings to `/storage/emulated/0/Mixxx/backup`, which survives uninstalling Mixxx. Passwords and the streaming cache are not included.
* **Rekordbox stick backup:** copies a whole Rekordbox USB stick to the tablet; the copy then appears in the library like the stick.
* **Browse with the controller:** BROWSE opens folders and playlists and goes on into their tracks, LOAD opens the deck loader when no library is shown, SHIFT + LOAD shows or hides the library (DDJ-400 and DDJ-FLX4), and only the library on screen follows the controller.
* **Library:** new *Added* column, scans ordered by file date, **+ ALL** adds a whole list to a playlist, the sidebar remembers whether it was shown, USB sticks are followed when plugged in and out.
* **Under the hood:** the Android screens live next to the upstream Mixxx files (`+android` folders), so new Mixxx versions can be merged more easily; crash reports in `/storage/emulated/0/Mixxx/logs`.
* **Fixes:** the language is now applied from the first screen and kept when Android closes the app, USB sticks are read again after a hub resets them, and the DDJ-FLX4 BEAT FX works without pressing LEFT/RIGHT first.

## 🗂️ What was new in v1.3.0

* **English and Spanish:** the whole Android interface is now available in English and Spanish. By default Mixxx follows the language of the device; change it in *Settings → Interface → Language*. Effect presets created on the first launch are shown in the selected language too.
* **Smoother selectors:** drop-down lists (quick effects, effect units, settings) scroll smoothly, no longer jump to the item under your finger while you drag, and open at the selected item.
* **About:** shows the real version of the installed app.

## 🗂️ What was new in v1.2.0

* **Play straight from Rekordbox and Engine DJ USB drives:** plug in the drive you use on Pioneer or Denon players and its playlists show up in the library (*Rekordbox* and *Engine DJ* sections). Engine DJ 2.x–4.x libraries are read directly; tracks are copied to the tablet when you load them into a deck.
* **Built-in USB drive reader:** drives that Android doesn't mount (e.g. FAT32 sticks formatted without a partition table, which Pioneer players accept) are read by Mixxx itself, read-only. Nothing is ever written to the drive.
* **Organized library:** sidebar with Tracks, Playlists, Crates, History, folders of the tablet (*Computer*) and Recordings. Create playlists and add tracks from the tablet with **+ PLAYLIST**.
* **Browse from the controller:** the BROWSE knob moves through the list and keeps the selection visible, stops at the first and last track, and pressing it switches to the sidebar to open playlists and folders (SHIFT + press goes back up).
* **Smoother scrolling:** cover art and track data are no longer loaded on the main thread while scrolling.
* **Stability:** fixed random crashes at startup caused by a memory bug in the audio library (PortAudio) when listing sound devices.

## 🗂️ What was new in v1.1.0

* **Built-in USB audio driver for DJ controllers:** Mixxx now drives class compliant USB sound cards itself (USB Audio Class 1 and 2, synchronous, adaptive and asynchronous endpoints). This fixes controllers whose headphone output (channels 3-4) Android can't play to, e.g. the Pioneer DDJ-400 on tablets with Qualcomm USB audio offload. Configurable in *Settings → Audio → Mixxx USB driver* (auto / always / never).
* **Native Android MIDI:** USB and Bluetooth MIDI controllers are detected through Android's MIDI service, hot-plugged at any time, and get their official mapping assigned automatically on first connection.
* **Automatic routing:** when a DJ controller with 4 outputs is connected, Main goes to channels 1-2 (RCA) and the headphone cue to channels 3-4, at the controller's own sample rate and full volume.
* **Manual beatmatching:** per deck quantize (Q) and hold-to-nudge (◀ ▶) buttons; the tempo fader no longer jumps when touched; tapping SYNC matches the beat once instead of locking.
* **Effects:** effect units are ready on first launch, BEAT FX on the DDJ-400 works from the start, channel assignment (1 / 2 / M) in the FX panel, safer XY pad and Quick Filter controls.
* **Stability:** fixed waveforms jumping back to the cue point, a crash at startup, USB permission handling on Android 12+, settings and effects now survive Android closing the app in the background.

<!-- END_AI_GENERATED_DISCLAIMER -->

---

## ✨ Key Features

### 🎛️ 1. Touch DJ Mode (`DJ`)
* **Full-Featured 2-Deck & 4-Deck Mixing Console:** Dedicated channels with 3-band EQ, gain trims, and high/low-pass Quick Filters.
* **Touch-Optimized Faders:** Responsive volume sliders and crossfader designed for capacitive touchscreens: faders move relative to your finger (no jumps when you touch them) and double-tap resets them.
* **Virtual Platters (Spinny):** Low-latency vinyl emulation scratching and visual cue point markers. Hold the **◀ ▶** nudge buttons to bend the tempo, and toggle **Q** (quantize) per deck.
* **Performance Hotcues & Stems:** Instantaneous trigger pads for cues, loops, and real-time audio stem isolation.

### 🌊 2. Pioneer XDJ-RX3 Waveform Mode (`WAVE`)
* **Dual Stacked Scrolling RGB Waveforms:** Vertically aligned scrolling waveforms for effortless visual beatmatching and phrase alignment.
* **Integrated Pioneer-Style Deck Status Dashboard:**
  * **Digital Time Display:** Monospace digital clock (`REMAIN` / `TIME`) with single-tap switching between remaining and elapsed track time.
  * **Precision Pitch Percentage:** Real-time tempo deviation display (`±0.00%`).
  * **Hero BPM Display:** High-visibility digital tempo with **`MASTER`** status badge. Tap **`SYNC`** to match tempo and phase once, long-press it to lock sync (**`LOCK`**).
  * **Musical Key Badge:** Harmonic key notation (Camelot / Musical key) displayed directly on each deck.
  * **Full Track Overview Waveform:** Interactive progress mini-map displaying track structure (intros, breakdowns, drops) with instant finger needle-drop seeking.
  * **Quick Transport Controls:** Dedicated `CUE` and `PLAY/PAUSE` buttons for direct performance control without leaving waveform view.

<!-- START_AI_GENERATED_DISCLAIMER -->
<!-- Sections 3 and 4 were updated with the assistance of an AI Agent (Claude). -->

### 🎹 3. USB MIDI DJ Controller Plug-and-Play
* **Automated Controller Detection:** Connect any class-compliant DJ controller (Pioneer DDJ-400 / DDJ-FLX4 / SB3, Traktor Kontrol S2/S4, Hercules Inpulse, Numark Mixtrack, etc.) via USB OTG, or a Bluetooth MIDI controller. Controllers are found through Android's MIDI service and can be plugged in and out while Mixxx is running.
* **Automatic Mapping:** On first connection Mixxx assigns and enables the matching official mapping; nothing to configure.
* **USB Permission Prompt:** MIDI-only controllers need no permission. HID controllers and controllers whose sound card Mixxx drives directly ask once for USB access; tick *Always* and accept.
* **Native Desktop Mappings:** Direct compatibility with Mixxx's extensive library of JavaScript and XML controller mappings.

### 🎧 4. Professional Multi-Channel Audio & Split Cue
* **Multi-Channel USB Soundcards:** Main output to the PA (channels 1-2) and headphone cue (channels 3-4) on DJ controllers and USB audio interfaces, routed automatically when a 4-output controller is connected.
* **Built-in USB Audio Driver:** Many Android devices can't play to multichannel USB sound cards. Mixxx then drives the card itself (USB Audio Class 1 and 2). *Settings → Audio → Mixxx USB driver*: **auto** (multichannel cards only), **always**, **never**.
* **Split Cue Support (`SPLIT`):** Mix in any environment with standard stereo headphones and a splitter cable: the left channel carries the headphone cue (PFL) and the right channel the Main output, both in mono.
* **Low-Latency Engine:** AAudio (via Oboe/PortAudio) or Mixxx's own USB driver, with a 10 ms default buffer.

<!-- END_AI_GENERATED_DISCLAIMER -->

### 📁 5. Fast Touch Music Library & Streaming Hub
* **Instant Track Search & Browser:** Search across hundreds of thousands of local tracks with instant filtering by title, artist, BPM, and musical key.
* **Sidebar (`PLAYLISTS` button):** Tracks, Playlists, Crates, History, the tablet's folders, Recordings, and the playlists of **Rekordbox** and **Engine DJ** USB drives. The controller's BROWSE knob moves through the list; pressing it switches to the sidebar.
* **USB drives:** Rekordbox and Engine DJ exports can be played straight from the drive. Reading them needs Android's *All files access* permission, which Mixxx asks for. Drives that Android doesn't mount are read by Mixxx itself, read-only.
* **QuickLoad Drawers (`LOAD 1` / `LOAD 2`):** Slide-out drawer to search and load tracks into any deck without obstructing your active mixer or waveforms.
* **Streaming & Remote Sources:** Support for local SD card music collections, USB flash drives, and online streaming endpoints (Subsonic / Web stream audio).

### 🎚️ 6. Performance XY Pad (`XY`)
* Real-time dual-axis touch modulation pad for sweeping resonant filter cutoff and effect parameters simultaneously during build-ups and drops.

---

## 📱 Hardware Requirements

| Component | Minimum | Recommended |
|---|---|---|
| **OS** | Android 10.0 (API 29) | Android 13 - 15 (HyperOS / OneUI / AOSP) |
| **Processor** | 64-bit ARM (ARMv8-A) | Snapdragon 8 Gen 1 / Dimensity 8300-Ultra or higher |
| **RAM** | 4 GB | 8 GB+ |
| **Screen** | 5.5" (1080p) | 6.67" AMOLED 120Hz+ or Tablet (10"+) |
| **Connectivity** | USB-C OTG Host support | USB-C with powered USB hub for soundcards & controllers |

---

## 🚀 Installation & Quick Start

### Option A: Install Pre-Built Signed APK
1. Download [**mixxx-android-v1.4.2.apk**](https://github.com/JaviLabarumDJ/mixxx-android/releases/download/v1.4.2-android/mixxx-android-v1.4.2.apk) from the [Official GitHub Release v1.4.2](https://github.com/JaviLabarumDJ/mixxx-android/releases/tag/v1.4.2-android).
2. Enable *"Install unknown apps"* in your Android device settings.
3. Install and launch **MIXXX**.
4. Grant storage and audio permissions when prompted.

### Option B: Fast Install via ADB (Connected Device)
If your phone is connected to your development machine with USB Debugging enabled:
```bash
# Clone the repository
git clone https://github.com/JaviLabarumDJ/mixxx-android.git
cd mixxx-android

# Install directly to phone using the built-in deployment tool
./tools/install_to_phone.sh
```

---

## 🛠️ Building from Source

Mixxx for Android is cross-compiled on Linux using CMake, the Android NDK, and pre-packaged Qt 6.8 build dependencies.

### 1. Prerequisites
* **OS:** Linux (Ubuntu 22.04+ / Debian 12 / Arch Linux)
* **Android SDK:** Platform `android-35`, Build-Tools `35.0.0`
* **Android NDK:** Version `27.2.12479018`
* **Java:** OpenJDK 21
* **Tools:** CMake 3.25+, Ninja, Git

### 2. Setup Build Environment
```bash
# Source and prepare Android dependency environment
source tools/android_buildenv.sh setup
```

### 3. Configure and Compile
```bash
# Configure CMake
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=buildenv/mixxx-deps-2.7-arm64-android-1c20f84a/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_SYSTEM_NAME=Android \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-35 \
  -DANDROID_NDK_HOME=$ANDROID_NDK_HOME \
  .

# Compile native binary and package APK
cmake --build build-android --target apk -j$(nproc)
```

### 4. Sign the APK
```bash
$ANDROID_SDK/build-tools/35.0.0/apksigner sign \
  --ks mixxx.keystore \
  --ks-key-alias mixxx \
  --out mixxx-android-release-signed.apk \
  build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

---

## 👥 Authors & Acknowledgments

* **Android Port & Touch UI Enhancements:** [Javier Sáenz (JaviLabarumDJ)](https://github.com/JaviLabarumDJ)
* **Contributions (v1.4.0 – v1.4.2):** [morpheasgr (Angelos Exarchos)](https://github.com/morpheasgr) — *merged from [morpheasgr/mixxx-android (android-stable)](https://github.com/morpheasgr/mixxx-android/tree/android-stable): Rekordbox playlist copies, library and stick backups, controller library navigation, upstream-friendly architecture, beatgrid corrections from the deck, and Rekordbox dates and playlist order.*
* **Upstream Project:** [Mixxx DJ Development Team](https://github.com/mixxxdj/mixxx) - *Free & Open Source DJ Software*

## 📄 License

This project is licensed under the **GNU General Public License v2 (GPL-2.0)**. See the [LICENSE](LICENSE) file for complete terms and conditions.
