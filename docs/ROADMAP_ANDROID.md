# Mejoras pendientes de Mixxx Android

> _La siguiente sección la ha escrito un agente de IA (Claude Code)._

Lista de mejoras salida de la revisión del 26-10-2026 (sobre la v1.5.2), con su estado. El esfuerzo es aproximado: **S** pequeño, **M** medio, **L** grande. Al terminar una tarea, márcala aquí con la versión en que salió.

## Resumen

| Estado | Tareas |
|---|---|
| ✅ Hechas | 1, 3, 5, 6, 27 (v1.5.3); 8, 9, 10, 11, 13, 15, 26 (v1.5.5) |
| 🟡 A medias | 4, 7 |
| ⬜ Pendientes | 2, 12, 14, 16–25, 28–33, 35–37 |

Fuera de la lista: **actualizaciones desde la app** (v1.5.4) y, en la v1.5.6, la **skin Lite**, **Android 10 a 13** (#34), **SD y navegador de carpetas** (#38) y el **reloj de fotogramas común** (actualizaciones de la interfaz alineadas, sin medir aún).

## 🔴 Prioridad 1: puede cortar o cerrar la sesión en directo

| # | Tarea | Esf. | Estado |
|---|---|---|---|
| 1 | Solo arranca en Android 14+ (Mixxx y Qt importan `copy_file_range@LIBC_U`, `backtrace@LIBC_T`, `pthread_cond_clockwait@LIBC_R`), aunque el README decía Android 10 | S | ✅ v1.5.3: `minSdk 34`, `targetSdk 35`, README corregido. Volver a soportar Android 10–13 exigiría recompilar Qt y las dependencias contra API 28 (**L**) |
| 2 | Sin servicio en primer plano ni wake lock: con la pantalla apagada, una llamada o al cambiar de app, Android puede frenar o matar Mixxx mientras suena (sobre todo con el driver USB propio) | M | ⬜ Pendiente. Servicio `mediaPlayback` con notificación (permiso `POST_NOTIFICATIONS`) y wake lock parcial mientras suena un deck. Los permisos `FOREGROUND_SERVICE*` y `WAKE_LOCK` ya están en el manifiesto, sin usar |
| 3 | El gesto «atrás» cerraba Mixxx de golpe | S | ✅ v1.5.3: diálogo «¿Salir de Mixxx?» (`onClosing` en `+android/main.qml`) |
| 4 | El hilo de audio del driver USB no tiene prioridad de tiempo real, va con unos 9 ms de margen y hace logs y reservas de memoria en el callback | M | 🟡 v1.5.3: el bucle de eventos ya no gira al 100 % ante errores. Falta: sesión de `APerformanceHint` para ese hilo, más audio en cola o renderizar por adelantado, y sacar los `qInfo`/`qWarning` de `measureClock`/`markDisconnected`, el `push_back` de `m_implicitPacketFrames` y el `resize` de `m_renderBuffer` del hilo de audio |
| 5 | El MIDI USB compartía un candado con el audio y reservaba memoria en su hilo | S | ✅ v1.5.3: `rigtorp::SPSCQueue` con la hora de llegada de cada paquete |
| 6 | Si fallaban transferencias USB, el audio se quedaba en silencio | S/M | ✅ v1.5.3: `markOutputFailed()` y `SoundManagerAndroid` reabre los dispositivos |

## 🟠 Prioridad 2: mejoras rápidas que se notan

| # | Tarea | Esf. | Estado |
|---|---|---|---|
| 7 | Tamaño del APK | S/M | 🟡 v1.5.3: de 249 a 236 MB (sin los `.ts` ni las skins Deere, Shade y Tango). **La estimación de «100 MB menos» era errónea**: las `.so` ya salen sin símbolos (Gradle las strippea). Lo que pesa es `Qt6Core` y `Core5Compat` (datos ICU, ~76 MB), `libmixxx` (46 MB) y el plugin FFmpeg de QtMultimedia (15 MB), y los estilos de Qt Quick Controls están en `libs.xml` (se cargan al arrancar). Aún se puede: excluir con un `build.gradle` propio el driver PostgreSQL (7 MB) y los plugins `qmltooling` (1 MB), y estudiar si `Core5Compat` y QtMultimedia son prescindibles |
| 8 | Booth: falta el knob de FILTRO (QuickFX, `super1`) en el mixer táctil (`Skins/Booth/BoothMixer.qml`); Classic lo tiene (`+android/QuickFxKnob.qml`) | S | ✅ v1.5.5: knob FILTRO (QuickFX), atenuado mientras el efecto está apagado |
| 9 | Booth: faltan REC y SPLIT (Classic: `+android/MainWindow.qml`, ~l. 373–427) | S | ✅ v1.5.5: pestañas SPLIT y REC (`MobileRecordButton.qml`, compartido con Classic) |
| 10 | Booth: una pulsación larga en un pad borra el hot cue sin preguntar (`BoothPerformanceDeck.qml`, ~l. 299); Classic abre un menú | S | ✅ v1.5.5: abre el menú de color y borrar (`HotcuePopup`) |
| 11 | Ajustes que no hacen nada: Tema claro/oscuro, Disposición, Color de beatjump (`Settings/+android/Interface.qml`) y «Ocultar barra superior» | S | ✅ v1.5.5: ocultos (`visible: false` con el motivo), también Pantalla completa, Tooltips e Historial de búsqueda |
| 12 | Menú contextual de pista con submenús vacíos: «Añadir a playlist», «Crates» y tres «Reanalizar» desactivados (`Library/+android/Track.qml`) | M | ⬜ |
| 13 | Arranque unos 2 s más lento: la barra de carga es simulada, con esperas fijas (`+android/main.qml`, `loadProgressTimer`/`dismissTimer`) | S | ✅ v1.5.5: se quita en cuanto la skin está cargada (100 ms y fundido de 250 ms) |
| 14 | `GaussianBlur` a pantalla completa en cada fotograma mientras Ajustes está abierto (Classic, `+android/MainWindow.qml` ~l. 935) | S | ⬜ Usar un oscurecido fijo como Booth |

## 🟡 Prioridad 3: funciones que faltan

| # | Tarea | Esf. | Estado |
|---|---|---|---|
| 15 | Opciones de mapeo de la DDJ-400 como las 14 de la FLX4 (Fader Start, instant doubles, sync/loop/cue modo rekordbox…) | M | ✅ v1.5.5: `levelDepthMode`, `loopButtonsMode`, `syncMode`, `cueLoopCallMode`, `shiftSyncMode` (por defecto el rango de tempo, como antes), `shiftHeadphoneCue`, `faderStart`, `instantDoubles`. Faltan los modos de pad (PAD FX, Keyboard, Key Shift, disposición del sampler) y compartir el código con el script de la FLX4 |
| 16 | Preescucha de pistas desde la biblioteca: el deck de preescucha existe (`num_preview_decks = 1`) pero no hay botón | M | ⬜ |
| 17 | Samplers: el motor crea 16 (`num_samplers`) y ninguna skin los muestra; el submenú «Cargar en → Sampler» está vacío | M | ⬜ O bajar `num_samplers` a 0 mientras tanto (**S**) |
| 18 | Booth sin controladora: faltan pitch bend/nudge, key shift, reverse y eject | M | ⬜ |
| 19 | Classic: sin botón SLIP ni modos de pad beatjump, beatloop o loop roll | S/M | ⬜ |
| 20 | USB en exFAT: el lector propio solo lee FAT16/32 (`src/library/usbdrive/fatvolume.cpp`) | L | ⬜ |
| 21 | Engine DJ: importar cues y rejillas (tabla `PerformanceData`), como en Rekordbox | M/L | ⬜ |
| 22 | Serato en USB: no usa `usbdrive::prepareFiles` como Rekordbox y Engine DJ | M | ⬜ |
| 23 | Atajos de Android (`[Skin]` `mobile_library_toggle`/`mobile_fx_toggle`) en más mapeos: DDJ-200, DDJ-SB3, Hercules Inpulse, Numark | S c/u | ⬜ |
| 24 | 4 decks: en móvil no se llega a ellos, pero el menú ofrece «Deck 3/4» | M | ⬜ |

## 🟢 Prioridad 4: móviles, calidad y mantenimiento

| # | Tarea | Esf. | Estado |
|---|---|---|---|
| 25 | Botones demasiado pequeños para el dedo (17–22 px): pitch bend y quantize de Classic, Booth en móviles | S/M | ⬜ |
| 26 | Grabación: el contador se reinicia al cambiar de skin y STOP no pide confirmación (`+android/MainWindow.qml`, ~l. 133–168) | S | ✅ v1.5.5: el tiempo viene de `Mixxx.Recording.durationText` y parar pide confirmación |
| 27 | Manifiesto y permisos | S | ✅ v1.5.3: `allowBackup="false"`, sin `READ_MEDIA_AUDIO` duplicado ni `requestLegacyExternalStorage`, receptor USB `RECEIVER_NOT_EXPORTED`, `targetSdk 35`, `installLocation="internalOnly"` |
| 28 | El QML se copia al almacenamiento compartido en cada arranque si hay «Acceso a todos los archivos» (`qmlapplication.cpp`, ~l. 220): más lento, y otra app podría modificarlo | S | ⬜ Dejarlo solo para desarrollo |
| 29 | La copia de seguridad de la biblioteca bloquea la interfaz (`QmlMobileLibraryProxy::createBackup`) | S | ⬜ |
| 30 | Traducciones: al español le faltan 75 textos y hay textos sin `qsTr`; abrir más idiomas (portugués, francés, griego) | S/M | ⬜ |
| 31 | Tests del código Android (lector FAT, Rekordbox) y CI; script de release (compilar, firmar, verificar) | M | ⬜ |
| 32 | README: limitaciones conocidas, tabla de controladoras, solución de problemas; pasar las notas antiguas a `CHANGELOG.md` | S | ⬜ |
| 33 | El hilo `ControllerManager` gasta ~7 % de CPU en reposo con la controladora USB directa (`AndroidUsbMidiController::isPolling()` es `true`: se sondea aunque no llegue nada). Despertarlo cuando llegan paquetes en vez de sondear | S/M | ⬜ |
| 34 | Dispositivos antiguos: Android 9/10 y 32 bits (`armeabi-v7a`) | L | 🟡 **Android 10 a 13 sin recompilar Qt**: faltaban `copy_file_range` (API 34), `backtrace` (API 33) y `pthread_cond_clockwait` (API 30); `libmixxx_libccompat.so` las aporta y Qt Core, Qt Test y libmixxx se parchean con patchelf para cargarla. `minSdk 29` y `requestLegacyExternalStorage` otra vez (solo lo usa Android 10). Probado que sigue arrancando en Android 14; **falta probarlo en Android 10-13 reales**. Android 9 no: la API de MIDI del NDK (AMidi) llega con Android 10 y las controladoras no funcionarían. 32 bits sigue pendiente |
| 38 | Tarjetas microSD y navegador de carpetas: FOLDER solo llegaba a la SD por «/» → storage → XXXX-XXXX, «Dispositivos extraíbles» estaba vacío en Android, no se pedía el acceso a todos los archivos y una SD insertada desactivaba el lector propio de USB | M | 🟡 Hecho, sin probar con una SD: FOLDER → Dispositivos lista la memoria interna, las SD y los USB montados por Android con su nombre (`org.mixxx.StorageHelper`, `mixxx::androidStorageVolumes()`), se relee al abrirlo, se pide el permiso al abrir FOLDER, botones de SD/USB en Ajustes → Librería Musical, y el lector de USB solo se aparta si Android montó un USB (no por una SD). Android no distingue SD de USB: se mira si el nombre dice «SD» |
| 35 | Si Mixxx muere (p. ej. al reinstalar) con su driver USB usando la DDJ, Android no le vuelve a dar los datos MIDI: el puerto figura abierto pero no llega nada hasta desenchufar la DDJ (visto el 26-10-2026; `/dev/bus/usb` no cambia). Reintentar o avisar en vez de quedarse mudo; al instalar desde la app, soltar antes la controladora | M | ⬜ |
| 36 | El aviso `VisualPlayPosition::calcOffsetAtNextVSync … outdated position request` (`src/waveform/visualplayposition.cpp`) se escribe en cada cambio de estado; con el altavoz de la tablet como reloj (modo Híbrido) cambia sin parar (~1.700 avisos en pocos minutos) y gasta CPU del hilo de la interfaz. Limitarlo | S | ⬜ |
| 37 | Tras una sesión larga con varios cambios de skin, el hilo de la interfaz de Booth estaba al 88 % y tras reiniciar al 64 % con la misma música. Al pasar de Booth a Lite sin reiniciar, el hilo bajó del 64 % al 5 %, así que la skin anterior sí se libera; buscar qué otra cosa se acumula en una sesión larga | S/M | ⬜ |

## Otros pendientes

- **Stems** (rama `stems`, saldrá como v1.6.0):
  - fusionar `android-stable` en ella y renumerar;
  - portar a Booth la cola, el aviso y la pestaña STEM;
  - dejar en Acerca de una sola tarjeta GRACIAS con Angelos y Dani Roman;
  - subir los modelos (`.release/stems-models-v1/`) a la release `stems-models-v1`.
- **SHIFT + LOAD en Booth:** sin probar.
- **Cuelgue del audio de la tablet** (HAL de audio / audioserver) al soltar la DDJ el driver USB propio: sin investigar.
- **Candidatos a enviarse a Mixxx:** el arreglo de uniforms de `rendergraph` (parpadeo de los loops) y el `i < numSamples` de `EngineEffectChain`.
- **Página de Audio:** puede marcar «Móvil / Bluetooth» estando en Híbrido.
- **Columna «#» en la vista de un USB entero:** muestra números sin significado.
- **`applySkinTheme`:** rompe los bindings internos de `Theme`.

## Orden propuesto

1. **v1.6.0 (directo):** 2 y lo que falta de 4, junto con los stems.
2. ~~**Booth y DDJ-400:** 8, 9, 10, 11, 13 y 15.~~ Hecho en la v1.5.5.
3. **Funciones:** 16, 17, 18, 19 y 23.
4. **Resto:** según lo que pidan los usuarios.

> _Fin de la sección escrita por IA._
