#include "soundio/soundmanager.h"

#include <qlogging.h>

#include <QLibrary>
#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>
#include <condition_variable>
#include <mutex>

#include "controllers/android.h"
#endif
#ifdef __ANDROID_USB_AUDIO__
#include "soundio/androidusbaudiodevice.h"
#include "soundio/sounddeviceandroidusb.h"
#endif
#include <QThread>
#include <QtGlobal>
#include <cstring> // for memcpy and strcmp
#include <memory>

#include "audio/types.h"
#include "control/controlobject.h"
#include "engine/enginemixer.h"
#include "moc_soundmanager.cpp"
#include "preferences/configobject.h"
#include "soundio/portaudioenumerator.h"
#include "soundio/sounddevice.h"
#include "soundio/sounddeviceenumerator.h"
#include "soundio/sounddevicenetwork.h"
#include "soundio/sounddevicenotfound.h"
#include "soundio/sounddeviceportaudio.h"
#include "soundio/soundmanagerconfig.h"
#include "soundio/soundmanagerutil.h"
#include "util/cmdlineargs.h"
#include "util/compatibility/qatomic.h"
#include "util/defs.h"
#include "util/sample.h"
#include "vinylcontrol/defs_vinylcontrol.h"

#ifdef __PIPEWIRE__
#include "soundio/pipewireenumerator.h"
#endif

namespace {

const QString kAppGroup = QStringLiteral("[App]");
const ConfigKey kAndroidUsbDirectModeKey =
        ConfigKey(QStringLiteral("[Soundcard]"), QStringLiteral("AndroidUsbDirectMode"));
const ConfigKey kAutoRouteControllerKey =
        ConfigKey(QStringLiteral("[Soundcard]"), QStringLiteral("AndroidAutoRouteController"));

#define CPU_OVERLOAD_DURATION 500 // in ms

struct DeviceMode {
    SoundDevicePointer pDevice;
    bool isInput;
    bool isOutput;
};

#ifdef __LINUX__
constexpr unsigned int kSleepSecondsAfterClosingDevice = 5;
#endif
} // anonymous namespace

SoundManager::SoundManager(
        UserSettingsPointer pConfig, EngineMixer* pEngineMixer)
        : m_pEngineMixer(pEngineMixer),
          m_pConfig(pConfig),
          m_config(this),
          m_pErrorDevice(nullptr),
          m_underflowHappened(0),
          m_underflowUpdateCount(0),
          m_audioLatencyOverloadCount(
                  kAppGroup, QStringLiteral("audio_latency_overload_count")),
          m_audioLatencyOverload(
                  kAppGroup, QStringLiteral("audio_latency_overload")),
          m_pNetworkStream(QSharedPointer<EngineNetworkStream>::create(2, 0)),
          m_pNetworkDevice(QSharedPointer<SoundDeviceNetwork>::create(
                  pConfig, this, m_pNetworkStream)),
          m_pipewireEnabled(m_pConfig->getValue(
                  ConfigKey(kAppGroup, QStringLiteral("pipewire")), false)) {
    // TODO(xxx) some of these ControlObject are not needed by soundmanager, or are unused here.
    // It is possible to take them out?
    m_pControlObjectSoundStatusCO = new ControlObject(
            ConfigKey("[SoundManager]", "status"));
    m_pControlObjectSoundStatusCO->set(SOUNDMANAGER_DISCONNECTED);

    m_pControlObjectVinylControlGainCO = new ControlObject(
            ConfigKey(VINYL_PREF_KEY, "gain"));

#ifdef __PIPEWIRE__
    if (isPipewireSelected()) {
        m_pEnumerator = std::make_unique<PipewireEnumerator>(m_pConfig, this);
    } else
#endif
    {
        m_pEnumerator = std::make_unique<PortAudioEnumerator>(m_pConfig, this);
    }

    queryDevices();

#if defined(Q_OS_ANDROID)
    if (!m_config.readFromDisk() || m_config.getOutputs().isEmpty()) {
        m_config.loadDefaults(this, SoundManagerConfig::API | SoundManagerConfig::DEVICES | SoundManagerConfig::OTHER);
        m_config.writeToDisk();
    }
#else
    if (!m_config.readFromDisk()) {
        m_config.loadDefaults(this, SoundManagerConfig::API | SoundManagerConfig::OTHER);
    }
#endif

    if (!isPipewireSelected()) {
        devicesEnumerated();
    }

    checkConfig();

#ifdef __ANDROID_USB_AUDIO__
    // USB sound cards driven by Mixxx can be plugged in and out at any time
    m_pAndroidUsbTimer = new QTimer(this);
    m_pAndroidUsbTimer->setSingleShot(true);
    m_pAndroidUsbTimer->setInterval(1000);
    connect(m_pAndroidUsbTimer,
            &QTimer::timeout,
            this,
            &SoundManager::slotAndroidUsbDevicesChanged);
    m_androidUsbListenerId = mixxx::android::addDevicesChangedListener([this]() {
        QMetaObject::invokeMethod(
                this, [this]() { m_pAndroidUsbTimer->start(); }, Qt::QueuedConnection);
    });
#endif

    // Don't write config to disk, yet -- it may be reset to defaults in case
    // previously configured devices were not found.
    // Write new config after MixxxMainWindow::noOutputDlg where the user has
    // a chance to keep the previous sound config (exit).
}

