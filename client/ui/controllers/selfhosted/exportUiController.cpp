#include "exportUiController.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>

#include "../systemController.h"
#include "core/utils/qrCodeUtils.h"

ExportUiController::ExportUiController(ExportController* exportController, SecureQSettings* settings, QObject *parent)
    : QObject(parent),
      m_exportController(exportController),
      m_settings(settings)
{
    if (m_settings) {
        const auto groups = QJsonDocument::fromJson(m_settings->value("Sharing/accountGroups").toByteArray()).array();
        const auto templates = QJsonDocument::fromJson(m_settings->value("Sharing/templates").toByteArray()).array();
        for (const auto &v : groups) m_accountGroups.append(v.toObject().toVariantMap());
        for (const auto &v : templates) m_shareTemplates.append(v.toObject().toVariantMap());
    }
    connect(m_exportController, &ExportController::revokeFinished, this, [this](ErrorCode errorCode) {
        if (errorCode == ErrorCode::NoError) {
            emit revokeConfigFinished();
        } else {
            emit exportErrorOccurred(errorCode);
        }
    });
}

QVariantList ExportUiController::accountGroups() const { return m_accountGroups; }
QVariantList ExportUiController::shareTemplates() const { return m_shareTemplates; }
int ExportUiController::batchProgress() const { return m_batchIndex * m_batchContainers.size() + m_batchProtocolIndex; }
int ExportUiController::batchTotal() const { return m_batchCount * m_batchContainers.size(); }
bool ExportUiController::batchRunning() const { return m_batchRunning; }

void ExportUiController::saveAccountGroups()
{
    QJsonArray array;
    for (const auto &group : m_accountGroups) array.append(QJsonObject::fromVariantMap(group.toMap()));
    if (m_settings) m_settings->setValue("Sharing/accountGroups", QJsonDocument(array).toJson(QJsonDocument::Compact));
    emit accountGroupsChanged();
}

void ExportUiController::saveShareTemplates()
{
    QJsonArray array;
    for (const auto &item : m_shareTemplates) array.append(QJsonObject::fromVariantMap(item.toMap()));
    if (m_settings) m_settings->setValue("Sharing/templates", QJsonDocument(array).toJson(QJsonDocument::Compact));
    emit shareTemplatesChanged();
}

void ExportUiController::startAccountBatch(const QString &serverId, const QString &serverName,
                                            const QString &baseName, int count, const QVariantList &containers)
{
    if (m_batchRunning || serverId.isEmpty() || baseName.trimmed().isEmpty() || containers.isEmpty() || count < 1 || count > 100)
        return;
    m_batchServerId = serverId;
    m_batchServerName = serverName;
    m_batchBaseName = baseName.trimmed();
    m_batchCount = count;
    m_batchIndex = 0;
    m_batchProtocolIndex = 0;
    m_batchSucceeded = 0;
    m_batchFailed = 0;
    m_batchContainers = containers;
    m_batchRunning = true;
    emit batchProgressChanged();
    QTimer::singleShot(50, this, &ExportUiController::createNextBatchAccount);
}

void ExportUiController::createNextBatchAccount()
{
    if (!m_batchRunning) return;
    if (m_batchIndex >= m_batchCount) {
        m_batchRunning = false;
        emit batchProgressChanged();
        emit accountBatchFinished(m_batchSucceeded, m_batchFailed);
        return;
    }

    const QString name = m_batchCount == 1 ? m_batchBaseName : QString("%1 %2").arg(m_batchBaseName).arg(m_batchIndex + 1);
    if (m_batchProtocolIndex == 0) {
        m_batchGroup = {{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"name", name},
                        {"serverId", m_batchServerId}, {"serverName", m_batchServerName},
                        {"createdAt", QDateTime::currentDateTime().toString(Qt::ISODate)}, {"status", QStringLiteral("inProgress")},
                        {"methods", QVariantList{}}, {"errors", QStringList{}}};
    }

    const int containerIndex = m_batchContainers.at(m_batchProtocolIndex).toInt();
    const auto container = static_cast<amnezia::DockerContainer>(containerIndex);
    ExportController::ExportResult result;
    if (container == amnezia::DockerContainer::OpenVpn)
        result = m_exportController->generateOpenVpnConfig(m_batchServerId, name);
    else if (container == amnezia::DockerContainer::WireGuard)
        result = m_exportController->generateWireGuardConfig(m_batchServerId, name);
    else if (container == amnezia::DockerContainer::Awg || container == amnezia::DockerContainer::Awg2)
        result = m_exportController->generateAwgConfig(m_batchServerId, containerIndex, name);
    else if (container == amnezia::DockerContainer::Xray)
        result = m_exportController->generateXrayConfig(m_batchServerId, name);
    else
        result = m_exportController->generateConnectionConfig(m_batchServerId, containerIndex, name);

    if (result.errorCode == ErrorCode::NoError && !result.config.isEmpty()) {
        QVariantList methods = m_batchGroup.value("methods").toList();
        QVariantList qrs;
        for (const auto &qr : result.qrCodes) qrs.append(qr);
        methods.append(QVariantMap{{"container", containerIndex}, {"name", amnezia::ContainerUtils::containerHumanNames().value(container)},
                                   {"clientId", result.clientId}, {"config", result.config}, {"qrCodes", qrs}});
        m_batchGroup.insert("methods", methods);
    } else {
        QStringList errors = m_batchGroup.value("errors").toStringList();
        errors.append(QString("%1: %2").arg(amnezia::ContainerUtils::containerHumanNames().value(container)).arg(static_cast<int>(result.errorCode)));
        m_batchGroup.insert("errors", errors);
    }

    ++m_batchProtocolIndex;
    emit batchProgressChanged();
    persistBatchGroup();
    if (m_batchProtocolIndex >= m_batchContainers.size()) {
        if (!m_batchGroup.value("methods").toList().isEmpty()) {
            m_batchGroup.insert("status", m_batchGroup.value("errors").toStringList().isEmpty()
                                ? QStringLiteral("complete") : QStringLiteral("partial"));
            persistBatchGroup();
            ++m_batchSucceeded;
        } else {
            ++m_batchFailed;
        }
        m_batchProtocolIndex = 0;
        ++m_batchIndex;
        emit batchProgressChanged();
    }
    QTimer::singleShot(50, this, &ExportUiController::createNextBatchAccount);
}

