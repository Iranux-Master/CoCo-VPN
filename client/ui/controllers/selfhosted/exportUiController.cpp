#include "exportUiController.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QStringList>
#include <QSet>
#include <QUuid>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QMap>

#include "../systemController.h"
#include "core/utils/qrCodeUtils.h"
#include "core/utils/errorStrings.h"

ExportUiController::ExportUiController(ExportController* exportController, SecureQSettings* settings, QObject *parent)
    : QObject(parent),
      m_exportController(exportController),
      m_settings(settings)
{
    addSystemShareTemplates();
    if (m_settings) {
        const auto groups = QJsonDocument::fromJson(m_settings->value("Sharing/accountGroups").toByteArray()).array();
        const auto templates = QJsonDocument::fromJson(m_settings->value("Sharing/templates").toByteArray()).array();
        bool recoveredGroups = false;
        for (const auto &v : groups) {
            QVariantMap group = v.toObject().toVariantMap();
            if (group.value("status").toString() == QStringLiteral("inProgress")) {
                group.insert("status", group.value("methods").toList().isEmpty()
                    ? QStringLiteral("failed") : QStringLiteral("partial"));
                QStringList errors = group.value("errors").toStringList();
                errors.append(tr("Account creation was interrupted. Check the server before trying again."));
                group.insert("errors", errors);
                recoveredGroups = true;
            }
            m_accountGroups.append(group);
        }
        if (recoveredGroups) saveAccountGroups();
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
        "برای اتصال، فایل تنظیمات را دانلود کنید و مراحل راهنمای همین حساب را انجام دهید.");
    const auto add = [this](const QString &id, const QString &name, const QString &protocol, const QString &body) {
        const QString help = QStringLiteral("https://iranux.site/cocoVPN/");
        m_shareTemplates.append(QVariantMap{{"id", id}, {"name", name}, {"protocol", protocol},
                                            {"body", body + QStringLiteral("\nراهنمای برنامه: ") + help}, {"isSystem", true}});
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
    QVariantList validContainers;
    for (const QVariant &value : containers) {
        bool valid = false;
        const int index = value.toInt(&valid);
        const auto container = static_cast<amnezia::DockerContainer>(index);
        if (!valid || !amnezia::ContainerUtils::allContainers().contains(container)
            || !amnezia::ContainerUtils::isShareable(container)
            || amnezia::ContainerUtils::containerService(container) != amnezia::ServiceType::Vpn
            || amnezia::ContainerUtils::isUnsupportedContainer(container)) {
            emit exportErrorOccurred(ErrorCode::InternalError);
            return;
        }
        if (!validContainers.contains(index)) validContainers.append(index);
    }
    m_batchServerId = serverId;
    m_batchServerName = serverName;
    m_batchBaseName = baseName.trimmed();
    m_batchCount = count;
    m_batchIndex = 0;
    m_batchProtocolIndex = 0;
    m_batchSucceeded = 0;
    m_batchFailed = 0;
    m_batchContainers = validContainers;
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
        const QString reason = result.errorCode == ErrorCode::NoError
            ? tr("No connection settings were returned.") : errorString(result.errorCode);
        errors.append(QString("%1: %2").arg(amnezia::ContainerUtils::containerHumanNames().value(container), reason));
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
            m_batchGroup.insert("status", QStringLiteral("failed"));
            persistBatchGroup();
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
    if (m_batchGroup.value("methods").toList().isEmpty()
        && m_batchGroup.value("status").toString() != QStringLiteral("failed")) return;
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
    const QVariantMap existing = shareTemplate(id);
    if (existing.isEmpty() || existing.value("isSystem").toBool()) return;
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
    const QVariantMap item = shareTemplate(templateId);
    const QString itemProtocol = item.value("protocol").toString();
    if (protocol.isEmpty() || item.isEmpty() || (itemProtocol != protocol && itemProtocol != "general")) return;
    m_templateDefaults.insert(protocol, templateId);
    saveTemplateDefaults();
}

QString ExportUiController::defaultShareTemplateId(const QString &protocol) const
{
    const QString configured = m_templateDefaults.value(protocol).toString();
    const QVariantMap item = shareTemplate(configured);
    const QString itemProtocol = item.value("protocol").toString();
    if (!item.isEmpty() && (itemProtocol == protocol || itemProtocol == "general")) return configured;
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
            if (method.value("config").toString().trimmed().isEmpty()) continue;
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
            if (method.value("config").toString().trimmed().isEmpty()) continue;
            const QString protocol = protocolKeyForMethod(method);
            QString templateId = templateIdsByProtocol.value(protocol).toString();
            if (templateId.isEmpty()) templateId = defaultShareTemplateId(protocol);
            QVariantMap selectedTemplate = shareTemplate(templateId);
            const QString templateProtocol = selectedTemplate.value("protocol").toString();
            if (selectedTemplate.isEmpty() || (templateProtocol != protocol && templateProtocol != "general"))
                selectedTemplate = shareTemplate(QStringLiteral("system-") + protocol);
            if (selectedTemplate.isEmpty()) selectedTemplate = shareTemplate(QStringLiteral("system-general"));
            QString body = selectedTemplate.value("body").toString();
            if (!html && selectedTemplate.value("isSystem").toBool()) {
                const bool wrappedKey = method.value("config").toString().startsWith(QStringLiteral("vpn://"));
                body = QStringLiteral("سلام {{NAME}}،\nسرور: {{SERVER}}\nنوع اتصال: {{PROTOCOLS}}\n\n{{CONFIGS}}\n\n"
                                      "راهنمای اتصال در ویندوز با CoCo VPN:\n"
                                      "دانلود برنامه و راهنمای نصب: https://iranux.site/cocoVPN/\n"
                                      "۱. برنامه CoCo VPN را نصب و باز کنید.\n۲. علامت + پایین صفحه را بزنید.\n");
                body += wrappedKey
                    ? QStringLiteral("۳. متن کلید بالا را کامل کپی کنید و در کادر کلید برنامه وارد کنید. سپس ادامه را بزنید.\n")
                    : QStringLiteral("۳. برای دریافت فایل تنظیمات این حساب، خروجی HTML را باز کنید و دکمه دانلود فایل تنظیمات را بزنید. سپس در برنامه گزینه فایل تنظیمات اتصال را انتخاب کنید و فایل همان حساب را باز کنید.\n");
                body += QStringLiteral("۴. حساب اضافه‌شده را انتخاب کنید و دکمه اتصال را بزنید. برای قطع اتصال، همان دکمه را دوباره بزنید.\n"
                                       "برای راهنمای دستگاه‌های دیگر و نمایش کد QR در صورت موجود بودن، فایل HTML را استفاده کنید.");
            }
            QVariantMap singleMethodGroup = group;
            singleMethodGroup.insert("methods", QVariantList{method});
            sections.append(html ? renderGuidedAccount(singleMethodGroup,
                                    selectedTemplate.value("isSystem").toBool() ? QString() : body)
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

namespace {
QString shareResource(const QString &name)
{
    QFile file(QStringLiteral(":/share/") + name);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.readAll());
}
QString shareAsset(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(file.readAll().toBase64());
}
// Replace tokens in one pass: account text and configuration may themselves contain {{...}}.
QString shareSubstitute(const QString &source, const QMap<QString, QString> &values)
{
    static const QRegularExpression token(QStringLiteral(R"(\{\{([A-Z_]+)\}\})"));
    QString result;
    qsizetype offset = 0;
    auto matches = token.globalMatch(source);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += source.mid(offset, match.capturedStart() - offset);
        result += values.value(match.captured(1), match.captured());
        offset = match.capturedEnd();
    }
    return result + source.mid(offset);
}
}

QString ExportUiController::wrapAccountsHtml(const QString &sections) const
{
    static const QString document = shareResource(QStringLiteral("document.html"));
    static const QString regular = shareAsset(QStringLiteral(":/fonts/Shabnam.ttf"));
    static const QString bold = shareAsset(QStringLiteral(":/fonts/Shabnam-Bold.ttf"));
    static const QString logo = shareAsset(QStringLiteral(":/images/cocoVpnLogo.png"));
    QString index;
    static const QRegularExpression accounts(QStringLiteral("<article class=\"account\" id=\"([^\"]+)\" data-label=\"([^\"]+)\""));
    auto entries = accounts.globalMatch(sections);
    while (entries.hasNext()) {
        const auto entry = entries.next();
        index += QStringLiteral("<a class=\"button\" href=\"#%1\">%2</a> ").arg(entry.captured(1), entry.captured(2));
    }
    return shareSubstitute(document, {{"SECTIONS", sections}, {"INDEX", index}, {"FONT_REGULAR", regular},
                                    {"FONT_BOLD", bold}, {"LOGO", logo},
                                    {"FONT_LICENSE", shareResource(QStringLiteral("font-license.txt")).toHtmlEscaped()}});
}

QString ExportUiController::renderGuidedAccount(const QVariantMap &group, const QString &message, bool sample) const
{
    const QVariantList methods = group.value("methods").toList();
    if (methods.size() != 1) return {};
    const QVariantMap method = methods.first().toMap();
    const QString config = method.value("config").toString();
    const QString protocol = protocolKeyForMethod(method);
    // Wrapped VPN keys require a compatible client, even when their protocol is OpenVPN or IKEv2.
    const bool key = config.startsWith(QStringLiteral("vpn://"));
    const QString guide = key || protocol == "ikev2" ? QStringLiteral("general") : protocol;
    const QString app = QStringLiteral("CoCo VPN");
    const QString extension = guide == "awg" || guide == "wireguard" ? QStringLiteral(".conf")
                            : guide == "openvpn" ? QStringLiteral(".ovpn")
                            : guide == "xray" ? QStringLiteral(".json") : QStringLiteral(".txt");
    QString filename = group.value("name").toString();
    filename.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x{0000}-\\x{001f}]")), QStringLiteral("_"));
    filename = filename.trimmed();
    if (filename.isEmpty()) filename = QStringLiteral("coco-vpn");
    filename = QStringLiteral("CoCoVPN_") + filename.left(60) + "-" + protocol + "-"
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")) + extension;
    const QString id = QUuid::createUuid().toString(QUuid::Id128);
    QString qrBlock;
    // OpenVPN Connect cannot import the segmented Amnezia QR series. Prefer its .ovpn file.
    if (!sample && guide != "openvpn") {
        QVariantList codes = method.value("qrCodes").toList();
        if (key) {
            codes.clear();
            const QString qr = qrCodeUtils::generatePlainQrCodeImage(config.toUtf8());
            if (!qr.isEmpty()) codes.append(qr);
        }
        for (const auto &value : codes) {
            QString qr = value.toString();
            qr.replace("data:image/svg;base64,", "data:image/svg+xml;base64,");
            if (qr.startsWith("data:image/"))
                qrBlock += QStringLiteral("<div class=\"qr-box\"><img src=\"%1\" alt=\"کد QR همین حساب\" width=\"186\" height=\"186\"></div>").arg(qr.toHtmlEscaped());
        }
    }
    if (qrBlock.isEmpty()) qrBlock = QStringLiteral("<p class=\"qr-caption\">کد QR برای این حساب موجود نیست؛ از فایل تنظیمات یا متن کلید استفاده کنید.</p>");
    const bool hasQr = qrBlock.contains(QStringLiteral("<img"));
    const QString intro = guide == "general"
        ? QStringLiteral("متن کلید این حساب را کپی کنید و در برنامه وارد کنید. مراحل دقیق در راهنمای کنار صفحه آمده است.")
        : QStringLiteral("برای اضافه‌کردن این حساب، فایل تنظیمات را دانلود کنید و مراحل راهنما را انجام دهید.");
    const QString importHelp = guide == "general"
        ? QStringLiteral("در این حساب باید متن کلید را کامل کپی کنید و در کادر کلید برنامه وارد کنید. فایل TXT برای نگهداری متن کلید است؛ آن را به‌عنوان فایل تنظیمات وارد نکنید.")
        : QStringLiteral("فایل تنظیمات را از دکمه همین صفحه دانلود کنید و در برنامه %1 وارد کنید. اگر برنامه فایل را قبول نکرد، آن را از لینک رسمی به‌روز کنید و متن خطا را برای ارائه‌دهنده حساب بفرستید.").arg(app.toHtmlEscaped());
    QString note = QStringLiteral("<div class=\"tip\">برنامه اصلی در ویندوز: %1. راهنمای برنامه جایگزین در بخش جدا آمده است. برای دستگاه‌های دیگر، برنامه معرفی‌شده در راهنمای همان دستگاه را نصب کنید.</div>").arg(app.toHtmlEscaped());
    const QString source = guide == "awg" ? "https://docs.amnezia.org/documentation/instructions/use-amneziawg-app/"
                         : guide == "wireguard" ? "https://www.wireguard.com/install/"
                         : guide == "openvpn" ? "https://openvpn.net/connect-docs/import-profile.html"
                         : guide == "general" ? "https://docs.amnezia.org/documentation/instructions/connect-via-text-key/"
                         : "https://docs.amnezia.org/documentation/instructions/connect-via-config/";
    note += QStringLiteral("<a class=\"source\" href=\"https://iranux.site/cocoVPN/\" target=\"_blank\" rel=\"noopener noreferrer\">دانلود و راهنمای CoCo VPN</a> · ");
    note += QStringLiteral("<a class=\"source\" href=\"%1\" target=\"_blank\" rel=\"noopener noreferrer\">راهنمای رسمی برنامه جایگزین</a>").arg(source);
    const QString sampleNote = sample ? QStringLiteral("<p class=\"sample-note\">این صفحه پیش‌نمایش قالب با اطلاعات نمونه است. فایل و متن نمونه برای اتصال قابل استفاده نیستند.</p>") : QString();
    QString fragment = shareSubstitute(shareResource(QStringLiteral("account.html")),
        {{"ACCOUNT", group.value("name").toString().toHtmlEscaped()}, {"SERVER", group.value("serverName").toString().toHtmlEscaped()},
         {"PROTOCOL", method.value("name", protocolDisplayName(protocol)).toString().toHtmlEscaped()},
         {"EXTENSION", extension}, {"APP", app.toHtmlEscaped()}, {"ID", id}, {"LABEL", (group.value("name").toString() + " · " + method.value("name").toString()).toHtmlEscaped()},
         {"GUIDES", shareResource(guide + ".html")}, {"GUIDE_NOTE", note}, {"QR_BLOCK", qrBlock},
         {"SAMPLE_NOTE", sampleNote}, {"CONNECTION_INTRO", intro}, {"IMPORT_HELP", importHelp},
         {"QR_HINT", hasQr ? QStringLiteral("برای اسکن QR، این صفحه را روی دستگاه دیگری باز کنید و در برنامه گزینه اسکن QR را بزنید.") : QString()}, {"MESSAGE", message.isEmpty() ? QString() : renderAccountTemplateFragment(group, message)},
         {"CONFIG", config.toHtmlEscaped()}, {"CONFIG_DATA", QString::fromLatin1(config.toUtf8().toBase64())},
         {"CONF_FILENAME", filename}});
    if (sample) {
        fragment.replace(QRegularExpression(QStringLiteral("href=\"data:application/octet-stream;base64,[^\"]*\"")), QStringLiteral("aria-disabled=\"true\""));
        fragment.replace("download=", "data-sample-download=");
    }
    return fragment;
}