SoundManager::~SoundManager() {
#ifdef __ANDROID_USB_AUDIO__
    mixxx::android::removeDevicesChangedListener(m_androidUsbListenerId);
#endif
    // Clean up devices.
    const bool sleepAfterClosing = false;
    clearDeviceList(sleepAfterClosing);

    // vinyl control proxies and input buffers are freed in closeDevices, called
    // by clearDeviceList -- bkgood

    delete m_pControlObjectSoundStatusCO;
    delete m_pControlObjectVinylControlGainCO;

    const QList<CSAMPLE*> buffers = m_inputBuffers.values();
    for (CSAMPLE* pBuffer : buffers) {
        if (pBuffer != nullptr) {
            SampleUtil::free(pBuffer);
        }
    }

    m_inputBuffers.clear();
}

QList<SoundDevicePointer> SoundManager::getDeviceList(
        const QString& filterAPI, bool bOutputDevices, bool bInputDevices) const {
    //qDebug() << "SoundManager::getDeviceList";

    if (filterAPI == SoundManagerConfig::kAPINone) {
        return QList<SoundDevicePointer>();
    }

    // Create a list of sound devices filtered to match given API and
    // input/output.
    QList<SoundDevicePointer> filteredDeviceList;

    for (const auto& pDevice : m_pEnumerator->queryDevices()) {
        // Skip devices that don't match the API, don't have input channels when
        // we want input devices, or don't have output channels when we want
        // output devices. If searching for both input and output devices,
        // make sure to include any devices that have >0 channels.
        const bool hasOutputs = pDevice->getNumOutputChannels().isValid();
        const bool hasInputs = pDevice->getNumInputChannels().isValid();
        qDebug() << "SoundManager::getDeviceList" << pDevice->getHostAPI()
                 << pDevice->getDeviceId().debugName() << filterAPI
                 << pDevice->getNumOutputChannels()
                 << pDevice->getNumInputChannels();
        if (pDevice->getHostAPI() != filterAPI ||
                (bOutputDevices && !bInputDevices && !hasOutputs) ||
                (bInputDevices && !bOutputDevices && !hasInputs) ||
                (!hasInputs && !hasOutputs)) {
            continue;
        }

        filteredDeviceList.push_back(pDevice);
    }

    return filteredDeviceList;
}

QList<QString> SoundManager::getHostAPIList() const {
    return m_pEnumerator->getAPIs();
}

void SoundManager::closeDevices(
        [[maybe_unused]] bool sleepAfterClosing, [[maybe_unused]] bool async) {
    // sleepAfterClosing and async maybe unused depending on platform support
    // qDebug() << "SoundManager::closeDevices()";

#ifdef __LINUX__
    bool closed = false;
#endif
    if (m_pNetworkDevice->isOpen()) {
        m_pNetworkDevice->close();
    }

    for (const auto& pDevice : m_pEnumerator->queryDevices()) {
        if (pDevice->isOpen()) {
            // NOTE(rryan): As of 2009 (?) it has been safe to close() a SoundDevice
            // while callbacks are active.
            pDevice->close();
#ifdef __LINUX__
            closed = true;
#endif
        }
    }

#ifdef __LINUX__
    if (closed && sleepAfterClosing) {
        // Sleep for 5 sec to allow asynchronously sound APIs like "pulse" to free
        // its resources as well
        if (async) {
            // Async mode - the caller will wait for `devicesClosed` before
            // trying to reconfigure or reopen audio devices
            QTimer::singleShot(
                    std::chrono::seconds(kSleepSecondsAfterClosingDevice),
                    this,
                    &SoundManager::completeDevicesClosing);
            return;
        }
        // Sync mode, legacy - we sleep the current thread for 5 seconds
        QThread::sleep(kSleepSecondsAfterClosingDevice);
    } else if (!closed)
#endif
    {
        completeDevicesClosing();
    }
}

