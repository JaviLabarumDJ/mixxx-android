# Mejoras pendientes de Mixxx Android

> _La siguiente sección la ha escrito un agente de IA (Claude Code)._

Lista de mejoras salida de la revisión del 26-10-2026 (sobre la v1.5.2), con su estado. El esfuerzo es aproximado: **S** pequeño, **M** medio, **L** grande. Al terminar una tarea, márcala aquí con la versión en que salió.

## Resumen

| Estado | Tareas |
|---|---|
| ✅ Hechas | 1, 3, 5, 6, 27 (v1.5.3); 8, 9, 10, 11, 13, 15, 26 (v1.5.5); 2, 12, 14, 17, 33, 36 (v1.8.0) |
| 🟡 A medias | 4, 7, 30 |
| ⬜ Pendientes | 16, 18–25, 28, 29, 31, 32, 35, 37 |

Fuera de la lista: **ondas fluidas** (v1.6.0 y v1.6.1: se reconstruían en cada fotograma; ahora por GPU y al ritmo de la pantalla), **actualizaciones desde la app** (v1.5.4) y, en la v1.5.6, la **skin Lite**, **Android 10 a 13** (#34), **SD y navegador de carpetas** (#38) y el **reloj de fotogramas común** (actualizaciones de la interfaz alineadas, sin medir aún).

## 🔴 Prioridad 1: puede cortar o cerrar la sesión en directo

| # | Tarea | Esf. | Estado |
|---|---|---|---|
| 1 | Solo arranca en Android 14+ (Mixxx y Qt importan `copy_file_range@LIBC_U`, `backtrace@LIBC_T`, `pthread_cond_clockwait@LIBC_R`), aunque el README decía Android 10 | S | ✅ v1.5.3: `minSdk 34`, `targetSdk 35`, README corregido. Volver a soportar Android 10–13 exigiría recompilar Qt y las dependencias contra API 28 (**L**) |
| 2 | Sin servicio en primer plano ni wake lock: con la pantalla apagada, una llamada o al cambiar de app, Android puede frenar o matar Mixxx mientras suena (sobre todo con el driver USB propio) | M | ✅ v1.8.0: `org.mixxx.PlaybackService` (`mediaPlayback`, notificación «Sonando» y wake lock parcial) mientras suena un deck o hay sonido en el master, y hasta 2 minutos después de parar (`keepRunningWhilePlaying()` en `qmlapplication.cpp`) |
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
| 12 | Menú contextual de pista con submenús vacíos: «Añadir a playlist», «Crates» y tres «Reanalizar» desactivados (`Library/+android/Track.qml`) | M | ✅ v1.8.0: «Añadir a playlist…» abre el selector de playlists de la lista (también crea una), «Volver a analizar la rejilla» usa `reanalyzeBeats`; «Crates» quitado hasta que se puedan llenar desde Android |
| 13 | Arranque unos 2 s más lento: la barra de carga es simulada, con esperas fijas (`+android/main.qml`, `loadProgressTimer`/`dismissTimer`) | S | ✅ v1.5.5: se quita en cuanto la skin está cargada (100 ms y fundido de 250 ms) |
| 14 | `GaussianBlur` a pantalla completa en cada fotograma mientras Ajustes está abierto (Classic, `+android/MainWindow.qml` ~l. 935) | S | ✅ v1.8.0: oscurecido fijo como Booth |

## 🟡 Prioridad 3: funciones que faltan

| # | Tarea | Esf. | Estado |
|---|---|---|---|
| 15 | Opciones de mapeo de la DDJ-400 como las 14 de la FLX4 (Fader Start, instant doubles, sync/loop/cue modo rekordbox…) | M | ✅ v1.5.5: `levelDepthMode`, `loopButtonsMode`, `syncMode`, `cueLoopCallMode`, `shiftSyncMode` (por defecto el rango de tempo, como antes), `shiftHeadphoneCue`, `faderStart`, `instantDoubles`. Faltan los modos de pad (PAD FX, Keyboard, Key Shift, disposición del sampler) y compartir el código con el script de la FLX4 |
| 16 | Preescucha de pistas desde la biblioteca: el deck de preescucha existe (`num_preview_decks = 1`) pero no hay botón | M | ⬜ |
| 17 | Samplers: el motor crea 16 (`num_samplers`) y ninguna skin los muestra; el submenú «Cargar en → Sampler» está vacío | M | ✅ v1.8.0: pestaña SAMPLES (`SamplerStation.qml`) y «Cargar en → Sampler» con los 16 samplers |
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
| 30 | Traducciones: al español le faltan 75 textos y hay textos sin `qsTr`; abrir más idiomas (portugués, francés, griego) | S/M | 🟡 v1.8.0: el español tiene 1.167 textos traducidos; faltan los de streaming. **Ojo:** `tools/android_translations.sh` necesita un `lupdate` que lea QML (p. ej. `QT_BIN=~/.local/lib/python3.13/site-packages/PySide6`); el de `buildenv` no lee QML y borra del catálogo todos sus textos |
| 31 | Tests del código Android (lector FAT, Rekordbox) y CI; script de release (compilar, firmar, verificar) | M | ⬜ |
| 32 | README: limitaciones conocidas, tabla de controladoras, solución de problemas; pasar las notas antiguas a `CHANGELOG.md` | S | ⬜ |
| 33 | El hilo `ControllerManager` gasta ~7 % de CPU en reposo con la controladora USB directa (`AndroidUsbMidiController::isPolling()` es `true`: se sondea aunque no llegue nada). Despertarlo cuando llegan paquetes en vez de sondear | S/M | ✅ v1.8.0: el sondeo pasa de 1 a 4 ms tras 2 s sin mensajes y vuelve a 1 ms con el primero (AMidi no avisa cuando llegan datos). También con las controladoras de Android (p. ej. la LPD8) |
| 34 | Dispositivos antiguos: Android 9/10 y 32 bits (`armeabi-v7a`) | L | 🟡 **Android 10 a 13 sin recompilar Qt**: faltaban `copy_file_range` (API 34), `backtrace` (API 33) y `pthread_cond_clockwait` (API 30); `libmixxx_libccompat.so` las aporta y Qt Core, Qt Test y libmixxx se parchean con patchelf para cargarla. `minSdk 29` y `requestLegacyExternalStorage` otra vez (solo lo usa Android 10). Probado que sigue arrancando en Android 14; **falta probarlo en Android 10-13 reales**. Android 9 no: la API de MIDI del NDK (AMidi) llega con Android 10 y las controladoras no funcionarían. 32 bits sigue pendiente |
| 38 | Tarjetas microSD y navegador de carpetas: FOLDER solo llegaba a la SD por «/» → storage → XXXX-XXXX, «Dispositivos extraíbles» estaba vacío en Android, no se pedía el acceso a todos los archivos y una SD insertada desactivaba el lector propio de USB | M | 🟡 Hecho, sin probar con una SD: FOLDER → Dispositivos lista la memoria interna, las SD y los USB montados por Android con su nombre (`org.mixxx.StorageHelper`, `mixxx::androidStorageVolumes()`), se relee al abrirlo, se pide el permiso al abrir FOLDER, botones de SD/USB en Ajustes → Librería Musical, y el lector de USB solo se aparta si Android montó un USB (no por una SD). Android no distingue SD de USB: se mira si el nombre dice «SD» |
| 35 | Si Mixxx muere (p. ej. al reinstalar) con su driver USB usando la DDJ, Android no le vuelve a dar los datos MIDI: el puerto figura abierto pero no llega nada hasta desenchufar la DDJ (visto el 26-10-2026; `/dev/bus/usb` no cambia). Reintentar o avisar en vez de quedarse mudo; al instalar desde la app, soltar antes la controladora | M | ⬜ |
| 36 | El aviso `VisualPlayPosition::calcOffsetAtNextVSync … outdated position request` (`src/waveform/visualplayposition.cpp`) se escribe en cada cambio de estado; con el altavoz de la tablet como reloj (modo Híbrido) cambia sin parar (~1.700 avisos en pocos minutos) y gasta CPU del hilo de la interfaz. Limitarlo | S | ✅ v1.8.0: como mucho un aviso cada 10 s |
| 37 | Tras una sesión larga con varios cambios de skin, el hilo de la interfaz de Booth estaba al 88 % y tras reiniciar al 64 % con la misma música. Al pasar de Booth a Lite sin reiniciar, el hilo bajó del 64 % al 5 %, así que la skin anterior sí se libera; buscar qué otra cosa se acumula en una sesión larga | S/M | ⬜ |

## Otros pendientes

- **v1.8.0, sin probar con el hardware:**
  - **Timecode / DVS** (Ajustes → Timecode / DVS, botón VINYL en Classic, Booth y Lite): la pantalla, los ajustes y el aviso sin entrada están probados en la tablet; la señal real de timecode, no (falta una tarjeta con entradas o una DJM-250MK2 y timecode).
  - **Entrada del driver USB propio** (`AndroidUsbAudioDevice::startOutput(..., captureCallback)`): las tarjetas con más de 2 entradas también pasan al driver propio en modo AUTO. Probar que la DDJ-400 y las tarjetas que ya iban siguen igual.
  - **Learn avanzado** (SHIFT, pulsación larga y doble, combinaciones, LED) con una controladora real.
- **Entradas del audio de Android (micrófonos, auxiliares, timecode por Android):** Mixxx declaraba `RECORD_AUDIO` pero no lo pedía; desde la v1.8.0 Ajustes → Audio lo pide al asignar una entrada y reabre los dispositivos al concederlo. Aun así, en la Tab M11 el PortAudio precompilado (host Oboe) no abre ninguna entrada, ni estéreo ni mono: `Oboe couldn't open the input stream: ErrorIllegalArgument`. Probar en otros dispositivos y, si pasa en todos, revisar cómo `PaOboe` construye el stream de entrada (no tenemos su código: es la librería de `buildenv`). El driver USB propio no depende de esto.
- **Groove Station:** los patrones, el kit y el swing se guardan en `groove-patterns.json` (datos de la app) cada 5 s si cambian.
- **Arranque (v1.8.0, medido en la Tab M11):** la pantalla de carga duraba **79 s** aunque el trabajo era de ~9 s: la skin se creaba con un `Loader` asíncrono, que solo avanza en el tiempo libre entre fotogramas, y las ondas ya se dibujaban. Ahora la skin se crea de golpe (`+android/main.qml`, «Skin … loaded in N ms» en el log, ~8 s) y la pantalla de carga se va a los **~12 s**. La ventana de Ajustes (todas las secciones, con los 169 mapeos) se crea en segundo plano 4 s después y está lista a los ~13 s; si se abre antes, muestra «Cargando los ajustes…» y termina en unos segundos. Además, el QML se carga del APK en vez de copiarse a `/sdcard/Mixxx/qml` en cada arranque (para desarrollo: crear `Mixxx/.qml-dev`), y el lienzo oculto de cables virtuales (`AudioRouter`) ya no se construye.

- **Stems** (v1.6.0, rama `stems-1.6`): hecho el paso a la v1.5.6, Booth (modo STEMS de los pads, cola y aviso compartidos en `StemsOverlay.qml`, también en Lite), Acerca de con Angelos y Dani Roman, y la separación ~25 % más rápida (etapas en paralelo, AAC en paralelo en los núcleos lentos). Falta:
  - **subir los modelos** (`.release/stems-models-v1/`) a la release `stems-models-v1` antes de publicar, o la descarga falla;
  - recortar ONNX Runtime a los operadores de los modelos (APK ~33 MB más pequeña).
- **Redibujados que quedan (Tab M11):** con las ondas a 45 fps, la ventana de Classic (vista DJ con biblioteca) se sigue redibujando ~74 veces por segundo y `QtThread` está al 94 %: algún elemento pide fotogramas continuamente (¿animaciones de VU, botones, `Behavior`?). Buscarlo con un perfil por elemento.
- **Rediseño de los pads de Classic:** el selector CUE / LOOP / STEM y la zona de pads se ven sobrecargados al lado de Booth (opinión del usuario al probar la v1.6.0). Pensar un diseño más limpio que sirva también en móviles.
- **SHIFT + LOAD en Booth:** sin probar.
- **Cuelgue del audio de la tablet** (HAL de audio / audioserver) al soltar la DDJ el driver USB propio: sin investigar.
- **Candidatos a enviarse a Mixxx:** el arreglo de uniforms de `rendergraph` (parpadeo de los loops) y el `i < numSamples` de `EngineEffectChain`.
- **Página de Audio:** puede marcar «Móvil / Bluetooth» estando en Híbrido.
- **Columna «#» en la vista de un USB entero:** muestra números sin significado.
- **`applySkinTheme`:** rompe los bindings internos de `Theme`.

## Orden propuesto

1. **v1.6.0:** los stems. Después, 2 y lo que falta de 4 (directo).
2. ~~**Booth y DDJ-400:** 8, 9, 10, 11, 13 y 15.~~ Hecho en la v1.5.5.
3. **Funciones:** 16, 17, 18, 19 y 23.
4. **Resto:** según lo que pidan los usuarios.

> _Fin de la sección escrita por IA._