void ExportUiController::persistBatchGroup()
{
    if (m_batchGroup.value("methods").toList().isEmpty()) return;
    const QString id = m_batchGroup.value("id").toString();
    bool updated = false;
    for (qsizetype i = 0; i < m_accountGroups.size(); ++i) {
        if (m_accountGroups.at(i).toMap().value("id").toString() == id) {
            m_accountGroups[i] = m_batchGroup;
            updated = true;
            break;
        }
    }
    if (!updated) m_accountGroups.prepend(m_batchGroup);
    saveAccountGroups();
}

void ExportUiController::saveShareTemplate(const QString &name, const QString &body)
{
    if (name.trimmed().isEmpty() || body.trimmed().isEmpty()) return;
    QVariantMap item{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"name", name.trimmed()}, {"body", body}};
    m_shareTemplates.prepend(item);
    saveShareTemplates();
}

void ExportUiController::deleteShareTemplate(const QString &id)
{
    for (qsizetype i = m_shareTemplates.size() - 1; i >= 0; --i)
        if (m_shareTemplates.at(i).toMap().value("id").toString() == id) m_shareTemplates.removeAt(i);
    saveShareTemplates();
}

void ExportUiController::deleteAccountGroup(const QString &id)
{
    for (qsizetype i = m_accountGroups.size() - 1; i >= 0; --i)
        if (m_accountGroups.at(i).toMap().value("id").toString() == id) m_accountGroups.removeAt(i);
    saveAccountGroups();
}

QVariantMap ExportUiController::accountGroup(const QString &id) const
{
    for (const auto &item : m_accountGroups)
        if (item.toMap().value("id").toString() == id) return item.toMap();
    return {};
}

QString ExportUiController::renderAccountTemplate(const QString &groupId, const QString &templateBody)
{
    const QVariantMap group = accountGroup(groupId);
    if (group.isEmpty()) return {};
    return QString("<!doctype html><html><head><meta charset=\"utf-8\"><style>body{font-family:Shabnam,sans-serif;direction:rtl;text-align:right;color:#202124;background:#fff;margin:24px}pre{white-space:pre-wrap;overflow-wrap:anywhere}img{max-width:100%;height:auto}</style></head><body>%1</body></html>")
            .arg(renderAccountTemplateFragment(group, templateBody));
}

QString ExportUiController::renderAccountsTemplate(const QVariantList &groupIds, const QString &templateBody)
{
    QString sections;
    for (const auto &value : groupIds) {
        const auto group = accountGroup(value.toString());
        if (!group.isEmpty()) sections += renderAccountTemplateFragment(group, templateBody);
    }
    if (sections.isEmpty()) return {};
    return QString("<!doctype html><html><head><meta charset=\"utf-8\"><style>body{font-family:Shabnam,sans-serif;direction:rtl;text-align:right;color:#202124;background:#fff;margin:24px}pre{white-space:pre-wrap;overflow-wrap:anywhere}img{max-width:100%;height:auto}article{margin-bottom:32px}</style></head><body>%1</body></html>")
            .arg(sections);
}