void SoundManager::completeDevicesClosing() {
    // TODO(rryan): Should we do this before SoundDevice::close()? No! Because
    // then the callback may be running when we call
    // onInputDisconnected/onOutputDisconnected.
    std::vector<SoundDevicePointer> devices = m_pEnumerator->queryDevices();
    devices.push_back(m_pNetworkDevice);

    for (const auto& pDevice : devices) {
        for (const auto& in : pDevice->inputs()) {
            // Need to tell all registered AudioDestinations for this AudioInput
            // that the input was disconnected.
            if (!isPipewireSelected()) {
                // PipeWire manages input/output configuration itself
                unconfigureInput(in);
            }
        }
        for (const auto& out: pDevice->outputs()) {
            // Need to tell all registered AudioSources for this AudioOutput
            // that the output was disconnected.
            if (!isPipewireSelected()) {
                // PipeWire manages input/output configuration itself
                unconfigureOutput(out);
            }
        }
    }

    // Indicate to the rest of Mixxx that sound is disconnected.
    m_pControlObjectSoundStatusCO->set(SOUNDMANAGER_DISCONNECTED);
    emit devicesClosed();
}

void SoundManager::clearDeviceList(bool sleepAfterClosing) {
    //qDebug() << "SoundManager::clearDeviceList()";

    // Close the devices first.
    closeDevices(sleepAfterClosing);

    // Empty out the list of devices we currently have.
    m_pErrorDevice.clear();

    // deinitialize in case of PortAudio so the devices are updated
    if (isPipewireSelected()) {
        m_pEnumerator->deinitialize();
    }
}

QList<mixxx::audio::SampleRate> SoundManager::getSampleRates(const QString& api) const {
    QList<mixxx::audio::SampleRate> samplerates =
            m_pEnumerator->getSampleRates(api == SoundManagerConfig::kAPIJack);
    if (!samplerates.empty()) {
        return samplerates;
    }

    return {
            mixxx::audio::SampleRate(44100),
            mixxx::audio::SampleRate(48000),
            mixxx::audio::SampleRate(96000),
    };
}

QList<mixxx::audio::SampleRate> SoundManager::getSampleRates() const {
    return getSampleRates(m_config.getAPI());
}

void SoundManager::queryDevices() {
    qDebug() << "SoundManager::queryDevices()";
    m_pEnumerator->initialize();
    // now tell the prefs that we updated the device list -- bkgood
#ifndef __PIPEWIRE__
    emit devicesUpdated();
#endif
}

void SoundManager::clearAndQueryDevices() {
    const bool sleepAfterClosing = true;
    clearDeviceList(sleepAfterClosing);
    queryDevices();
}

int SoundManager::getAndroidUsbDirectMode() const {
    return m_pConfig->getValue(kAndroidUsbDirectModeKey, 0);
}

void SoundManager::setAndroidUsbDirectMode(int mode) {
    m_pConfig->setValue(kAndroidUsbDirectModeKey, mode);
}

#ifdef __ANDROID_USB_AUDIO__
void SoundManager::slotAndroidUsbDevicesChanged() {
    auto* pEnumerator = dynamic_cast<PortAudioEnumerator*>(m_pEnumerator.get());
    if (!pEnumerator) {
        return;
    }
    std::vector<SoundDevicePointer> removed;
    if (!pEnumerator->refreshAndroidUsbDevices(&removed, false)) {
        return;
    }
    qInfo() << "USB sound cards changed, reconfiguring the audio devices";
    for (const auto& pDevice : removed) {
        if (pDevice->isOpen()) {
            pDevice->close();
        }
    }
    const bool sleepAfterClosing = false;
    closeDevices(sleepAfterClosing);
    setupDevices();
    emit devicesUpdated();
}
#endif

bool SoundManager::getAutoRouteController() const {
    return m_pConfig->getValue(kAutoRouteControllerKey, true);
}

void SoundManager::setAutoRouteController(bool enabled) {
    m_pConfig->setValue(kAutoRouteControllerKey, enabled);
}

