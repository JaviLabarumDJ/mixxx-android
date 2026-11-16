#include "qml/qmlcontrollerlearningproxy.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include "controllers/controllermanager.h"
#include "controllers/learningutils.h"
#include "controllers/midi/legacymidicontrollermapping.h"
#include "controllers/midi/midicontroller.h"
#include "controllers/midi/midiutils.h"
#include "moc_qmlcontrollerlearningproxy.cpp"
#include "qml/qmlpreferencesproxy.h"

namespace mixxx {
namespace qml {

namespace {

// A control is learned once its messages stop for this long (a knob turned
// sends many)
constexpr int kSettleMillis = 800;

QList<MidiController*> openMidiControllers() {
    QList<MidiController*> result;
    const auto pManager = QmlControllerManagerProxy::manager();
    if (!pManager) {
        return result;
    }
    const auto controllers = pManager->getControllers();
    for (Controller* pController : controllers) {
        auto* pMidi = qobject_cast<MidiController*>(pController);
        if (pMidi && pMidi->isOpen()) {
            result.append(pMidi);
        }
    }
    return result;
}

QString describe(const MidiInputMappings& mappings) {
    QStringList parts;
    for (const MidiInputMapping& mapping : mappings) {
        const MidiOpCode opCode = MidiUtils::opCodeFromStatus(mapping.key.status);
        const QString number = QStringLiteral("0x%1").arg(
                QString::number(mapping.key.control, 16).toUpper().rightJustified(2, '0'));
        QString kind;
        switch (opCode) {
        case MidiOpCode::NoteOn:
        case MidiOpCode::NoteOff:
            kind = QStringLiteral("note %1").arg(number);
            break;
        case MidiOpCode::ControlChange:
            kind = QStringLiteral("CC %1").arg(number);
            break;
        case MidiOpCode::PitchBendChange:
            kind = QStringLiteral("pitch bend");
            break;
        default:
            kind = number;
            break;
        }
        parts.append(QStringLiteral("%1, ch %2")
                             .arg(kind)
                             .arg(MidiUtils::channelFromStatus(mapping.key.status) + 1));
    }
    parts.removeDuplicates();
    return parts.join(QStringLiteral(" · "));
}

// The function prefix of the script of learned mappings
const QString kScriptPrefix = QStringLiteral("LearnedMap");

/// A note off as the note on of its note, other messages as they are
MidiKey normalized(MidiKey key) {
    if (MidiUtils::opCodeFromStatus(key.status) == MidiOpCode::NoteOff) {
        return MidiKey(static_cast<unsigned char>(0x90 | (key.status & 0x0F)), key.control);
    }
    return key;
}

bool isNote(MidiKey key) {
    return MidiUtils::opCodeFromStatus(key.status) == MidiOpCode::NoteOn;
}

QString describeKey(MidiKey key) {
    const QString number = QStringLiteral("0x%1").arg(
            QString::number(key.control, 16).toUpper().rightJustified(2, '0'));
    return QStringLiteral("%1 %2, ch %3")
            .arg(isNote(key) ? QStringLiteral("note") : QStringLiteral("CC"),
                    number)
            .arg(MidiUtils::channelFromStatus(key.status) + 1);
}

/// The JavaScript of the learned mappings: SHIFT layers, long and double
/// presses, combinations, toggles, knob modes and lights. The entries and the
/// SHIFT buttons are written before it by writeScript().
const char* const kScriptBody = R"JS(
LearnedMap.LONG_MS = 500;
LearnedMap.DOUBLE_MS = 300;
// Functions that a button switches on and off when its mode is "auto"
LearnedMap.toggles = ["play", "sync_enabled", "keylock", "quantize", "slip_enabled", "pfl", "mute",
    "reverse", "scene_on", "xpad_overdub", "fx_source_input", "fx_source_xpad", "enabled"];
LearnedMap.held = {};
LearnedMap.state = {};

LearnedMap.keyOf = function(status, control) {
    let type = status & 0xF0;
    if (type === 0x80) {
        type = 0x90;
    }
    return (type | (status & 0x0F)) + ":" + control;
};

LearnedMap.isToggle = function(entry) {
    if (entry.buttonMode === "toggle") {
        return true;
    }
    if (entry.buttonMode === "momentary") {
        return false;
    }
    return LearnedMap.toggles.indexOf(entry.item) >= 0;
};

LearnedMap.down = function(entry) {
    if (LearnedMap.isToggle(entry)) {
        script.toggleControl(entry.group, entry.item);
    } else {
        engine.setValue(entry.group, entry.item, 1);
    }
};

LearnedMap.up = function(entry) {
    if (!LearnedMap.isToggle(entry)) {
        engine.setValue(entry.group, entry.item, 0);
    }
};

LearnedMap.pulse = function(entry) {
    LearnedMap.down(entry);
    LearnedMap.up(entry);
};

LearnedMap.init = function() {
    LearnedMap.entries.forEach(function(entry) {
        if (!entry.button && entry.knobMode === "soft") {
            engine.softTakeover(entry.group, entry.item, true);
        }
        if (entry.button && entry.led && entry.trigger === "press") {
            engine.makeConnection(entry.group, entry.item, function(value) {
                midi.sendShortMsg(entry.status, entry.control, value > 0 ? 0x7F : 0x00);
            }).trigger();
        }
    });
};

LearnedMap.shutdown = function() {
    LearnedMap.entries.forEach(function(entry) {
        if (entry.button && entry.led) {
            midi.sendShortMsg(entry.status, entry.control, 0x00);
        }
    });
};

/// The layer of the held SHIFT buttons
LearnedMap.layer = function() {
    if (LearnedMap.shifts[0] && LearnedMap.held[LearnedMap.shifts[0]]) {
        return "shift1";
    }
    if (LearnedMap.shifts[1] && LearnedMap.held[LearnedMap.shifts[1]]) {
        return "shift2";
    }
    return "press";
};

LearnedMap.knob = function(entry, value, status) {
    if (entry.knobMode === "relative") {
        const delta = value < 64 ? value : value - 128;
        if (entry.item === "jog" || entry.group === "[Library]") {
            engine.setValue(entry.group, entry.item, delta);
        } else {
            const parameter = engine.getParameter(entry.group, entry.item) + delta / 100;
            engine.setParameter(entry.group, entry.item, Math.max(0, Math.min(1, parameter)));
        }
        return;
    }
    let parameter = value / 127;
    if (entry.knobMode === "invert") {
        parameter = 1 - parameter;
    }
    engine.setParameter(entry.group, entry.item, parameter);
};

LearnedMap.input = function(channel, control, value, status) {
    const key = LearnedMap.keyOf(status, control);
    const type = status & 0xF0;
    const pressed = (type === 0x90 || type === 0xB0) && value > 0;
    if (type === 0x90 || type === 0x80 || type === 0xB0) {
        LearnedMap.held[key] = pressed;
    }
    if (LearnedMap.shifts.indexOf(key) >= 0) {
        return;
    }
    const entries = LearnedMap.entries.filter(function(entry) {
        return entry.key === key;
    });
    if (entries.length === 0) {
        return;
    }
    const layer = LearnedMap.layer();
    if (!entries[0].button) {
        const knob = entries.find(function(entry) {
            return entry.trigger === layer;
        }) || entries.find(function(entry) {
            return entry.trigger === "press";
        });
        if (knob) {
            LearnedMap.knob(knob, value, status);
        }
        return;
    }

    const state = LearnedMap.state[key] || (LearnedMap.state[key] = {});
    const of = function(trigger) {
        return entries.filter(function(entry) {
            return entry.trigger === trigger;
        });
    };
    if (pressed) {
        // A combination: the other button is held
        const combos = entries.filter(function(entry) {
            return entry.trigger === "combo" && LearnedMap.held[entry.combo];
        });
        if (combos.length > 0) {
            combos.forEach(LearnedMap.down);
            state.active = combos;
            return;
        }
        const longs = of("long");
        const doubles = of("double");
        if (layer !== "press" || (longs.length === 0 && doubles.length === 0)) {
            state.active = of(layer);
            state.active.forEach(LearnedMap.down);
            return;
        }
        // Waiting to know whether it is a long, double or plain press
        state.pending = true;
        state.longFired = false;
        if (longs.length > 0) {
            state.longTimer = engine.beginTimer(LearnedMap.LONG_MS, function() {
                state.longTimer = 0;
                state.longFired = true;
                longs.forEach(LearnedMap.down);
            }, true);
        }
        return;
    }

    if (state.active) {
        state.active.forEach(LearnedMap.up);
        state.active = null;
        return;
    }
    if (!state.pending) {
        return;
    }
    state.pending = false;
    if (state.longFired) {
        of("long").forEach(LearnedMap.up);
        return;
    }
    if (state.longTimer) {
        engine.stopTimer(state.longTimer);
        state.longTimer = 0;
    }
    const doubles = of("double");
    if (doubles.length === 0) {
        of("press").forEach(LearnedMap.pulse);
        return;
    }
    if (state.tapTimer) {
        engine.stopTimer(state.tapTimer);
        state.tapTimer = 0;
        doubles.forEach(LearnedMap.pulse);
        return;
    }
    state.tapTimer = engine.beginTimer(LearnedMap.DOUBLE_MS, function() {
        state.tapTimer = 0;
        of("press").forEach(LearnedMap.pulse);
    }, true);
};
)JS";

} // namespace

QmlControllerLearningProxy::QmlControllerLearningProxy(QObject* pParent)
        : QObject(pParent),
          m_listening(false),
          m_listenShift(-1) {
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(kSettleMillis);
    connect(&m_settleTimer, &QTimer::timeout, this, &QmlControllerLearningProxy::slotSettled);
    if (const auto pManager = QmlControllerManagerProxy::manager()) {
        connect(pManager.get(),
                &ControllerManager::devicesChanged,
                this,
                &QmlControllerLearningProxy::controllersChanged);
    }
}

QmlControllerLearningProxy::~QmlControllerLearningProxy() {
    disconnectController();
}

// static
QmlControllerLearningProxy* QmlControllerLearningProxy::create(QQmlEngine* pQmlEngine, QJSEngine*) {
    return new QmlControllerLearningProxy(pQmlEngine);
}

QStringList QmlControllerLearningProxy::controllers() const {
    QStringList names;
    for (MidiController* pController : openMidiControllers()) {
        names.append(pController->getName());
    }
    return names;
}

QStringList QmlControllerLearningProxy::shifts() const {
    QStringList result;
    for (const auto& shift : m_shifts) {
        result.append(shift ? describeKey(*shift) : QString());
    }
    return result;
}

QStringList QmlControllerLearningProxy::learned() const {
    QStringList result;
    for (const ConfigKey& key : m_learnedKeys) {
        result.append(key.group + QLatin1Char(',') + key.item);
    }
    return result;
}

void QmlControllerLearningProxy::connectController(MidiController* pController) {
    m_pController = pController;
    connect(pController,
            &MidiController::messageReceived,
            this,
            &QmlControllerLearningProxy::slotMessageReceived);
    // On the controller thread, like DlgControllerLearning
    connect(this,
            &QmlControllerLearningProxy::startLearningRequested,
            pController,
            &MidiController::startLearning);
    connect(this,
            &QmlControllerLearningProxy::stopLearningRequested,
            pController,
            &MidiController::stopLearning);
    connect(this,
            &QmlControllerLearningProxy::learnRequested,
            pController,
            &MidiController::learnTemporaryInputMappings);
    connect(this,
            &QmlControllerLearningProxy::clearRequested,
            pController,
            &MidiController::clearTemporaryInputMappings);
    emit startLearningRequested();
}

void QmlControllerLearningProxy::disconnectController() {
    if (!m_pController) {
        return;
    }
    emit clearRequested();
    emit stopLearningRequested();
    disconnect(m_pController, nullptr, this, nullptr);
    disconnect(this, nullptr, m_pController, nullptr);
    m_pController.clear();
}

bool QmlControllerLearningProxy::start(int index) {
    const auto controllers = openMidiControllers();
    if (index < 0 || index >= controllers.size()) {
        return false;
    }
    cancel();
    connectController(controllers[index]);
    emit activeChanged();
    return true;
}

void QmlControllerLearningProxy::listen(const QString& group, const QString& key) {
    if (!m_pController) {
        return;
    }
    m_listenKey = ConfigKey(group, key);
    m_listenOptions.clear();
    m_listenShift = -1;
    m_messages.clear();
    m_settleTimer.stop();
    m_listening = true;
    emit listeningChanged();
}

void QmlControllerLearningProxy::listenWith(
        const QString& group, const QString& key, const QVariantMap& options) {
    listen(group, key);
    m_listenOptions = options;
}

void QmlControllerLearningProxy::listenShift(int which) {
    if (!m_pController || which < 0 || which > 1) {
        return;
    }
    listen(QString(), QString());
    m_listenShift = which;
}

void QmlControllerLearningProxy::clearShift(int which) {
    if (which < 0 || which > 1) {
        return;
    }
    m_shifts[which].reset();
    emit shiftsChanged();
    emit learnedChanged();
}

void QmlControllerLearningProxy::stopListening() {
    m_settleTimer.stop();
    m_messages.clear();
    if (m_listening) {
        m_listening = false;
        emit listeningChanged();
    }
}

void QmlControllerLearningProxy::slotMessageReceived(
        unsigned char status, unsigned char control, unsigned char value) {
    const MidiOpCode opCode = MidiUtils::opCodeFromStatus(status);
    m_lastMessage = MidiUtils::isMessageTwoBytes(opCode)
            ? QStringLiteral("0x%1 0x%2 0x%3")
                      .arg(QString::number(status, 16).toUpper(),
                              QString::number(control, 16).toUpper().rightJustified(2, '0'),
                              QString::number(value, 16).toUpper().rightJustified(2, '0'))
            : QStringLiteral("0x%1 0x%2")
                      .arg(QString::number(status, 16).toUpper(),
                              QString::number(control, 16).toUpper().rightJustified(2, '0'));
    emit lastMessageChanged();
    if (!m_listening) {
        return;
    }
    m_messages.append(qMakePair(MidiKey(status, control), value));
    m_settleTimer.start();
}

void QmlControllerLearningProxy::slotSettled() {
    if (!m_listening || m_messages.isEmpty()) {
        return;
    }
    // The buttons pressed, in order, and the values of each control
    QList<MidiKey> pressed;
    QHash<quint16, QSet<int>> values;
    MidiKey lastKey;
    for (const auto& [midiKey, value] : std::as_const(m_messages)) {
        const MidiKey key = normalized(midiKey);
        lastKey = key;
        const MidiOpCode opCode = MidiUtils::opCodeFromStatus(midiKey.status);
        values[key.key].insert(value);
        const bool press = (opCode == MidiOpCode::NoteOn || opCode == MidiOpCode::ControlChange) &&
                value > 0;
        if (press && !std::any_of(pressed.begin(), pressed.end(), [&key](const MidiKey& other) {
                return other.key == key.key;
            })) {
            pressed.append(key);
        }
    }

    if (m_listenShift >= 0) {
        const int which = m_listenShift;
        m_listenShift = -1;
        m_listening = false;
        m_messages.clear();
        emit listeningChanged();
        if (pressed.isEmpty()) {
            emit controlNotLearned(QString(), QString());
            return;
        }
        m_shifts[which] = pressed.first();
        emit shiftsChanged();
        emit learnedChanged();
        emit controlLearned(QString(),
                QStringLiteral("shift%1").arg(which + 1),
                describeKey(pressed.first()));
        return;
    }

    if (!m_listenOptions.isEmpty()) {
        Entry entry;
        entry.key = m_listenKey;
        entry.trigger = m_listenOptions.value(QStringLiteral("trigger"), QStringLiteral("press")).toString();
        entry.buttonMode = m_listenOptions.value(QStringLiteral("buttonMode"), QStringLiteral("auto")).toString();
        entry.knobMode = m_listenOptions.value(QStringLiteral("knobMode"), QStringLiteral("normal")).toString();
        entry.led = m_listenOptions.value(QStringLiteral("led")).toBool();
        if (entry.needsScript()) {
            m_listening = false;
            m_messages.clear();
            emit listeningChanged();
            const bool combo = entry.trigger == QLatin1String("combo");
            if (combo ? pressed.size() < 2 : (pressed.isEmpty() && values.isEmpty())) {
                emit controlNotLearned(entry.key.group, entry.key.item);
                return;
            }
            if (combo) {
                entry.combo = pressed.first();
                entry.midi = pressed.last();
            } else if (!pressed.isEmpty()) {
                entry.midi = pressed.last();
            } else {
                // A knob: only values moving, no press
                entry.midi = lastKey;
            }
            // A control change of more than on and off values is a knob
            const QSet<int> seen = values.value(entry.midi.key);
            entry.button = isNote(entry.midi) ||
                    std::all_of(seen.begin(), seen.end(), [](int value) {
                        return value == 0 || value == 127;
                    });
            for (int i = m_entries.size() - 1; i >= 0; --i) {
                if (m_entries[i].key == entry.key && m_entries[i].trigger == entry.trigger) {
                    m_entries.removeAt(i);
                }
            }
            m_entries.append(entry);
            m_learnedKeys.removeAll(entry.key);
            m_learnedKeys.append(entry.key);
            emit learnedChanged();
            QString description = describeKey(entry.midi);
            if (combo) {
                description = describeKey(entry.combo) + QStringLiteral(" + ") + description;
            }
            emit controlLearned(entry.key.group,
                    entry.key.item,
                    description + QStringLiteral(" · ") + entry.trigger);
            return;
        }
    }
    const ConfigKey key = m_listenKey;
    const MidiInputMappings mappings = LearningUtils::guessMidiInputMappings(key, m_messages);
    m_listening = false;
    m_messages.clear();
    emit listeningChanged();
    if (mappings.isEmpty()) {
        emit controlNotLearned(key.group, key.item);
        return;
    }
    // A function learned again replaces what it had
    for (int i = m_learnedMappings.size() - 1; i >= 0; --i) {
        const auto& existing = m_learnedMappings[i];
        if (std::holds_alternative<ConfigKey>(existing.control) &&
                std::get<ConfigKey>(existing.control) == key) {
            m_learnedMappings.removeAt(i);
        }
    }
    m_learnedMappings.append(mappings);
    // Also as an entry, for the script when it shares its control with
    // functions learned with options
    {
        Entry entry;
        entry.key = key;
        entry.midi = normalized(mappings.first().key);
        entry.button = isNote(entry.midi) || mappings.first().options.testFlag(MidiOption::Button);
        entry.trigger = QStringLiteral("press");
        entry.buttonMode = QStringLiteral("auto");
        entry.knobMode = QStringLiteral("normal");
        for (int i = m_entries.size() - 1; i >= 0; --i) {
            if (m_entries[i].key == key && m_entries[i].trigger == entry.trigger) {
                m_entries.removeAt(i);
            }
        }
        m_entries.append(entry);
    }
    m_learnedKeys.removeAll(key);
    m_learnedKeys.append(key);
    emit learnRequested(mappings);
    emit learnedChanged();
    emit controlLearned(key.group, key.item, describe(mappings));
}

QString QmlControllerLearningProxy::writeScript(const QString& path) const {
    QJsonArray entries;
    QSet<quint16> scriptKeys;
    for (const auto& shift : m_shifts) {
        if (shift) {
            scriptKeys.insert(shift->key);
        }
    }
    for (const Entry& entry : m_entries) {
        if (entry.needsScript()) {
            scriptKeys.insert(entry.midi.key);
            if (entry.trigger == QLatin1String("combo")) {
                scriptKeys.insert(entry.combo.key);
            }
        }
    }
    const auto keyText = [](MidiKey key) {
        return QStringLiteral("%1:%2").arg(key.status).arg(key.control);
    };
    for (const Entry& entry : m_entries) {
        if (!scriptKeys.contains(entry.midi.key)) {
            continue;
        }
        QJsonObject object;
        object.insert(QStringLiteral("key"), keyText(entry.midi));
        object.insert(QStringLiteral("status"), entry.midi.status);
        object.insert(QStringLiteral("control"), entry.midi.control);
        object.insert(QStringLiteral("group"), entry.key.group);
        object.insert(QStringLiteral("item"), entry.key.item);
        object.insert(QStringLiteral("button"), entry.button);
        object.insert(QStringLiteral("trigger"), entry.trigger);
        object.insert(QStringLiteral("combo"),
                entry.trigger == QLatin1String("combo") ? keyText(entry.combo) : QString());
        object.insert(QStringLiteral("buttonMode"), entry.buttonMode);
        object.insert(QStringLiteral("knobMode"), entry.knobMode);
        object.insert(QStringLiteral("led"), entry.led);
        entries.append(object);
    }
    QJsonArray shifts;
    for (const auto& shift : m_shifts) {
        shifts.append(shift ? QJsonValue(keyText(*shift)) : QJsonValue());
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QString();
    }
    QByteArray text;
    text += "// Made with Learn controller on Android: the functions learned with\n"
            "// options (SHIFT, long and double presses, combinations, modes, lights)\n\n"
            "// eslint-disable-next-line no-var\n"
            "var LearnedMap = {};\n";
    text += "LearnedMap.entries = " +
            QJsonDocument(entries).toJson(QJsonDocument::Indented).trimmed() + ";\n";
    text += "LearnedMap.shifts = " + QJsonDocument(shifts).toJson(QJsonDocument::Compact) + ";\n";
    text += kScriptBody;
    if (file.write(text) != text.size()) {
        return QString();
    }
    return QFileInfo(path).fileName();
}

QString QmlControllerLearningProxy::save(const QString& name) {
    if (!m_pController || (m_learnedMappings.isEmpty() && m_entries.isEmpty())) {
        return QString();
    }
    // The mapping the controller had, and what was learned over it
    auto pMapping = std::make_shared<LegacyMidiControllerMapping>();
    if (const auto pCurrent = m_pController->getMapping()) {
        if (const auto* pMidi = dynamic_cast<const LegacyMidiControllerMapping*>(pCurrent.get())) {
            pMapping = std::make_shared<LegacyMidiControllerMapping>(*pMidi);
        }
    }
    for (const MidiInputMapping& mapping : std::as_const(m_learnedMappings)) {
        pMapping->removeInputMapping(mapping.key.key);
    }
    for (const MidiInputMapping& mapping : std::as_const(m_learnedMappings)) {
        pMapping->addInputMapping(mapping.key.key, mapping);
    }
    // (The script, below, takes over the controls it plays)
    const QString cleanName = name.trimmed().isEmpty() ? m_pController->getName() : name.trimmed();
    const QString directory = QStringLiteral("/storage/emulated/0/Mixxx/controllers/");
    QDir().mkpath(directory);
    QString fileName = cleanName;
    fileName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9 _.-]")), QStringLiteral("_"));

