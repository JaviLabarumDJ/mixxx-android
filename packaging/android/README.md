# Mixxx on Android

The Android port builds the QML user interface (`--new-ui`) against the Oboe
backend of PortAudio. It targets tablets and Chromebooks as much as phones:
the whole DJ surface is laid out for landscape and is driven by multi-touch.

## Building

```sh
source tools/android_buildenv.sh setup
cmake -DCMAKE_TOOLCHAIN_FILE="${MIXXX_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
      -DCMAKE_SYSTEM_NAME=Android \
      -DANDROID_ABI=arm64-v8a \
      -S . -B build-android
cmake --build build-android
```

`ANDROID_ABI` selects the target architecture (`arm64-v8a`, `x86_64`,
`armeabi-v7a`, `x86`). Only `arm64-v8a` has a published prebuilt dependency
archive, which covers phones, ARM tablets and ARM Chromebooks. To build for
another ABI — notably `x86_64` for Intel Chromebooks and the emulator — the
vcpkg dependency tree has to be built for that triplet first, see
<https://github.com/mixxxdj/mixxx/wiki/Compiling-dependencies-for-android>.

## Touch interaction

Touch input is handled by Qt Quick pointer handlers rather than `MouseArea`,
because Qt synthesizes mouse events from a single touch point only. Using
handlers is what allows two decks to be operated at the same time:

| Gesture                          | Action                                     |
| -------------------------------- | ------------------------------------------ |
| Drag on the waveform             | Scratch (independently per deck)           |
| Pinch on the waveform            | Waveform zoom (mouse wheel equivalent)     |
| Drag on the spinny               | Scratch (independently per deck)           |
| Drag on a knob or fader          | Change the value                           |
| Long press on a hotcue           | Open the hotcue popup (right click on desktop) |

## Controllers and the screens

> _The following section was written by an AI agent (Claude Code)._

The Android UI creates `[Skin]` controls that controller mappings can use
(see `res/qml/+android/MainWindow.qml`):

| Control | Action |
| ------- | ------ |
| `[Skin],mobile_library_toggle` | Shows the LIB view, or returns to the previous view |
| `[Skin],mobile_loader_deck1`, `[Skin],mobile_loader_deck2` | Opens the deck loader of deck 1/2 |

`[ChannelN],LoadSelectedTrack` (LOAD) opens the deck loader of deck 1/2 when
no library is on screen; with a library on screen it loads the selected
track, and closes the deck loader if it was used. The DDJ-400 and DDJ-FLX4
mappings map SHIFT+LOAD to `mobile_library_toggle`.

> _End of AI-written section._

## Merging upstream Mixxx

### QML overrides

> _The following section was written by an AI agent (Claude Code)._

The upstream QML files in `res/qml/` are kept unmodified, so that merging
`mixxxdj/mixxx` does not conflict. The Android versions live next to them in
`+android/` folders, e.g. `res/qml/Settings/+android/Interface.qml` replaces
`res/qml/Settings/Interface.qml`.