#ifdef Q_OS_ANDROID
void SoundManager::autoRouteToController() {
    if (!getAutoRouteController()) {
        return;
    }
    // The Android enumerator suffixes USB devices with ": USB"
    SoundDevicePointer pController;
    for (const auto& pDevice : m_pEnumerator->queryDevices()) {
        if (pDevice->getNumOutputChannels() >= 4 &&
                pDevice->getDisplayName().endsWith(QStringLiteral(": USB")) &&
                (!pController ||
                        pDevice->getNumOutputChannels() > pController->getNumOutputChannels())) {
            pController = pDevice;
        }
    }
    if (!pController || m_config.getOutputs().contains(pController->getDeviceId())) {
        return;
    }

    qInfo() << "DJ controller sound card detected, routing Main to channels 1-2 "
               "and Headphones to channels 3-4 of"
            << pController->getDisplayName();
    m_config.clearOutputs();
    m_config.addOutput(pController->getDeviceId(),
            AudioOutput(AudioPathType::Main, 0, mixxx::audio::ChannelCount::stereo(), 0));
    m_config.addOutput(pController->getDeviceId(),
            AudioOutput(AudioPathType::Headphones, 2, mixxx::audio::ChannelCount::stereo(), 0));
    // Play at the controller's own rate to avoid resampling
    const auto sampleRate = pController->getDefaultSampleRate();
    if (sampleRate.isValid()) {
        m_config.setSampleRate(sampleRate);
    }
    // The cable splitter preset is not used with a controller
    ControlObject::set(ConfigKey(QStringLiteral("[Master]"), QStringLiteral("headSplit")), 0.0);
    m_config.writeToDisk();
}
#endif

