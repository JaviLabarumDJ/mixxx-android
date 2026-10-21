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

## Android QML overrides

> _The following section was written by an AI agent (Claude Code)._

The upstream QML files in `res/qml/` are kept unmodified, so that merging
`mixxxdj/mixxx` does not conflict. The Android versions live next to them in
`+android/` folders, e.g. `res/qml/Settings/+android/Interface.qml` replaces
`res/qml/Settings/Interface.qml`. This uses Qt's
[QML file selectors](https://doc.qt.io/qt-6/qtquick-positioning-qml-file-selectors.html):
the `android` selector is active automatically on Android, and the
`QQmlApplicationEngine` applies it to the `assets:/` files.

Rules when changing the UI:

* To change an upstream QML file for Android, copy it to the `+android/`
  folder next to it and edit the copy. Never edit the upstream file.
* Relative imports and implicit same-folder types in an override resolve
  against the original folder, not against `+android/`, so
  `import "../Theme"` keeps working unchanged.
* Files that only exist in this fork (e.g. `res/qml/WaveDeckBar.qml`,
  `res/qml/Deck/TouchSpinny.qml`) are added as normal files, not into
  `+android/`, so that other files can find them.
* QML compiled into the binary (`res/qml/Mixxx/...`) cannot be overridden this
  way; use a fork-owned copy instead, as `Deck/TouchSpinny.qml` does for
  `Mixxx.Controls.Spinny`.
* After merging upstream, check whether upstream changed a file that has an
  override (`git diff <old-upstream> <new-upstream> -- res/qml`) and port the
  change into the override by hand if needed.

To run the Android UI on a desktop build, start Mixxx with
`QT_FILE_SELECTORS=android mixxx --new-ui`.

> _End of AI-written section._

## Translations

> _The following section was written by an AI agent (Claude Code)._

The UI language is chosen in *Settings → Interface → Language* and is applied
immediately. It is stored in the same `[Config],Locale` key as the desktop
preferences; "System default" follows the Android language.

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
