#pragma once

#include <QObject>
#include <QPair>
#include <QPointer>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <array>
#include <optional>

#include "controllers/midi/midimessage.h"

class MidiController;

namespace mixxx {
namespace qml {

/// Makes a mapping by learning (Android), as the `Mixxx.ControllerLearning`
/// singleton: choose a function of Mixxx, move or press the control on the
/// controller, and the MIDI messages it sends are turned into a mapping
/// (LearningUtils guesses buttons, knobs, faders and encoders). The learned
/// controls work at once; save() writes them, with the mapping the
/// controller had, into a mapping file of its own.
///
/// A function can also be learned with options (see listenWith()): on a
/// SHIFT layer, on a long or a double press, on a combination of two
/// buttons, as a toggle, a knob inverted, relative or with soft takeover,
/// or with the light of its button. Those are played by a script that save()
/// writes next to the mapping (they work once saved).
class QmlControllerLearningProxy : public QObject {
    Q_OBJECT
    /// The MIDI controllers that are open
    Q_PROPERTY(QStringList controllers READ controllers NOTIFY controllersChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    /// The last message received, e.g. "0x90 0x0B 0x7F"
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    /// The functions learned in this session, as "group,key"
    Q_PROPERTY(QStringList learned READ learned NOTIFY learnedChanged)
    /// The buttons learned as SHIFT 1 and 2, described ("" when none)
    Q_PROPERTY(QStringList shifts READ shifts NOTIFY shiftsChanged)
    QML_NAMED_ELEMENT(ControllerLearning)
    QML_SINGLETON

  public:
    explicit QmlControllerLearningProxy(QObject* pParent = nullptr);
    ~QmlControllerLearningProxy() override;

    static QmlControllerLearningProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QStringList controllers() const;
    bool active() const {
        return !m_pController.isNull();
    }
    bool listening() const {
        return m_listening;
    }
    QString lastMessage() const {
        return m_lastMessage;
    }
    QStringList learned() const;
    QStringList shifts() const;

    /// Starts learning on a controller of `controllers`
    Q_INVOKABLE bool start(int index);
    /// Listens for the control of a function of Mixxx, e.g. "[Channel1]",
    /// "play"
    Q_INVOKABLE void listen(const QString& group, const QString& key);
    /// Listens with options: "trigger" (press, shift1, shift2, long, double,
    /// combo: hold a button, then press the other one), "buttonMode" (auto,
    /// momentary, toggle), "knobMode" (normal, invert, relative, soft) and
    /// "led" (true: the light of the button shows the function)
    Q_INVOKABLE void listenWith(const QString& group, const QString& key, const QVariantMap& options);
    /// Listens for the button to use as SHIFT 1 (0) or SHIFT 2 (1)
    Q_INVOKABLE void listenShift(int which);
    /// Forgets the SHIFT button 0 or 1
    Q_INVOKABLE void clearShift(int which);
    /// Stops listening without learning
    Q_INVOKABLE void stopListening();
    /// Saves the mapping as Mixxx/controllers/<name>.midi.xml and uses it.
    /// Returns the file, or an empty string on error.
    Q_INVOKABLE QString save(const QString& name);
    /// Stops learning, forgetting what wasn't saved
    Q_INVOKABLE void cancel();

  signals:
    void controllersChanged();
    void activeChanged();
    void listeningChanged();
    void lastMessageChanged();
    void learnedChanged();
    void shiftsChanged();
    /// A function was learned (description: e.g. "button, note 0x0B")
    void controlLearned(const QString& group, const QString& key, const QString& description);
    /// Nothing usable was received for a function
    void controlNotLearned(const QString& group, const QString& key);

    // To the controller thread
    void startLearningRequested();
    void stopLearningRequested();
    void learnRequested(const MidiInputMappings& mappings);
    void clearRequested();

  private slots:
    void slotMessageReceived(unsigned char status, unsigned char control, unsigned char value);
    void slotSettled();

  private:
    /// A function as the script plays it
    struct Entry {
        ConfigKey key;
        // The note on (also for its note off) or control change
        MidiKey midi;
        bool button = true;
        QString trigger;
        MidiKey combo;
        QString buttonMode;
        QString knobMode;
        bool led = false;

        bool needsScript() const {
            return trigger != QLatin1String("press") || buttonMode == QLatin1String("toggle") ||
                    buttonMode == QLatin1String("momentary") ||
                    knobMode != QLatin1String("normal") || led;
        }
    };

    void connectController(MidiController* pController);
    void disconnectController();
    /// Writes the script of the entries and SHIFT buttons next to the
    /// mapping; returns its file name, or an empty string on error
    QString writeScript(const QString& path) const;

    QPointer<MidiController> m_pController;
    bool m_listening;
    ConfigKey m_listenKey;
    QList<QPair<MidiKey, unsigned char>> m_messages;
    QTimer m_settleTimer;
    QString m_lastMessage;
    /// What was learned in this session, for save()
    MidiInputMappings m_learnedMappings;
    QList<ConfigKey> m_learnedKeys;
    // Learning with options, and what was learned so
    QVariantMap m_listenOptions;
    int m_listenShift;
    QList<Entry> m_entries;
    std::array<std::optional<MidiKey>, 2> m_shifts;
};

} // namespace qml
} // namespace mixxx