SoundDeviceStatus SoundManager::setupDevices() {
    // NOTE(rryan): Big warning: This function is concurrent with calls to
    // pushBuffer and onDeviceOutputCallback until closeDevices() below.

    qDebug() << "SoundManager::setupDevices()";
    m_pControlObjectSoundStatusCO->set(SOUNDMANAGER_CONNECTING);
    SoundDeviceStatus status = SoundDeviceStatus::Ok;
    // NOTE(rryan): Do not clear m_pClkRefDevice here. If we didn't touch the
    // SoundDevice that is the clock reference, then it is safe to leave it as
    // it was. Clearing it causes the engine to stop being processed which
    // results in a stuttering noise (sometimes a loud buzz noise at low
    // latencies) when changing devices.
    //m_pClkRefDevice = NULL;
    m_pErrorDevice.clear();
    int outputDevicesOpened = 0;
    int physicalOutputDevicesOpened = 0;
    int inputDevicesOpened = 0;

    // NOTE(rryan): Documenting for future people touching this class. If you
    // would like to remove the fact that we close all the devices first and
    // then re-open them, I'm with you! The problem is that SoundDevicePortAudio
    // and SoundManager are not thread safe and the way that mutual exclusion
    // between the Qt main thread and the PortAudio callback thread is achieved
    // is that we shut off the PortAudio callbacks for all devices by closing
    // every device first. We then update all the SoundDevice settings
    // (configured AudioInputs/AudioOutputs) and then we re-open them.
    //
    // If you want to solve this issue, you should start by separating the PA
    // callback from the logic in SoundDevicePortAudio. They should communicate
    // via message passing over a request/response FIFO.

    // Instead of clearing m_pClkRefDevice and then assigning it directly,
    // compute the new one then atomically hand off below.
    SoundDevicePointer pNewMainClockRef;

    m_audioLatencyOverloadCount.set(0);

#if defined(Q_OS_ANDROID)
    if (m_config.getOutputs().isEmpty()) {
        qWarning() << "SoundManager on Android: No outputs configured in SoundManagerConfig. Loading defaults...";
        m_config.loadDefaults(this, SoundManagerConfig::API | SoundManagerConfig::DEVICES | SoundManagerConfig::OTHER);
        m_config.writeToDisk();
    }
#endif
#if defined(Q_OS_ANDROID)
    autoRouteToController();
    // A USB sound card driven directly by Mixxx only plays at the rates it
    // supports (e.g. 44.1 kHz for the DDJ-400), there is no resampling
#ifdef __ANDROID_USB_AUDIO__
    for (const auto& pDevice : m_pEnumerator->queryDevices()) {
        const auto* pUsbDevice = dynamic_cast<const SoundDeviceAndroidUsb*>(pDevice.data());
        if (!pUsbDevice || !m_config.getOutputs().contains(pDevice->getDeviceId())) {
            continue;
        }
        const int sampleRate = static_cast<int>(m_config.getSampleRate().value());
        if (!pUsbDevice->usbDevice()->sampleRates().contains(sampleRate)) {
            qInfo() << pDevice->getDisplayName() << "doesn't support" << sampleRate
                    << "Hz, using" << pDevice->getDefaultSampleRate();
            m_config.setSampleRate(pDevice->getDefaultSampleRate());
        }
    }
#endif
#endif
    // load with all configured devices.
    // all found devices are removed below
    QSet<SoundDeviceId> devicesNotFound = m_config.getDevices();

    // pair is isInput, isOutput
    QVector<DeviceMode> toOpen;
    bool haveOutput = false;
    // loop over all available devices

    std::vector<SoundDevicePointer> devices = m_pEnumerator->queryDevices();

    // here some network device conditions can be separated, currently simply
    // add it to the list of other devices
    devices.push_back(m_pNetworkDevice);
    for (const auto& pDevice : devices) {
        DeviceMode mode = {pDevice, false, false};
        pDevice->clearInputs();
        pDevice->clearOutputs();
        m_pErrorDevice = pDevice;
        const auto inputs = m_config.getInputs().values(pDevice->getDeviceId());
        for (const auto& in : inputs) {
            mode.isInput = true;
            // TODO(bkgood) look into allocating this with the frames per
            // buffer value from SMConfig
            status = pDevice->addInput(AudioInputBuffer{in, m_inputBuffers.value(in)});
            if (status != SoundDeviceStatus::Ok) {
                goto closeAndError;
            }

            if (!isPipewireSelected()) {
                // PipeWire manages input/output configuration itself
                configureInput(in);
            }
        }
        QList<AudioOutput> outputs =
                m_config.getOutputs().values(pDevice->getDeviceId());

        // Statically connect the Network Device to the Sidechain
        if (pDevice->getDeviceId().name == kNetworkDeviceInternalName) {
            AudioOutput out(AudioPathType::RecordBroadcast,
                    0,
                    mixxx::audio::ChannelCount::stereo(),
                    0);
            outputs.append(out);
            if (m_config.getForceNetworkClock() && !jackApiUsed()) {
                pNewMainClockRef = pDevice;
            }
        }

        for (const auto& out : std::as_const(outputs)) {
            mode.isOutput = true;
            if (pDevice->getDeviceId().name != kNetworkDeviceInternalName) {
                haveOutput = true;
            }
            // following keeps us from asking for a channel buffer EngineMixer
            // doesn't have -- bkgood
            const CSAMPLE* pBuffer = m_registeredSources.value(out)->buffer(out).data();
            if (pBuffer == nullptr) {
                qDebug() << "AudioSource returned null for" << out.getString();
                continue;
            }

            AudioOutputBuffer aob(out, pBuffer);
            status = pDevice->addOutput(aob);
            if (status != SoundDeviceStatus::Ok) {
                goto closeAndError;
            }

            if (!m_config.getForceNetworkClock() || jackApiUsed()) {
                if (out.getType() == AudioPathType::Main) {
                    pNewMainClockRef = pDevice;
                } else if ((out.getType() == AudioPathType::Deck ||
                                   out.getType() == AudioPathType::Bus) &&
                        !pNewMainClockRef) {
                    pNewMainClockRef = pDevice;
                }
            }

            if (!isPipewireSelected()) {
                // PipeWire manages input/output configuration itself
                configureOutput(out);
            }
        }

        if (mode.isInput || mode.isOutput) {
            pDevice->setSampleRate(m_config.getSampleRate());
            pDevice->setConfigFramesPerBuffer(m_config.getFramesPerBuffer());
            toOpen.append(mode);
        }
    }

    for (const auto& mode: toOpen) {
        SoundDevicePointer pDevice = mode.pDevice;
        m_pErrorDevice = pDevice;

        // Don't use network clock unless forced with PipeWire
        // as PipeWire callbacks run even without any devices configured
        const bool isNetworkDevice = pDevice->getDeviceId().name == kNetworkDeviceInternalName;
        const bool deviceAllowedClockReference = isPipewireSelected() != isNetworkDevice;

        // If we have not yet set a clock source then we use the first
        // output pDevice (preferring real physical devices over network stream)
        if (pNewMainClockRef.isNull() &&
                (!haveOutput || mode.isOutput) && deviceAllowedClockReference && !isNetworkDevice) {
            pNewMainClockRef = pDevice;
            qWarning() << "Output sound device clock reference not set! Using"
                       << pDevice->getDisplayName();
        }

        int syncBuffers = m_config.getSyncBuffers();
        // If we are in safe mode and using experimental polling support, use
        // the default of 2 sync buffers instead.
        if (CmdlineArgs::Instance().getSafeMode() && syncBuffers == 0) {
            syncBuffers = 2;
        }
        status = pDevice->open(pNewMainClockRef == pDevice, syncBuffers);
        if (status != SoundDeviceStatus::Ok) {
#if defined(Q_OS_ANDROID)
            // Never end up without any sound: skip the device and let the
            // fallback below open another one
            qWarning() << "Unable to open" << pDevice->getDisplayName() << ":"
                       << pDevice->getError();
            if (pNewMainClockRef == pDevice) {
                pNewMainClockRef.clear();
            }
            status = SoundDeviceStatus::Ok;
            continue;
#else
            goto closeAndError;
#endif
        }
        devicesNotFound.remove(pDevice->getDeviceId());
        if (mode.isOutput) {
            ++outputDevicesOpened;
            if (pDevice->getDeviceId().name != kNetworkDeviceInternalName) {
                ++physicalOutputDevicesOpened;
            }
        }
        if (mode.isInput) {
            ++inputDevicesOpened;
        }
    }

#if defined(Q_OS_ANDROID)
    if (!haveOutput || physicalOutputDevicesOpened == 0) {
        qWarning() << "SoundManager on Android: No physical output device opened! Forcing first available output device...";
        SoundDevicePointer targetDevice;
        for (const auto& pDevice : devices) {
            if (pDevice && pDevice->getNumOutputChannels() > 0 && pDevice->getDeviceId().name != kNetworkDeviceInternalName) {
                if (pDevice->getDisplayName().contains(QStringLiteral("earpiece"), Qt::CaseInsensitive)) {
                    continue;
                }
                if (pDevice->getHostAPI().contains(QStringLiteral("Oboe"), Qt::CaseInsensitive)) {
                    // Prefer the built-in speaker, which is always present
                    if (pDevice->getDisplayName().contains(QStringLiteral("Speaker"))) {
                        targetDevice = pDevice;
                        break;
                    }
                    if (!targetDevice || !targetDevice->getHostAPI().contains(
                                                 QStringLiteral("Oboe"), Qt::CaseInsensitive)) {
                        targetDevice = pDevice;
                    }
                    continue;
                }
                if (!targetDevice) {
                    targetDevice = pDevice;
                }
            }
        }

        if (targetDevice) {
            const auto channelCount = targetDevice->getNumOutputChannels() >= 2
                    ? mixxx::audio::ChannelCount::stereo()
                    : mixxx::audio::ChannelCount::mono();
            AudioOutput forcedOut(AudioPathType::Main, 0, channelCount, 0);
            auto pSource = m_registeredSources.value(forcedOut);
            const CSAMPLE* pBuffer = pSource ? pSource->buffer(forcedOut).data() : nullptr;
            if (pBuffer) {
                targetDevice->clearOutputs();
                AudioOutputBuffer aob(forcedOut, pBuffer);
                targetDevice->addOutput(aob);
                targetDevice->setSampleRate(m_config.getSampleRate());
                targetDevice->setConfigFramesPerBuffer(m_config.getFramesPerBuffer());
                if (targetDevice->open(true, m_config.getSyncBuffers()) == SoundDeviceStatus::Ok) {
                    pNewMainClockRef = targetDevice;
                    ++outputDevicesOpened;
                    haveOutput = true;
                    devicesNotFound.remove(targetDevice->getDeviceId());
                    // Not saved to the configuration on purpose: the
                    // configured device (e.g. a DJ controller that isn't
                    // plugged in yet) must be used again next time.
                    qInfo() << "Successfully opened fallback Android audio device:" << targetDevice->getDisplayName();
                }
            }
        }
    }
#endif

    if (pNewMainClockRef) {
        qDebug() << "Using" << pNewMainClockRef->getDisplayName()
                 << "as output sound device clock reference";
    } else {
        qWarning() << "No output devices opened, no clock reference device set";
    }

    qDebug() << outputDevicesOpened << "output sound devices opened";
    qDebug() << inputDevicesOpened << "input sound devices opened";

#if defined(Q_OS_ANDROID)
    // Android attenuates USB sound cards with the media volume. DJ
    // controllers have their own level knobs, so play at full scale.
    for (const auto& mode : std::as_const(toOpen)) {
        if (mode.isOutput &&
                mode.pDevice->getDisplayName().endsWith(QStringLiteral(": USB"))) {
            QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
                QJniObject context = QNativeInterface::QAndroidApplication::context();
                QJniObject::callStaticMethod<jboolean>("org/mixxx/AudioHelper",
                        "maximizeMediaVolumeForUsb",
                        "(Landroid/content/Context;)Z",
                        context.object<jobject>());
            });
            break;
        }
    }