QString ExportUiController::renderAccountTemplateFragment(const QVariantMap &group, const QString &templateBody) const
{
    QString protocols;
    QString configs;
    QString qrs;
    for (const auto &entry : group.value("methods").toList()) {
        const QVariantMap method = entry.toMap();
        if (!protocols.isEmpty()) protocols += ", ";
        const QString methodName = method.value("name").toString();
        protocols += methodName.toHtmlEscaped();
        configs += QString("<h3>%1</h3><pre>%2</pre>").arg(methodName.toHtmlEscaped(), method.value("config").toString().toHtmlEscaped());
        for (const auto &qrValue : method.value("qrCodes").toList()) {
            const QString qr = qrValue.toString();
            if (qr.startsWith("data:image/"))
                qrs += QString("<img alt=\"%1 QR\" src=\"%2\"/>").arg(methodName.toHtmlEscaped(), qr.toHtmlEscaped());
        }
    }
    QString output = templateBody.toHtmlEscaped().replace("\n", "<br/>");
    output.replace("{{NAME}}", group.value("name").toString().toHtmlEscaped());
    output.replace("{{SERVER}}", group.value("serverName").toString().toHtmlEscaped());
    output.replace("{{PROTOCOLS}}", protocols);
    output.replace("{{CONFIGS}}", configs);
    output.replace("{{QR}}", qrs.isEmpty() ? "(برای این تنظیمات کد QR در دسترس نیست.)" : qrs);
    return QString("<article>%1</article>").arg(output);
}

void ExportUiController::generateFullAccessConfig(const QString &serverId)
{
    clearPreviousConfig();
    auto result = m_exportController->generateFullAccessConfig(serverId);
    applyExportResult(result);
}

void ExportUiController::generateConnectionConfig(const QString &serverId, int containerIndex, const QString &clientName)
{
    clearPreviousConfig();
    auto result = m_exportController->generateConnectionConfig(serverId, containerIndex, clientName);
    applyExportResult(result);
}

void ExportUiController::generateOpenVpnConfig(const QString &serverId, const QString &clientName)
{
    clearPreviousConfig();
    auto result = m_exportController->generateOpenVpnConfig(serverId, clientName);
    applyExportResult(result);
}

void ExportUiController::generateWireGuardConfig(const QString &serverId, const QString &clientName)
{
    clearPreviousConfig();
    auto result = m_exportController->generateWireGuardConfig(serverId, clientName);
    applyExportResult(result);
}

void ExportUiController::generateAwgConfig(const QString &serverId, int containerIndex, const QString &clientName)
{
    clearPreviousConfig();
    auto result = m_exportController->generateAwgConfig(serverId, containerIndex, clientName);
    applyExportResult(result);
}


void ExportUiController::generateXrayConfig(const QString &serverId, const QString &clientName)
{
    clearPreviousConfig();
    auto result = m_exportController->generateXrayConfig(serverId, clientName);
    applyExportResult(result);
}

void ExportUiController::generateQrFromString(const QString &text)
{
    clearPreviousConfig();
    m_config = text;
    m_qrCodes = qrCodeUtils::generateQrCodeImageSeries(text.toUtf8());
    emit exportConfigChanged();
}

void ExportUiController::generateQrFromStringRaw(const QString &text)
{
    clearPreviousConfig();
    m_config = text;
    m_qrCodes = { qrCodeUtils::generatePlainQrCodeImage(text.toUtf8()) };
    emit exportConfigChanged();
}

QString ExportUiController::getConfig()
{
    return m_config;
}

QString ExportUiController::getNativeConfigString()
{
    return m_nativeConfigString;
}

QList<QString> ExportUiController::getQrCodes()
{
    return m_qrCodes;
}

void ExportUiController::exportConfig(const QString &fileName)
{
    if (!SystemController::saveFile(fileName, m_config)) {
        qInfo() << "ExportUiController::exportConfig: save or share was cancelled or failed";
    }
}

void ExportUiController::updateClientManagementModel(const QString &serverId, int containerIndex)
{
    m_exportController->updateClientManagementModel(serverId, containerIndex);
}

void ExportUiController::revokeConfig(int row, const QString &serverId, int containerIndex)
{
    m_exportController->revokeConfig(row, serverId, containerIndex);
}

void ExportUiController::renameClient(int row, const QString &clientName, const QString &serverId, int containerIndex)
{
    m_exportController->renameClient(row, clientName, serverId, containerIndex);
}

int ExportUiController::getQrCodesCount()
{
    return m_qrCodes.size();
}

void ExportUiController::clearPreviousConfig()
{
    m_config.clear();
    m_nativeConfigString.clear();
    m_qrCodes.clear();

    emit exportConfigChanged();
}

void ExportUiController::applyExportResult(const ExportController::ExportResult &result)
{
    if (result.errorCode != ErrorCode::NoError) {
        emit exportErrorOccurred(result.errorCode);
        return;
    }

    m_config = result.config;
    m_nativeConfigString = result.nativeConfigString;
    m_qrCodes = result.qrCodes;

    emit exportConfigChanged();
}

void ExportUiController::setConfigFromString(const QString &config, const QString &fileName)
{
    clearPreviousConfig();
    m_config = config;
    emit exportConfigChanged();
    if (!fileName.isEmpty()) {
        SystemController::saveFile(fileName, m_config);
    }
}