QString ExportUiController::renderShareTemplatePreview(const QString &templateId) const
{
    return renderTemplatePreview(shareTemplate(templateId));
}

QString ExportUiController::renderShareTemplateDraft(const QString &name, const QString &body, const QString &protocol) const
{
    return renderTemplatePreview({{"name", name}, {"body", body}, {"protocol", protocol}, {"isSystem", false}});
}

QString ExportUiController::renderTemplatePreview(const QVariantMap &item) const
{
    if (item.isEmpty()) return {};
    const QString protocol = item.value("protocol").toString();
    amnezia::DockerContainer container = amnezia::DockerContainer::None;
    if (protocol == "awg") container = amnezia::DockerContainer::Awg;
    else if (protocol == "wireguard") container = amnezia::DockerContainer::WireGuard;
    else if (protocol == "openvpn") container = amnezia::DockerContainer::OpenVpn;
    else if (protocol == "xray") container = amnezia::DockerContainer::Xray;
    else if (protocol == "ikev2") container = amnezia::DockerContainer::Ipsec;
    const QVariantMap method{{"container", static_cast<int>(container)}, {"name", protocolDisplayName(protocol)},
                             {"config", QStringLiteral("اطلاعات نمونه برای پیش‌نمایش؛ قابل استفاده برای اتصال نیست.")}, {"qrCodes", QVariantList{}}};
    const QVariantMap group{{"name", QStringLiteral("حساب نمونه")}, {"serverName", QStringLiteral("سرور نمونه")},
                            {"methods", QVariantList{method}}};
    return wrapAccountsHtml(renderGuidedAccount(group, item.value("isSystem").toBool() ? QString() : item.value("body").toString(), true));
}