#endif
    for (const auto& device : std::as_const(devicesNotFound)) {
        qWarning() << device << "not found";
    }

    m_pControlObjectSoundStatusCO->set(
            outputDevicesOpened > 0 ?
                    SOUNDMANAGER_CONNECTED : SOUNDMANAGER_DISCONNECTED);

    // returns OK if we were able to open all the devices the user wanted
    if (devicesNotFound.isEmpty() || pipewireSkipConfig()) {
        emit devicesSetup();
        return SoundDeviceStatus::Ok;
    }
    m_pErrorDevice = SoundDevicePointer(
            new SoundDeviceNotFound(devicesNotFound.constBegin()->name));
    return SoundDeviceStatus::ErrorDeviceCount;

closeAndError:
    const bool sleepAfterClosing = false;
    closeDevices(sleepAfterClosing);
    return status;
}

SoundDevicePointer SoundManager::getErrorDevice() const {
    return m_pErrorDevice;
}

QString SoundManager::getErrorDeviceName() const {
    SoundDevicePointer pDevice = getErrorDevice();
    if (pDevice) {
        return pDevice->getDisplayName();
    }
    return tr("a device");
}

QString SoundManager::getLastErrorMessage(SoundDeviceStatus status) const {
    QString error;
    QString deviceName(tr("a device"));
    QString detailedError(tr("An unknown error occurred"));
    SoundDevicePointer pDevice = getErrorDevice();
    if (pDevice) {
        deviceName = pDevice->getDisplayName();
        detailedError = pDevice->getError();
    }
    switch (status) {
    case SoundDeviceStatus::ErrorDuplicateOutputChannel:
        error = tr("Two outputs cannot share channels on \"%1\"").arg(deviceName);
        break;
    default:
        error = tr("Error opening \"%1\"").arg(deviceName) + "\n" + detailedError;
        break;
    }
    return error;
}

