#include "exportUiController.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QStringList>
#include <QSet>
#include <QUuid>

#include "../systemController.h"
#include "core/utils/qrCodeUtils.h"

ExportUiController::ExportUiController(ExportController* exportController, SecureQSettings* settings, QObject *parent)
    : QObject(parent),
      m_exportController(exportController),
      m_settings(settings)
{
    addSystemShareTemplates();
    if (m_settings) {
        const auto groups = QJsonDocument::fromJson(m_settings->value("Sharing/accountGroups").toByteArray()).array();
        const auto templates = QJsonDocument::fromJson(m_settings->value("Sharing/templates").toByteArray()).array();
        for (const auto &v : groups) m_accountGroups.append(v.toObject().toVariantMap());
        for (const auto &v : templates) {
            QVariantMap item = v.toObject().toVariantMap();
            item.insert("isSystem", false);
            if (item.value("protocol").toString().isEmpty()) item.insert("protocol", QStringLiteral("general"));
            m_shareTemplates.append(item);
        }
        m_templateDefaults = QJsonDocument::fromJson(
            m_settings->value("Sharing/templateDefaults").toByteArray()).object().toVariantMap();
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
    for (const auto &item : m_shareTemplates) {
        const QVariantMap value = item.toMap();
        if (!value.value("isSystem").toBool()) array.append(QJsonObject::fromVariantMap(value));
    }
    if (m_settings) m_settings->setValue("Sharing/templates", QJsonDocument(array).toJson(QJsonDocument::Compact));
    emit shareTemplatesChanged();
}

void ExportUiController::saveTemplateDefaults()
{
    if (m_settings) {
        m_settings->setValue("Sharing/templateDefaults",
                             QJsonDocument(QJsonObject::fromVariantMap(m_templateDefaults)).toJson(QJsonDocument::Compact));
    }
    emit shareTemplatesChanged();
}

void ExportUiController::addSystemShareTemplates()
{
    const QString common = QStringLiteral(
        "سلام {{NAME}}،\nسرور: {{SERVER}}\nروش اتصال: {{PROTOCOLS}}\n{{CONFIGS}}\n{{QR}}\n"
        "برای اتصال، کانفیگ را در برنامهٔ سازگار وارد کنید.");
    const auto add = [this](const QString &id, const QString &name, const QString &protocol, const QString &body) {
        m_shareTemplates.append(QVariantMap{{"id", id}, {"name", name}, {"protocol", protocol},
                                            {"body", body}, {"isSystem", true}});
    };
    add(QStringLiteral("system-general"), tr("General"), QStringLiteral("general"), common);
    add(QStringLiteral("system-openvpn"), QStringLiteral("OpenVPN"), QStringLiteral("openvpn"), common);
    add(QStringLiteral("system-wireguard"), QStringLiteral("WireGuard"), QStringLiteral("wireguard"), common);
    add(QStringLiteral("system-awg"), QStringLiteral("AWG"), QStringLiteral("awg"), common);
    add(QStringLiteral("system-xray"), QStringLiteral("XRay"), QStringLiteral("xray"), common);
    add(QStringLiteral("system-ikev2"), QStringLiteral("IKEv2"), QStringLiteral("ikev2"), common);
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

    const QString suffix = m_batchCount == 1 ? QString() : QString(" %1").arg(m_batchIndex + 1);
    const int baseNameLength = qMax(0, 20 - suffix.size());
    const QString name = m_batchBaseName.left(baseNameLength).trimmed() + suffix;
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
    upsertShareTemplate({}, name, body, QStringLiteral("general"));
}

void ExportUiController::upsertShareTemplate(const QString &id, const QString &name,
                                               const QString &body, const QString &protocol)
{
    if (name.trimmed().isEmpty() || body.trimmed().isEmpty()) return;
    const QString normalizedProtocol = protocol.isEmpty() ? QStringLiteral("general") : protocol;
    if (!id.isEmpty()) {
        for (qsizetype i = 0; i < m_shareTemplates.size(); ++i) {
            QVariantMap item = m_shareTemplates.at(i).toMap();
            if (item.value("id").toString() == id && !item.value("isSystem").toBool()) {
                item.insert("name", name.trimmed());
                item.insert("body", body);
                item.insert("protocol", normalizedProtocol);
                m_shareTemplates[i] = item;
                saveShareTemplates();
                return;
            }
        }
    }
    QVariantMap item{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                     {"name", name.trimmed()}, {"body", body}, {"protocol", normalizedProtocol},
                     {"isSystem", false}};
    m_shareTemplates.append(item);
    saveShareTemplates();
}

void ExportUiController::deleteShareTemplate(const QString &id)
{
    for (qsizetype i = m_shareTemplates.size() - 1; i >= 0; --i) {
        const QVariantMap item = m_shareTemplates.at(i).toMap();
        if (item.value("id").toString() == id && !item.value("isSystem").toBool()) m_shareTemplates.removeAt(i);
    }
    for (auto it = m_templateDefaults.begin(); it != m_templateDefaults.end();) {
        if (it.value().toString() == id) it = m_templateDefaults.erase(it); else ++it;
    }
    saveShareTemplates();
    saveTemplateDefaults();
}

void ExportUiController::setDefaultShareTemplate(const QString &protocol, const QString &templateId)
{
    if (protocol.isEmpty() || shareTemplate(templateId).isEmpty()) return;
    m_templateDefaults.insert(protocol, templateId);
    saveTemplateDefaults();
}

QString ExportUiController::defaultShareTemplateId(const QString &protocol) const
{
    const QString configured = m_templateDefaults.value(protocol).toString();
    if (!configured.isEmpty() && !shareTemplate(configured).isEmpty()) return configured;
    const QString systemId = QStringLiteral("system-") + protocol;
    if (!shareTemplate(systemId).isEmpty()) return systemId;
    return QStringLiteral("system-general");
}

QString ExportUiController::shareTemplateName(const QString &id) const
{
    return shareTemplate(id).value("name").toString();
}

QVariantMap ExportUiController::shareTemplate(const QString &id) const
{
    for (const auto &value : m_shareTemplates) {
        const QVariantMap item = value.toMap();
        if (item.value("id").toString() == id) return item;
    }
    return {};
}

QVariantList ExportUiController::templatesForProtocol(const QString &protocol) const
{
    QVariantList result;
    for (const auto &value : m_shareTemplates) {
        const QVariantMap item = value.toMap();
        const QString itemProtocol = item.value("protocol").toString();
        if (itemProtocol == protocol || itemProtocol == QStringLiteral("general")) result.append(item);
    }
    return result;
}

QVariantList ExportUiController::shareProtocols(const QVariantList &groupIds) const
{
    QVariantList result;
    QSet<QString> seen;
    for (const auto &groupId : groupIds) {
        const QVariantMap group = accountGroup(groupId.toString());
        for (const auto &entry : group.value("methods").toList()) {
            const QVariantMap method = entry.toMap();
            const QString key = protocolKeyForMethod(method);
            if (seen.contains(key)) continue;
            seen.insert(key);
            result.append(QVariantMap{{"key", key}, {"name", protocolDisplayName(key)},
                                      {"defaultTemplateId", defaultShareTemplateId(key)}});
        }
    }
    return result;
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
    return wrapAccountsHtml(renderAccountTemplateFragment(group, templateBody));
}

QString ExportUiController::renderAccountsTemplate(const QVariantList &groupIds, const QString &templateBody)
{
    QString sections;
    for (const auto &value : groupIds) {
        const auto group = accountGroup(value.toString());
        if (!group.isEmpty()) sections += renderAccountTemplateFragment(group, templateBody);
    }
    if (sections.isEmpty()) return {};
    return wrapAccountsHtml(sections);
}

QString ExportUiController::renderAccountsText(const QVariantList &groupIds, const QString &templateBody)
{
    QStringList sections;
    for (const auto &value : groupIds) {
        const QVariantMap group = accountGroup(value.toString());
        if (!group.isEmpty()) sections.append(renderAccountTextFragment(group, templateBody));
    }
    return sections.join(QStringLiteral("\n\n----------------------------------------\n\n"));
}

QString ExportUiController::renderAccountsWithTemplates(const QVariantList &groupIds,
                                                         const QVariantMap &templateIdsByProtocol,
                                                         bool html) const
{
    QStringList sections;
    for (const auto &groupId : groupIds) {
        const QVariantMap group = accountGroup(groupId.toString());
        if (group.isEmpty()) continue;
        for (const auto &entry : group.value("methods").toList()) {
            const QVariantMap method = entry.toMap();
            const QString protocol = protocolKeyForMethod(method);
            QString templateId = templateIdsByProtocol.value(protocol).toString();
            if (templateId.isEmpty()) templateId = defaultShareTemplateId(protocol);
            QString body = templateBodyById(templateId);
            if (body.isEmpty()) body = templateBodyById(QStringLiteral("system-general"));
            QVariantMap singleMethodGroup = group;
            singleMethodGroup.insert("methods", QVariantList{method});
            sections.append(html ? renderAccountTemplateFragment(singleMethodGroup, body)
                                 : renderAccountTextFragment(singleMethodGroup, body));
        }
    }
    if (sections.isEmpty()) return {};
    return html ? wrapAccountsHtml(sections.join(QString()))
                : sections.join(QStringLiteral("\n\n----------------------------------------\n\n"));
}

QString ExportUiController::protocolKeyForMethod(const QVariantMap &method) const
{
    const auto container = static_cast<amnezia::DockerContainer>(method.value("container").toInt());
    switch (container) {
    case amnezia::DockerContainer::OpenVpn:
    case amnezia::DockerContainer::ShadowSocks:
    case amnezia::DockerContainer::Cloak: return QStringLiteral("openvpn");
    case amnezia::DockerContainer::WireGuard: return QStringLiteral("wireguard");
    case amnezia::DockerContainer::Awg:
    case amnezia::DockerContainer::Awg2: return QStringLiteral("awg");
    case amnezia::DockerContainer::Xray:
    case amnezia::DockerContainer::SSXray: return QStringLiteral("xray");
    case amnezia::DockerContainer::Ipsec: return QStringLiteral("ikev2");
    default: return QStringLiteral("general");
    }
}

QString ExportUiController::protocolDisplayName(const QString &protocol) const
{
    if (protocol == QStringLiteral("openvpn")) return QStringLiteral("OpenVPN");
    if (protocol == QStringLiteral("wireguard")) return QStringLiteral("WireGuard");
    if (protocol == QStringLiteral("awg")) return QStringLiteral("AWG");
    if (protocol == QStringLiteral("xray")) return QStringLiteral("XRay");
    if (protocol == QStringLiteral("ikev2")) return QStringLiteral("IKEv2");
    return tr("General");
}

QString ExportUiController::templateBodyById(const QString &id) const
{
    return shareTemplate(id).value("body").toString();
}

QString ExportUiController::wrapAccountsHtml(const QString &sections) const
{
    return QStringLiteral(
        "<!doctype html><html lang=\"fa\" dir=\"rtl\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>CoCo VPN · اطلاعات اتصال</title>"
        "<style>"
        ":root{color-scheme:light;--ink:#18221c;--muted:#64736a;--green:#176b36;--line:#dce8df;--paper:#fff;--wash:#f1f7f2}"
        "*{box-sizing:border-box}body{margin:0;background:#edf3ee;color:var(--ink);font:16px/1.85 Tahoma,Arial,sans-serif}"
        ".page{max-width:900px;margin:32px auto;padding:0 18px}.brand{display:flex;align-items:center;gap:12px;margin:0 0 20px;color:var(--green);font-size:14px;font-weight:700}"
        ".brand-mark{width:12px;height:12px;border-radius:50%;background:var(--green);box-shadow:0 0 0 5px #d8ecdd}"
        ".account{overflow:hidden;margin:0 0 24px;background:var(--paper);border:1px solid var(--line);border-radius:20px;box-shadow:0 8px 28px #173c2210}"
        ".account-head{padding:24px 26px;background:linear-gradient(135deg,#f0f8f1,#fff);border-bottom:1px solid var(--line)}"
        "h1{margin:0;color:var(--green);font-size:24px;line-height:1.4} .server{margin:6px 0 0;color:var(--muted)}"
        ".account-body{padding:22px 26px}.message{margin-bottom:18px}h2,h3{color:var(--green)}h3{margin:18px 0 8px;font-size:17px}"
        "pre{margin:0;padding:16px;overflow-wrap:anywhere;white-space:pre-wrap;direction:ltr;text-align:left;background:#f5f8f5;border:1px solid var(--line);border-radius:12px;font:13px/1.65 Consolas,\"Courier New\",monospace}"
        ".protocols{display:flex;flex-wrap:wrap;gap:8px;margin:14px 0}.protocol{padding:3px 11px;border-radius:999px;background:#e8f4ea;color:var(--green);font-size:13px;font-weight:700}"
        ".qr-list{display:flex;flex-wrap:wrap;gap:14px;margin-top:12px}.qr{margin:0;padding:12px;text-align:center;border:1px solid var(--line);border-radius:14px;background:#fff}.qr img{display:block;width:190px;max-width:100%;height:auto;margin:auto}.qr figcaption{margin-top:6px;color:var(--muted);font-size:13px}"
        ".footer{padding:14px;color:var(--muted);text-align:center;font-size:12px}@media(max-width:600px){.page{margin:14px auto;padding:0 10px}.account-head,.account-body{padding:18px}.qr img{width:160px}}"
        "@media print{body{background:#fff}.page{max-width:none;margin:0;padding:0}.account{break-inside:avoid;box-shadow:none}.footer{color:#555}}"
        "</style></head><body><main class=\"page\"><div class=\"brand\"><span class=\"brand-mark\"></span><span>CoCo VPN · راهنمای اتصال</span></div>%1"
        "<footer class=\"footer\">اطلاعات اتصال محرمانه است؛ این فایل را فقط با کاربر موردنظر به‌اشتراک بگذارید.</footer></main></body></html>")
        .arg(sections);
}

QString ExportUiController::renderAccountTemplateFragment(const QVariantMap &group, const QString &templateBody) const
{
    QStringList protocolBadges;
    QString configs;
    QString qrs;
    for (const auto &entry : group.value("methods").toList()) {
        const QVariantMap method = entry.toMap();
        const QString methodName = method.value("name").toString();
        protocolBadges.append(QString("<span class=\"protocol\">%1</span>").arg(methodName.toHtmlEscaped()));
        configs += QString("<section><h3>%1</h3><pre>%2</pre></section>")
                       .arg(methodName.toHtmlEscaped(), method.value("config").toString().toHtmlEscaped());
        for (const auto &qrValue : method.value("qrCodes").toList()) {
            QString qr = qrValue.toString();
            qr.replace(QStringLiteral("data:image/svg;base64,"), QStringLiteral("data:image/svg+xml;base64,"));
            if (qr.startsWith("data:image/"))
                qrs += QString("<figure class=\"qr\"><img alt=\"%1 QR\" src=\"%2\"><figcaption>کد QR · %1</figcaption></figure>")
                           .arg(methodName.toHtmlEscaped(), qr.toHtmlEscaped());
        }
    }
    QString output = templateBody.toHtmlEscaped().replace("\n", "<br/>");
    output.replace("{{NAME}}", group.value("name").toString().toHtmlEscaped());
    output.replace("{{SERVER}}", group.value("serverName").toString().toHtmlEscaped());
    output.replace("{{PROTOCOLS}}", QString("<div class=\"protocols\">%1</div>").arg(protocolBadges.join(QString())));
    output.replace("{{CONFIGS}}", configs);
    output.replace("{{QR}}", qrs.isEmpty() ? "(برای این تنظیمات کد QR در دسترس نیست.)" : QString("<div class=\"qr-list\">%1</div>").arg(qrs));
    return QString("<article class=\"account\"><div class=\"account-body\"><div class=\"message\">%1</div></div></article>")
        .arg(output);
}

QString ExportUiController::renderAccountTextFragment(const QVariantMap &group, const QString &templateBody) const
{
    QStringList protocols;
    QStringList configs;
    for (const auto &entry : group.value("methods").toList()) {
        const QVariantMap method = entry.toMap();
        const QString methodName = method.value("name").toString();
        protocols.append(methodName);
        configs.append(QString("[%1]\n%2").arg(methodName, method.value("config").toString()));
    }

    QString output = templateBody;
    output.replace("{{NAME}}", group.value("name").toString());
    output.replace("{{SERVER}}", group.value("serverName").toString());
    output.replace("{{PROTOCOLS}}", protocols.join(QStringLiteral(", ")));
    output.replace("{{CONFIGS}}", configs.join(QStringLiteral("\n\n")));
    output.replace("{{QR}}", protocols.isEmpty()
                                   ? QStringLiteral("کد QR برای این حساب موجود نیست.")
                                   : QStringLiteral("برای مشاهدهٔ QR کدها، خروجی HTML را ذخیره کنید."));
    return output;
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

bool ExportUiController::setConfigFromString(const QString &config, const QString &fileName)
{
    clearPreviousConfig();
    m_config = config;
    emit exportConfigChanged();
    if (fileName.isEmpty()) {
        return false;
    }
    if (!SystemController::saveFile(fileName, m_config)) {
        emit exportErrorOccurred(ErrorCode::InternalError);
        return false;
    }
    return true;
}