bool ExportUiController::openHtmlPreview(const QString &html)
{
    if (html.isEmpty() || !m_previewDirectory.isValid()) return false;
    const QString path = m_previewDirectory.filePath(QStringLiteral("CoCoVPN-preview.html"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray data = html.toUtf8();
    if (file.write(data) != data.size()) return false;
    file.close();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
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
    const QString output = shareSubstitute(templateBody.toHtmlEscaped().replace("\n", "<br/>"),
        {{"NAME", group.value("name").toString().toHtmlEscaped()},
         {"SERVER", group.value("serverName").toString().toHtmlEscaped()},
         {"PROTOCOLS", QString("<div class=\"protocols\">%1</div>").arg(protocolBadges.join(QString()))},
         {"CONFIGS", configs}, {"QR", qrs.isEmpty() ? QStringLiteral("کد QR برای این حساب موجود نیست.")
                                                    : QString("<div class=\"qr-list\">%1</div>").arg(qrs)}});
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

    return shareSubstitute(templateBody,
        {{"NAME", group.value("name").toString()}, {"SERVER", group.value("serverName").toString()},
         {"PROTOCOLS", protocols.join(QStringLiteral(", "))}, {"CONFIGS", configs.join(QStringLiteral("\n\n"))},
         {"QR", QStringLiteral("برای مشاهده کد QR در صورت موجود بودن، فایل HTML را ذخیره کنید.")}});
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