SoundManagerConfig SoundManager::getConfig() const {
    return m_config;
}

void SoundManager::closeActiveConfig(bool async) {
    // Close open devices. After this call we will not get any more
    // onDeviceOutputCallback() or pushBuffer() calls because all the
    // SoundDevices are closed. closeDevices() blocks and can take a while.
    const bool sleepAfterClosing = true;
    closeDevices(sleepAfterClosing, async);
}

SoundDeviceStatus SoundManager::setConfig(const SoundManagerConfig& config) {
    SoundDeviceStatus status = SoundDeviceStatus::Ok;
    m_config = config;
    checkConfig();

    closeActiveConfig();

    status = setupDevices();
    if (status == SoundDeviceStatus::Ok) {
        m_config.writeToDisk();
    }
    return status;
}

void SoundManager::checkConfig() {
    if (!m_config.checkAPI()) {
        m_config.setAPI(SoundManagerConfig::kAPINone);
        m_config.loadDefaults(this, SoundManagerConfig::API | SoundManagerConfig::DEVICES);
    }
    if (!m_config.checkSampleRate(*this)) {
        m_config.setSampleRate(SoundManagerConfig::kFallbackSampleRate);
        m_config.loadDefaults(this, SoundManagerConfig::OTHER);
    }

    // Even if we have a two-deck skin, if someone has configured a deck > 2
    // then the configuration needs to know about that extra deck.
    m_config.setCorrectDeckCount(getConfiguredDeckCount());
    // latency checks itself for validity on SMConfig::setLatency()
}

void SoundManager::onDeviceOutputCallback(const SINT iFramesPerBuffer) {
    // Produce a block of samples for output. EngineMixer expects stereo
    // samples so multiply iFramesPerBuffer by 2.
    m_pEngineMixer->process(iFramesPerBuffer * 2);
}

void SoundManager::pushInputBuffer(const AudioInput& input, const SINT iFramesPerBuffer) {
    CSAMPLE* pInputBuffer = m_inputBuffers.value(input);
    for (auto it = m_registeredDestinations.constFind(input);
            it != m_registeredDestinations.constEnd() && it.key() == input;
            ++it) {
        it.value()->receiveBuffer(input, pInputBuffer, iFramesPerBuffer);
    }
}
void SoundManager::pushInputBuffers(const QList<AudioInputBuffer>& inputs,
                                    const SINT iFramesPerBuffer) {
   for (QList<AudioInputBuffer>::ConstIterator i = inputs.begin(),
                 e = inputs.end(); i != e; ++i) {
        const AudioInputBuffer& in = *i;
        CSAMPLE* pInputBuffer = in.getBuffer();
        for (auto it = m_registeredDestinations.constFind(in);
                it != m_registeredDestinations.constEnd() && it.key() == in;
                ++it) {
            it.value()->receiveBuffer(in, pInputBuffer, iFramesPerBuffer);
        }
   }
}

void SoundManager::writeProcess(SINT framesPerBuffer) const {
    for (const auto& pDevice : m_pEnumerator->queryDevices()) {
        if (pDevice) {
            pDevice->writeProcess(framesPerBuffer);
        }
    }
    m_pNetworkDevice->writeProcess(framesPerBuffer);
}

void SoundManager::readProcess(SINT framesPerBuffer) const {
    for (const auto& pDevice : m_pEnumerator->queryDevices()) {
        if (pDevice) {
            pDevice->readProcess(framesPerBuffer);
        }
    }
    m_pNetworkDevice->readProcess(framesPerBuffer);
}