The replacement happens when the APK is built:
`cmake/modules/apply_android_qml_overrides.cmake` copies each `+android` file
over the upstream one in the package's assets and removes the `+android`
folders. The app therefore sees the same files as if the upstream files had
been edited. Qt's runtime
[QML file selectors](https://doc.qt.io/qt-6/qtquick-positioning-qml-file-selectors.html)
are deliberately not used: from Qt 6.12 on, relative imports inside a
`+android` file (`import "."`, `import "../Theme"`) resolve against the
`+android` folder and the UI would fail to load.

Rules when changing the UI:

* To change an upstream QML file for Android, copy it to the `+android/`
  folder next to it and edit the copy. Never edit the upstream file.
* Write the copy as if it were in the original folder: its relative imports
  are resolved from there once it is packaged.
* Files that only exist in this fork (e.g. `res/qml/WaveDeckBar.qml`,
  `res/qml/Deck/TouchSpinny.qml`) are added as normal files, not into
  `+android/`.
* QML compiled into the binary (`res/qml/Mixxx/...`) cannot be overridden this
  way; use a fork-owned copy instead, as `Deck/TouchSpinny.qml` does for
  `Mixxx.Controls.Spinny`.
* After merging upstream, check whether upstream changed a file that has an
  override (`git diff <old-upstream> <new-upstream> -- res/qml`) and port the
  change into the override by hand if needed.

To run the Android UI on a desktop build, apply the overrides to a copy of
`res/qml`
(`cmake -DQML_DIR=<copy> -P cmake/modules/apply_android_qml_overrides.cmake`)
and start Mixxx with `--new-ui --resource-path` pointing at a resource folder
that contains it.

### C++ and CMake

The same goal applies to the C++ code: Android features live in their own
files, and upstream files only get small hooks.

* `cmake/modules/MixxxAndroid.cmake` adds the Android sources, definitions
  and build steps. `CMakeLists.txt` only includes it at the end; what has to
  happen at a fixed point (toolchain setup, Qt components, the Android
  packaging properties and permissions) stays in `CMakeLists.txt`.
* QML features only used by the Android UI are separate singletons:
  `Mixxx.MobileLibrary`, `Mixxx.MobileAudio`, `Mixxx.MobileControllers`,
  `Mixxx.Language`, `Mixxx.StreamingManager` and the
  `Mixxx.LibraryFeatureSource` type. The singletons that need core services
  get them in `src/qml/qmlmobilesingletons.cpp`, which `coreservices.cpp`
  calls once.
* `SoundManagerAndroid` holds the Android audio logic (routing to DJ
  controllers, fallback to the speaker, USB hotplug); `SoundManager` calls
  it at a few points. Likewise `AndroidUsbSoundDevices` for
  `PortAudioEnumerator`, `mappingmatcher.cpp` for `ControllerManager` and
  `librarycoverloader.cpp` for `AsyncImageProvider`.
* The remaining changes to upstream files are fixes of upstream code (for
  example in `qmlpreferencesproxy.cpp`, `controller.h` or the Oboe setup in
  `portaudioenumerator.cpp`). The best way to keep them from conflicting is to
  contribute them to mixxxdj/mixxx.

> _End of AI-written section._

## Translations

> _The following section was written by an AI agent (Claude Code)._

The UI language is chosen in *Settings → Interface → Language* and is applied
immediately. It is stored in the same `[Config],Locale` key as the desktop
preferences; "System default" follows the Android language. The selector only
lists the languages that have a `mixxx-android_<lang>` catalog (plus English):
the upstream catalogs don't cover the QML UI, so other languages would still
show it in English.

The source language of all UI strings is English (`qsTr("...")`). Strings of
the QML UI and of the Android-only C++ code are kept in their own catalogs,
`res/translations/mixxx-android_<lang>.ts`, which are loaded on top of the
upstream `mixxx_<lang>.qm` files. The upstream catalogs are never edited, so
merging `mixxxdj/mixxx` does not cause translation conflicts.

After changing UI strings, refresh the catalogs and the compiled `.qm` files
(Qt 6 `lupdate`/`lrelease` in `PATH` or `QT_BIN`):

```sh
tools/android_translations.sh        # update the existing catalogs
tools/android_translations.sh fr     # also start a new French catalog
```

Then fill in the `<translation>` entries (e.g. with Qt Linguist) and run the
script again. Commit both the `.ts` and the `.qm` files.

> _End of AI-written section._

## Permissions

* `READ_MEDIA_AUDIO` (Android 13+) resp. `READ_EXTERNAL_STORAGE` are requested
  at every start; without them the library scan finds nothing.
* `MANAGE_EXTERNAL_STORAGE` ("All files access") is optional and only needed to
  add music directories outside of the media store. Mixxx opens the system
  settings page for it once, on first run.
* `usb.host` is declared as *not* required so that devices without a USB host
  port — many tablets, Chromebooks and emulators — can install Mixxx. Wired
  MIDI/HID controllers are unavailable there.

## Known limitations

* Audio stops being reliable when Mixxx is sent to the background: there is no
  foreground service yet, so Android may throttle or kill the audio thread.
* Recording and broadcasting write to the app private directory unless "All
  files access" has been granted.
* The desktop (QWidget) skins are not built for Android, only the QML UI is.