    // The controls of the functions learned with options, and those sharing
    // them, go through the script
    QSet<quint16> scriptKeys;
    for (const auto& shift : m_shifts) {
        if (shift) {
            scriptKeys.insert(shift->key);
        }
    }
    for (const Entry& entry : std::as_const(m_entries)) {
        if (entry.needsScript()) {
            scriptKeys.insert(entry.midi.key);
            if (entry.trigger == QLatin1String("combo")) {
                scriptKeys.insert(entry.combo.key);
            }
        }
    }
    if (!scriptKeys.isEmpty()) {
        const QString scriptPath = directory + fileName + QStringLiteral("-scripts.js");
        const QString scriptName = writeScript(scriptPath);
        if (scriptName.isEmpty()) {
            return QString();
        }
        pMapping->addScriptFile(LegacyControllerMapping::ScriptFileInfo{scriptName,
                kScriptPrefix,
                QFileInfo(scriptPath),
                LegacyControllerMapping::ScriptFileInfo::Type::Javascript,
                false});
        for (const quint16 rawKey : std::as_const(scriptKeys)) {
            MidiKey key;
            key.key = rawKey;
            QList<MidiKey> keys{key};
            if (isNote(key)) {
                keys.append(MidiKey(static_cast<unsigned char>(0x80 | (key.status & 0x0F)),
                        key.control));
            }
            for (const MidiKey& variant : keys) {
                pMapping->removeInputMapping(variant.key);
                pMapping->addInputMapping(variant.key,
                        MidiInputMapping(variant,
                                MidiOptions(MidiOption::Script),
                                ConfigKey(QStringLiteral("[Master]"),
                                        kScriptPrefix + QStringLiteral(".input"))));
            }
        }
    }
    pMapping->setName(cleanName);
    pMapping->setAuthor(QStringLiteral("Mixxx Android (learned)"));
    pMapping->setDescription(QStringLiteral("Made with Learn controller on Android for %1")
                                     .arg(m_pController->getName()));

    const QString path = directory + fileName + QStringLiteral(".midi.xml");
    pMapping->setFilePath(path);
    if (!pMapping->saveMapping(path)) {
        return QString();
    }

    // Used from now on (and remembered for this controller)
    if (const auto pManager = QmlControllerManagerProxy::manager()) {
        MidiController* pController = m_pController;
        QMetaObject::invokeMethod(
                pManager.get(),
                [pManager, pController, pMapping]() {
                    pManager->slotApplyMapping(pController, pMapping, true);
                },
                Qt::QueuedConnection);
    }
    m_learnedMappings.clear();
    m_learnedKeys.clear();
    m_entries.clear();
    m_shifts = {};
    emit learnedChanged();
    emit shiftsChanged();
    disconnectController();
    emit activeChanged();
    return path;
}

void QmlControllerLearningProxy::cancel() {
    stopListening();
    m_learnedMappings.clear();
    m_learnedKeys.clear();
    m_entries.clear();
    m_shifts = {};
    emit shiftsChanged();
    emit learnedChanged();
    const bool wasActive = active();
    disconnectController();
    if (wasActive) {
        emit activeChanged();
    }
}

} // namespace qml
} // namespace mixxx