void SoundManager::registerOutput(const AudioOutput& output, AudioSource* src) {
    VERIFY_OR_DEBUG_ASSERT(!m_registeredSources.contains(output)) {
        return;
    }
    m_registeredSources.insert(output, src);
    emit outputRegistered(output, src);
}

void SoundManager::registerInput(const AudioInput& input, AudioDestination* dest) {
    // Vinyl control inputs are registered twice, once for timecode and once for
    // passthrough, each with different outputs. So unlike outputs, do not assert
    // that the input has not been registered yet.
    m_registeredDestinations.insert(input, dest);

    if (!m_inputBuffers.contains(input)) {
        m_inputBuffers.insert(input, SampleUtil::alloc(kMaxEngineSamples));
    }

    emit inputRegistered(input, dest);
}

QList<AudioOutput> SoundManager::registeredOutputs() const {
    return m_registeredSources.keys();
}

QList<AudioInput> SoundManager::registeredInputs() const {
    return m_registeredDestinations.keys();
}

void SoundManager::setConfiguredDeckCount(int count) {
    if (getConfiguredDeckCount() == count) {
        // Unchanged
        return;
    }
    m_config.setDeckCount(count);
    checkConfig();
    m_config.writeToDisk();
}

int SoundManager::getConfiguredDeckCount() const {
    return m_config.getDeckCount();
}

void SoundManager::processUnderflowHappened(SINT framesPerBuffer) {
    if (m_underflowUpdateCount == 0) {
        if (atomicLoadRelaxed(m_underflowHappened)) {
            m_audioLatencyOverload.set(1.0);
            m_audioLatencyOverloadCount.set(
                    m_audioLatencyOverloadCount.get() + 1);
            m_underflowUpdateCount = CPU_OVERLOAD_DURATION *
                    m_config.getSampleRate() / framesPerBuffer / 1000;

            m_underflowHappened = 0; // resetting here is not thread safe,
                                     // but that is OK, because we count only
                                     // 1 underflow each 500 ms
        } else {
            m_audioLatencyOverload.set(0.0);
        }
    } else {
        --m_underflowUpdateCount;
    }
}

void SoundManager::addDevice(SoundDevicePointer pDevice) {
    qDebug() << "SoundManager::addDevice" << pDevice->getDisplayName();
    emit deviceAdded(pDevice);
}

void SoundManager::removeDevice(SoundDevicePointer pDevice) {
    qDebug() << "SoundManager::removeDevice" << pDevice->getDisplayName();
    emit deviceRemoved(pDevice);
}

void SoundManager::updateDeviceChannels(SoundDevicePointer pDevice) {
    emit deviceChannelsUpdated(pDevice);
}

void SoundManager::configureInput(const AudioInput& input) {
    // Check if any AudioDestination is registered for this AudioInput
    // and call the onInputConnected method.
    qWarning() << "SoundManager::configureInput";
    auto [first, last] = m_registeredDestinations.equal_range(input);

    for (auto it = first; it != last; ++it) {
        it.value()->onInputConfigured(input);
        m_pEngineMixer->onInputConnected(input);
    }
}

void SoundManager::unconfigureInput(const AudioInput& input) {
    qWarning() << "SoundManager::unconfigureInput";
    auto [first, last] = m_registeredDestinations.equal_range(input);
    for (auto it = first; it != last; ++it) {
        it.value()->onInputUnconfigured(input);
        m_pEngineMixer->onInputDisconnected(input);
    }
}

void SoundManager::configureOutput(const AudioOutput& output) {
    // Check if any AudioSource is registered for this AudioOutput and
    // call the onOutputConnected method.
    qWarning() << "SoundManager::configureOutput";
    m_registeredSources.value(output)->onOutputConnected(output);
}

void SoundManager::unconfigureOutput(const AudioOutput& output) {
    qWarning() << "SoundManager::unconfigureOutput";
    m_registeredSources.value(output)->onOutputDisconnected(output);
}

void SoundManager::devicesEnumerated() {
    qDebug() << "SoundManager::devicesEnumerated";
    for (const auto& device : m_pEnumerator->queryDevices()) {
        qDebug() << device->getDisplayName();
    }

    // currently PipeWire has no sane default device options, we need to obtain
    // it from metadata, and select it in SoundManagerConfig::loadDefaults
    if (!m_config.validateDevices() && !isPipewireSelected()) {
        qDebug() << "!m_config.validateDevices()";
        m_config.loadDefaults(this, SoundManagerConfig::DEVICES);
    }
}

void SoundManager::invalidateConfig() {
    qDebug() << "SoundManager::invalidateConfig";
    emit configInvalidated();
}
