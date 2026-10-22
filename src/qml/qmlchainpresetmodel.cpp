#include "qml/qmlchainpresetmodel.h"

#include <QCoreApplication>
#include <QLocale>
#include <QModelIndex>
#include <QTranslator>

#include "effects/backends/effectmanifest.h"
#include "effects/effectsmanager.h"
#include "effects/presets/effectchainpreset.h"
#include "effects/presets/effectpreset.h"
#include "moc_qmlchainpresetmodel.cpp"
#include "util/translations.h"

namespace mixxx {
namespace qml {
namespace {
// The Android user interface is available in English and Spanish
const QTranslator& spanishTranslator() {
    static QTranslator s_translator;
    static const bool s_loaded = s_translator.load(QLocale(QLocale::Spanish),
            QStringLiteral("mixxx"),
            QStringLiteral("_"),
            mixxx::Translations::translationsDirectory());
    Q_UNUSED(s_loaded);
    return s_translator;
}

const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "display"},
        {Qt::ToolTipRole, "tooltip"},
        {QmlChainPresetModel::NameRole, "name"},
        {QmlChainPresetModel::ReadOnlyRole, "readOnly"},
        {QmlChainPresetModel::PresetDisplayRole, "presetDisplay"},
};
}

QmlChainPresetModel::QmlChainPresetModel(
        std::shared_ptr<EffectsManager> pEffectsManager,
        PresetType presetType,
        QObject* parent)
        : QAbstractListModel(parent),
          m_pEffectsManager(std::move(pEffectsManager)),
          m_pEffectChainPresetManager(m_pEffectsManager->getChainPresetManager()),
          m_presetType(presetType) {
    slotUpdated();
    if (m_presetType == PresetType::Quick) {
        connect(m_pEffectChainPresetManager.get(),
                &EffectChainPresetManager::quickEffectChainPresetListUpdated,
                this,
                &QmlChainPresetModel::slotUpdated);
    } else {
        connect(m_pEffectChainPresetManager.get(),
                &EffectChainPresetManager::effectChainPresetListUpdated,
                this,
                &QmlChainPresetModel::slotUpdated);
    }
}

void QmlChainPresetModel::slotUpdated() {
    beginResetModel();
    m_effectChainPresets = m_presetType == PresetType::Quick
            ? m_pEffectChainPresetManager->getQuickEffectPresetsSorted()
            : m_pEffectChainPresetManager->getPresetsSorted();
    endResetModel();
}

QVariant QmlChainPresetModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_effectChainPresets.size()) {
        return QVariant();
    }

    const EffectChainPresetPointer pPreset = m_effectChainPresets.at(index.row());
    switch (role) {
    case NameRole:
        return pPreset->name();
    case Qt::DisplayRole:
    case PresetDisplayRole:
        return displayName(pPreset);
    case ReadOnlyRole:
        return pPreset->isReadOnly();
    case Qt::ToolTipRole: {
        QStringList effectNames;
        for (const auto& pEffectPreset : pPreset->effectPresets()) {
            if (pEffectPreset->isEmpty()) {
                continue;
            }
            const EffectManifestPointer pManifest =
                    m_pEffectsManager->getBackendManager()->getManifest(pEffectPreset);
            if (pManifest) {
                effectNames.append(pManifest->name());
            }
        }
        QString tooltip = QStringLiteral("<b>%1</b>").arg(displayName(pPreset).toHtmlEscaped());
        if (effectNames.size() > 1) {
            tooltip.append(QStringLiteral("<br/>"));
            tooltip.append(effectNames.join(QStringLiteral("<br/>")));
        }
        return tooltip;
    }
    default:
        return QVariant();
    }
}

QString QmlChainPresetModel::displayName(const EffectChainPresetPointer& pPreset) const {
    const QString name = pPreset->name();
    EffectManifestPointer pManifest;
    for (const auto& pEffectPreset : pPreset->effectPresets()) {
        if (pEffectPreset->isEmpty()) {
            continue;
        }
        if (pManifest) {
            // Several effects: made by the user
            return name;
        }
        pManifest = m_pEffectsManager->getBackendManager()->getManifest(pEffectPreset);
        if (!pManifest) {
            return name;
        }
    }
    if (!pManifest) {
        return name;
    }
    // QuickEffect presets are generated on the first run, named after their
    // effect in the language used then. Show them in the current language.
    const QString effectName = pManifest->displayName();
    if (name == effectName) {
        return name;
    }
    if (QCoreApplication::translate("QObject", name.toUtf8().constData()) == effectName ||
            spanishTranslator().translate("QObject", effectName.toUtf8().constData()) ==
                    name) {
        return effectName;
    }
    return name;
}

int QmlChainPresetModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return m_effectChainPresets.size();
}

QHash<int, QByteArray> QmlChainPresetModel::roleNames() const {
    return kRoleNames;
}

QVariant QmlChainPresetModel::get(int row) const {
    QVariantMap dataMap;
    QModelIndex idx = index(row, 0);

    if (!idx.isValid()) {
        return dataMap;
    }

    for (auto it = kRoleNames.constBegin(); it != kRoleNames.constEnd(); it++) {
        dataMap.insert(it.value(), data(idx, it.key()));
    }
    return dataMap;
}

} // namespace qml
} // namespace mixxx
