# Mixxx DJ for Android 🎧📱

<div align="center">

![Android Version](https://img.shields.io/badge/Android-10.0%2B%20(API%2029--35)-brightgreen?logo=android&logoColor=white)
![Architecture](https://img.shields.io/badge/Arch-ARM64%20(arm64--v8a)-blue?logo=arm)
![Framework](https://img.shields.io/badge/Qt-6.8%20%7C%20QML-41cd52?logo=qt)
![License](https://img.shields.io/badge/License-GPL%20v2-orange.svg)
![Status](https://img.shields.io/badge/Status-Optimized%20Mobile%20Port-purple)

**The world's premier open-source DJ software, fully ported, modernized, and touch-optimized for Android smartphones, tablets, and standalone hardware setups.**

*Maintained and enhanced by [JaviLabarumDJ](https://github.com/JaviLabarumDJ)*

</div>

---

## 🌟 Overview

**Mixxx DJ for Android** brings the full power of desktop Mixxx directly to your mobile device. Rather than a stripped-down companion app, this is a native C++/Qt 6.8 build running the complete Mixxx 2.7 real-time audio engine, 32-bit floating point DSP, and low-latency audio processing on Android ARM64 architecture.

Whether you're performing live sets using a USB DJ controller, mixing on the go with custom touch controls, or using your device as a standalone visual deck screen, Mixxx Android delivers studio-grade DJ capabilities in your pocket.

---

## ✨ Key Features

### 🎛️ 1. Touch DJ Mode (`DJ`)
* **Full-Featured 2-Deck & 4-Deck Mixing Console:** Dedicated channels with 3-band EQ, gain trims, and high/low-pass Quick Filters.
* **Touch-Optimized Faders:** Responsive volume sliders and crossfader designed specifically for capacitive touchscreen dragging, direct tapping, and double-tap centering.
* **Virtual Platters (Spinny):** Low-latency vinyl emulation scratching, touch pitch bend, and visual cue point markers.
* **Performance Hotcues & Stems:** Instantaneous trigger pads for cues, loops, and real-time audio stem isolation.

### 🌊 2. Pioneer XDJ-RX3 Waveform Mode (`WAVE`)
* **Dual Stacked Scrolling RGB Waveforms:** Vertically aligned scrolling waveforms for effortless visual beatmatching and phrase alignment.
* **Integrated Pioneer-Style Deck Status Dashboard:**
  * **Digital Time Display:** Monospace digital clock (`REMAIN` / `TIME`) with single-tap switching between remaining and elapsed track time.
  * **Precision Pitch Percentage:** Real-time tempo deviation display (`±0.00%`).
  * **Hero BPM Display:** High-visibility digital tempo with **`MASTER`** status badge and single-tap **`SYNC`** engagement.
  * **Musical Key Badge:** Harmonic key notation (Camelot / Musical key) displayed directly on each deck.
  * **Full Track Overview Waveform:** Interactive progress mini-map displaying track structure (intros, breakdowns, drops) with instant finger needle-drop seeking.
  * **Quick Transport Controls:** Dedicated `CUE` and `PLAY/PAUSE` buttons for direct performance control without leaving waveform view.

### 🎹 3. USB MIDI DJ Controller Plug-and-Play
* **Automated USB Device Detection:** Connect any class-compliant DJ controller (Pioneer DDJ-400 / DDJ-FLX4 / SB3, Traktor Kontrol S2/S4, Hercules Inpulse, Numark Mixtrack, etc.) via USB OTG.
* **Built-In Permission Broker:** Mixxx automatically intercepts USB controller connection events and requests Android USB host access with a single system prompt.
* **Native Desktop Mappings:** Direct compatibility with Mixxx's extensive library of JavaScript and XML controller mappings.

### 🎧 4. Professional Multi-Channel Audio & Split Cue
* **Multi-Channel USB Soundcards:** Route Master output to PA systems while monitoring channels privately in headphones via USB audio interfaces.
* **Split Cue Support (`SPLIT`):** Mix in any environment with standard stereo headphones! Directs Headphone PFL (Cue) to one ear and Master output to the other.
* **Low-Latency Engine:** Direct OpenSL ES and AAudio / PortAudio backend configuration for instantaneous response.

### 📁 5. Fast Touch Music Library & Streaming Hub
* **Instant Track Search & Browser:** Search across hundreds of thousands of local tracks with instant filtering by title, artist, BPM, and musical key.
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
1. Download the latest signed release APK from [Releases](https://github.com/JaviLabarumDJ/mixxx-android/releases).
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
* **Upstream Project:** [Mixxx DJ Development Team](https://github.com/mixxxdj/mixxx) - *Free & Open Source DJ Software*

## 📄 License

This project is licensed under the **GNU General Public License v2 (GPL-2.0)**. See the [LICENSE](LICENSE) file for complete terms and conditions.
