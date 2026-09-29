#include "exportUiController.h"

#include <QDebug>
#include <QDateTime>
#include <QStringList>
#include <QBuffer>
#include <QRegularExpression>
#include <QImage>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include "../systemController.h"
#include "core/utils/qrCodeUtils.h"

ExportUiController::ExportUiController(ExportController* exportController, SecureQSettings* settings, QObject *parent)
    : QObject(parent),
      m_exportController(exportController),
      m_settings(settings)
{
    if (m_settings) {
        // Remove persisted CoCo logical account bundles from earlier builds.
        m_settings->remove("Sharing/accountGroups");
        m_shareLanguage = m_settings->value("Sharing/language", QStringLiteral("fa")).toString();
    }
    connect(m_exportController, &ExportController::revokeFinished, this, [this](ErrorCode errorCode) {
        if (errorCode == ErrorCode::NoError) {
            emit revokeConfigFinished();
        } else {
            emit exportErrorOccurred(errorCode);
        }
    });
}

QString ExportUiController::shareLanguage() const { return m_shareLanguage; }
QString ExportUiController::shareKind() const { return m_shareKind; }
QString ExportUiController::shareServer() const { return m_shareServer; }
QString ExportUiController::shareAccount() const { return m_shareAccount; }
QString ExportUiController::shareCreatedAt() const { return m_shareCreatedAt; }
bool ExportUiController::shareNativeFormat() const { return m_shareNativeFormat; }

QVariantList ExportUiController::shareTemplateKinds() const
{
    const QList<QPair<QString, QString>> kinds = {
        {"full", tr("Administrator / Full access")}, {"openvpn", "OpenVPN"}, {"wireguard", "WireGuard"},
        {"awg", "AmneziaWG (AWG)"}, {"ikev2", "IKEv2 / IPsec"}, {"xray", "XRay (VLESS / VMess / Trojan)"},
        {"shadowsocks", "Shadowsocks"}, {"tor", tr("Tor website")}, {"dns", "DNS"}, {"sftp", "SFTP"},
        {"socks5", "SOCKS5"}, {"mtproxy", "MTProxy (Telegram)"}, {"telemt", "Telemt (Telegram)"}, {"tproxy", "TProxy (Telegram WEB)"}
    };
    QVariantList result;
    for (const auto &kind : kinds) result.append(QVariantMap{{"id", kind.first}, {"name", kind.second}});
    return result;
}

void ExportUiController::setShareLanguage(const QString &language)
{
    const QString normalized = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    if (normalized == m_shareLanguage) return;
    m_shareLanguage = normalized;
    if (m_settings) m_settings->setValue("Sharing/language", normalized);
    emit shareLanguageChanged();
}

QString ExportUiController::defaultShareTemplate(const QString &kind, const QString &language) const
{
    const bool fa = language != QLatin1String("en");
    if (kind == QLatin1String("tor"))
        return fa ? QStringLiteral("راهنمای بازکردن وب‌سایت تور\n۱. Tor Browser رسمی را نصب کنید: https://www.torproject.org/download/\n۲. مرورگر را باز کنید و اتصال به شبکه Tor را برقرار کنید.\n۳. نشانی onion ارسال‌شده را در نوار آدرس وارد کنید.\n\nاگر صفحه باز نشد، اتصال Tor را دوباره برقرار کنید و نشانی کامل را از مدیر سرور بگیرید.\n\nسرور: {{SERVER}}\nتاریخ آماده‌سازی راهنما: {{CREATED}}\nنشانی یا اطلاعات سایت:\n{{CONFIG}}")
                   : QStringLiteral("Open a Tor website\n1. Install the official Tor Browser: https://www.torproject.org/download/\n2. Open it and connect to the Tor network.\n3. Enter the onion address you received in the address bar.\n\nIf the page does not load, reconnect Tor and ask the server administrator for the complete address.\n\nServer: {{SERVER}}\nGuide created: {{CREATED}}\nWebsite address or details:\n{{CONFIG}}");
    if (kind == QLatin1String("sftp"))
        return fa ? QStringLiteral("راهنمای دسترسی امن به فایل‌ها (SFTP)\n۱. FileZilla Client رسمی را نصب کنید: https://filezilla-project.org/download.php?type=client\n۲. برنامه را باز کنید و اطلاعات میزبان، پورت، نام کاربری و رمز را از بخش اطلاعات اتصال وارد کنید.\n۳. روش انتقال را SFTP انتخاب کنید و اتصال را بزنید.\n۴. فایل‌ها را بین پوشه محلی و سرور جابه‌جا کنید.\n\nاگر هشدار اثرانگشت سرور نمایش داده شد، آن را با مدیر سرور تأیید کنید. رمز و کلید خصوصی را محرمانه نگه دارید.\n\nسرور: {{SERVER}}\nتاریخ ساخت دسترسی: {{CREATED}}\nاطلاعات اتصال:\n{{CONFIG}}")
                   : QStringLiteral("Secure file access (SFTP)\n1. Install the official FileZilla Client: https://filezilla-project.org/download.php?type=client\n2. Enter the host, port, username, and password from the connection details.\n3. Choose SFTP as the transfer protocol and connect.\n4. Move files between your computer and the server folders.\n\nIf the server fingerprint prompt appears, verify it with the administrator. Keep passwords and private keys confidential.\n\nServer: {{SERVER}}\nAccess created: {{CREATED}}\nConnection details:\n{{CONFIG}}");
    if (kind == QLatin1String("socks5"))
        return fa ? QStringLiteral("راهنمای تنظیم SOCKS5\n۱. تنظیمات Proxy را در برنامه‌ای که از SOCKS5 پشتیبانی می‌کند باز کنید.\n۲. نوع پروکسی را SOCKS5 بگذارید.\n۳. میزبان، پورت و در صورت نیاز نام کاربری و رمز را از اطلاعات اتصال وارد کنید.\n۴. اتصال را ذخیره و آزمایش کنید.\n\nاگر کار نکرد، میزبان و پورت را با فرستنده بررسی کنید؛ برخی برنامه‌ها برای DNS گزینه جداگانه دارند.\n\nسرور: {{SERVER}}\nتاریخ ساخت دسترسی: {{CREATED}}\nاطلاعات اتصال:\n{{CONFIG}}")
                   : QStringLiteral("Configure SOCKS5\n1. Open proxy settings in an app that supports SOCKS5.\n2. Select SOCKS5 as the proxy type.\n3. Enter the host, port, and username/password if provided.\n4. Save and test the connection.\n\nIf it fails, verify the host and port with the sender. Some apps have a separate DNS proxy option.\n\nServer: {{SERVER}}\nAccess created: {{CREATED}}\nConnection details:\n{{CONFIG}}");
    if (kind == QLatin1String("dns"))
        return fa ? QStringLiteral("راهنمای DNS اختصاصی\nاین سرویس فقط نشانی DNS را در اختیار دستگاه یا برنامه سازگار قرار می‌دهد و به‌تنهایی تونل VPN نیست.\n۱. نشانی DNS را از اطلاعات اتصال بردارید.\n۲. آن را در بخش DNS خصوصی یا تنظیمات شبکه دستگاه وارد کنید.\n۳. اتصال را ذخیره و یک وب‌سایت را دوباره باز کنید.\n\nاگر اینترنت قطع شد، تنظیم قبلی DNS را بازگردانید و از مدیر سرور درباره دسترس‌پذیری سرویس بپرسید.\n\nسرور: {{SERVER}}\nاطلاعات DNS:\n{{CONFIG}}")
                   : QStringLiteral("Use the private DNS service\nThis service provides a DNS address for a compatible device or app; it is not a VPN tunnel by itself.\n1. Copy the DNS address from the connection details.\n2. Enter it under Private DNS or the device network settings.\n3. Save and reopen a website to check it.\n\nIf internet access stops, restore the previous DNS setting and ask the server administrator whether the service is available.\n\nServer: {{SERVER}}\nDNS details:\n{{CONFIG}}");
    if (kind == QLatin1String("mtproxy") || kind == QLatin1String("telemt") || kind == QLatin1String("tproxy"))
        return fa ? QStringLiteral("راهنمای اتصال پروکسی تلگرام ({{PROTOCOL}})\n۱. برنامه رسمی تلگرام را نصب کنید: https://telegram.org/apps\n۲. روی پیوند پروکسی بزنید یا QR را اسکن کنید.\n۳. در تلگرام گزینه افزودن پروکسی را تأیید کنید.\n۴. وضعیت اتصال را در تنظیمات تلگرام بررسی کنید.\n\nاگر وصل نشد، تلگرام را به‌روز کنید و از فرستنده بخواهید فعال بودن سرویس و پورت سرور را بررسی کند.\n\nسرور: {{SERVER}}\nنام سرویس: {{NAME}}\nتاریخ ساخت پیوند: {{CREATED}}\nپیوند اتصال:\n{{CONFIG}}\n{{QR}}")
                   : QStringLiteral("Telegram proxy connection guide ({{PROTOCOL}})\n1. Install the official Telegram app: https://telegram.org/apps\n2. Tap the proxy link or scan its QR code.\n3. Confirm Add Proxy in Telegram.\n4. Check the connection status in Telegram settings.\n\nIf it fails, update Telegram and ask the sender to verify the service and server port.\n\nServer: {{SERVER}}\nService name: {{NAME}}\nLink created: {{CREATED}}\nConnection link:\n{{CONFIG}}\n{{QR}}");
    const QString guide = kind == QLatin1String("full")
        ? (fa ? QStringLiteral("دسترسی کامل مدیر سرور\nاین دسترسی امکان مدیریت سرور، نصب و حذف سرویس‌ها و ساخت یا لغو کاربران را می‌دهد. فقط به فرد مورداعتماد بدهید.\n\nمراحل اتصال:\n۱. برنامه رسمی CoCo VPN را از {{APP_LINK}} نصب کنید.\n۲. برنامه را باز کنید و گزینه واردکردن کانفیگ را بزنید.\n۳. فایل پیوست را انتخاب کنید یا QR را اسکن کنید.\n۴. برای اتصال، سرور را انتخاب و دکمه اتصال را بزنید.\n۵. برای مدیریت سرور، در بخش تنظیمات سرور از اطلاعات SSH همین فایل استفاده کنید.\n\nاگر اتصال برقرار نشد، اینترنت دستگاه را بررسی کنید، تاریخ و ساعت را خودکار بگذارید و از مدیر سرور بخواهید دسترسی SSH و VPN را بررسی کند.\n\nسرور: {{SERVER}}\nنام دسترسی: {{NAME}}\nتاریخ تهیه فایل دسترسی: {{CREATED}}\nاطلاعات ورود و کانفیگ:\n{{CONFIG}}\n{{QR}}")
                 : QStringLiteral("Administrator / Full server access\nThis credential can manage the server, install or remove services, and create or revoke users. Share it only with someone you trust.\n\nConnect step by step:\n1. Install the official CoCo VPN app from {{APP_LINK}}.\n2. Open the app and choose Import configuration.\n3. Select the attached file or scan the QR code.\n4. Select the server and tap Connect.\n5. To administer the server, use the SSH details in this file in the server settings.\n\nIf connection fails, check internet access, set date and time automatically, and ask the server administrator to verify SSH and VPN access.\n\nServer: {{SERVER}}\nAccess name: {{NAME}}\nShare file created: {{CREATED}}\nLogin details and configuration:\n{{CONFIG}}\n{{QR}}"))
        : (fa ? QStringLiteral("راهنمای اتصال {{PROTOCOL}}\n۱. برنامه رسمی را از پیوند زیر نصب کنید: {{APP_LINK}}\n۲. برنامه را باز کنید و افزودن اتصال را انتخاب کنید.\n۳. فایل کانفیگ یا QR همین راهنما را وارد کنید.\n۴. اتصال را فعال کنید.\n\nاگر وصل نشد، یک بار اینترنت را عوض کنید، تاریخ دستگاه را بررسی کنید و از فرستنده بخواهید فعال بودن حساب و سرور را تأیید کند.\n\nاطلاعات دسترسی:\nنوع: {{PROTOCOL}}\nسرور: {{SERVER}}\nنام دسترسی: {{NAME}}\nتاریخ ساخت: {{CREATED}}\n\nکانفیگ:\n{{CONFIG}}\n\nQR:\n{{QR}}")
             : QStringLiteral("{{PROTOCOL}} connection guide\n1. Install the official app from this link: {{APP_LINK}}\n2. Open it and choose Add connection.\n3. Import the attached configuration or scan the QR code in this guide.\n4. Turn the connection on.\n\nIf it fails, try another internet connection, check the device date and time, and ask the sender to confirm the account and server are active.\n\nAccess details:\nType: {{PROTOCOL}}\nServer: {{SERVER}}\nAccess name: {{NAME}}\nCreated: {{CREATED}}\n\nConfiguration:\n{{CONFIG}}\n\nQR code:\n{{QR}}"));
    return guide;
}

QString ExportUiController::shareTemplate(const QString &kind, const QString &language) const
{
    const QString lang = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    const QString selectedId = defaultShareTemplateId(kind, lang);
    if (m_settings && selectedId != QLatin1String("builtin")) {
        const QString customKey = QStringLiteral("Sharing/customTemplates/%1/%2").arg(kind, lang);
        const QJsonArray custom = QJsonDocument::fromJson(m_settings->value(customKey).toByteArray()).array();
        for (const QJsonValue &entry : custom) {
            const QJsonObject object = entry.toObject();
            if (object.value(QStringLiteral("id")).toString() == selectedId)
                return object.value(QStringLiteral("body")).toString();
        }
    }
    const QString key = QStringLiteral("Sharing/contentTemplates/%1/%2").arg(kind, lang);
    if (m_settings && m_settings->value(key).isValid()) return m_settings->value(key).toString();
    return defaultShareTemplate(kind, lang);
}

QVariantList ExportUiController::shareTemplates(const QString &kind, const QString &language) const
{
    const QString lang = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    const QString builtInKey = QStringLiteral("Sharing/contentTemplates/%1/%2").arg(kind, lang);
    const QString builtIn = m_settings && m_settings->value(builtInKey).isValid()
        ? m_settings->value(builtInKey).toString() : defaultShareTemplate(kind, lang);
    QVariantList result{QVariantMap{{"id", QStringLiteral("builtin")},
                                    {"name", lang == QLatin1String("en") ? QStringLiteral("Built-in default") : QStringLiteral("قالب پیش‌فرض داخلی")},
                                    {"body", builtIn}, {"builtIn", true}}};
    if (!m_settings) return result;
    const QString customKey = QStringLiteral("Sharing/customTemplates/%1/%2").arg(kind, lang);
    const QJsonArray custom = QJsonDocument::fromJson(m_settings->value(customKey).toByteArray()).array();
    for (const QJsonValue &entry : custom) result.append(entry.toObject().toVariantMap());
    return result;
}

QString ExportUiController::defaultShareTemplateId(const QString &kind, const QString &language) const
{
    if (!m_settings) return QStringLiteral("builtin");
    const QString lang = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    return m_settings->value(QStringLiteral("Sharing/defaultTemplates/%1/%2").arg(kind, lang),
                             QStringLiteral("builtin")).toString();
}

void ExportUiController::setDefaultShareTemplate(const QString &kind, const QString &language, const QString &templateId)
{
    if (!m_settings) return;
    const QString lang = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    bool valid = templateId == QLatin1String("builtin");
    for (const QVariant &entry : shareTemplates(kind, lang))
        valid = valid || entry.toMap().value(QStringLiteral("id")).toString() == templateId;
    if (!valid) return;
    m_settings->setValue(QStringLiteral("Sharing/defaultTemplates/%1/%2").arg(kind, lang), templateId);
}

void ExportUiController::saveShareTemplate(const QString &kind, const QString &language, const QString &body)
{
    if (!m_settings || body.trimmed().isEmpty()) return;
    const QString lang = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    m_settings->setValue(QStringLiteral("Sharing/contentTemplates/%1/%2").arg(kind, lang), body);
    emit shareTemplateChanged();
}

QString ExportUiController::saveCustomShareTemplate(const QString &kind, const QString &language,
                                                     const QString &templateId, const QString &name,
                                                     const QString &body)
{
    if (!m_settings || name.trimmed().isEmpty() || body.trimmed().isEmpty()) return {};
    const QString lang = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    const QString key = QStringLiteral("Sharing/customTemplates/%1/%2").arg(kind, lang);
    QJsonArray custom = QJsonDocument::fromJson(m_settings->value(key).toByteArray()).array();
    const QString id = templateId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : templateId;
    QJsonArray updated;
    bool replaced = false;
    for (const QJsonValue &entry : custom) {
        QJsonObject object = entry.toObject();
        if (object.value(QStringLiteral("id")).toString() == id) {
            object.insert(QStringLiteral("name"), name.trimmed());
            object.insert(QStringLiteral("body"), body);
            replaced = true;
        }
        updated.append(object);
    }
    if (!replaced) updated.append(QJsonObject{{"id", id}, {"name", name.trimmed()}, {"body", body}, {"builtIn", false}});
    m_settings->setValue(key, QJsonDocument(updated).toJson(QJsonDocument::Compact));
    emit shareTemplateChanged();
    return id;
}

void ExportUiController::resetShareTemplate(const QString &kind, const QString &language)
{
    if (!m_settings) return;
    const QString lang = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("fa");
    m_settings->remove(QStringLiteral("Sharing/contentTemplates/%1/%2").arg(kind, lang));
    emit shareTemplateChanged();
}

void ExportUiController::setSharingContext(const QString &kind, const QString &serverName,
                                            const QString &accountName, const QString &createdAt,
                                            bool nativeFormat)
{
    m_shareKind = kind;
    m_shareServer = serverName;
    m_shareAccount = accountName;
    m_shareCreatedAt = createdAt.isEmpty() ? QDateTime::currentDateTime().toString(Qt::ISODate) : createdAt;
    m_shareNativeFormat = nativeFormat;
    emit sharingContextChanged();
}

QString ExportUiController::htmlEscape(const QString &value) const { return value.toHtmlEscaped(); }

QString ExportUiController::renderShareDocument(const QString &kind, const QString &language,
                                                 const QString &serverName, const QString &accountName,
                                                 const QString &createdAt, const QString &config,
                                                 const QStringList &qrCodes, bool nativeFormat, bool html) const
{
    return renderShareDocumentWithTemplate(kind, language, serverName, accountName, createdAt,
                                           config, qrCodes, shareTemplate(kind, language),
                                           nativeFormat, html, false);
}

QString ExportUiController::renderShareTemplatePreview(const QString &kind, const QString &language,
                                                        const QString &templateBody, bool html) const
{
    const bool fa = language != QLatin1String("en");
    const QString server = fa ? QStringLiteral("نمونه سرور") : QStringLiteral("Example server");
    const QString account = fa ? QStringLiteral("حساب نمونه") : QStringLiteral("Example account");
    const QString date = fa ? QStringLiteral("تاریخ نمونه") : QStringLiteral("Example date");
    const QString config = fa ? QStringLiteral("نمونه؛ اطلاعات اتصال واقعی هنگام اشتراک‌گذاری حساب درج می‌شود.")
                              : QStringLiteral("Sample only; real connection details appear when an account is shared.");
    return renderShareDocumentWithTemplate(kind, language, server, account, date, config, {},
                                           templateBody, false, html, true);
}

bool ExportUiController::saveShareTemplatePreview(const QString &fileName, const QString &kind,
                                                   const QString &language, const QString &templateBody,
                                                   bool html) const
{
    return SystemController::saveFile(fileName, renderShareTemplatePreview(kind, language, templateBody, html));
}

QString ExportUiController::renderShareDocumentWithTemplate(const QString &kind, const QString &language,
                                                             const QString &serverName, const QString &accountName,
                                                             const QString &createdAt, const QString &config,
                                                             const QStringList &qrCodes, const QString &templateBody,
                                                             bool nativeFormat, bool html, bool preview) const
{
    QString protocol = kind.toUpper();
    const bool fa = language != QLatin1String("en");
    if (kind == QLatin1String("full")) protocol = language == QLatin1String("en") ? QStringLiteral("Full Access") : QStringLiteral("دسترسی کامل");
    if (kind == QLatin1String("awg")) protocol = QStringLiteral("AmneziaWG (AWG)");
    QString appLink = QStringLiteral("https://github.com/Iranux-Master/CoCo-VPN/releases");
    if (nativeFormat && kind == QLatin1String("wireguard")) appLink = QStringLiteral("https://www.wireguard.com/install/");
    if (nativeFormat && kind == QLatin1String("openvpn")) appLink = QStringLiteral("https://openvpn.net/client/");
    if (kind == QLatin1String("mtproxy") || kind == QLatin1String("telemt") || kind == QLatin1String("tproxy")) appLink = QStringLiteral("https://telegram.org/apps");
    QString body = templateBody;
    body.replace("{{PROTOCOL}}", protocol).replace("{{APP_LINK}}", appLink)
        .replace("{{SERVER}}", serverName).replace("{{NAME}}", accountName)
        .replace("{{CREATED}}", createdAt);
    body.replace("{{CONFIG}}", html ? QStringLiteral("__COCO_CONFIG_BLOCK__") : config);
    QString qrText = preview
                          ? (fa ? QStringLiteral("پیش‌نمایش: QR واقعی هنگام اشتراک‌گذاری حساب درج می‌شود.")
                                : QStringLiteral("Preview: a real QR code is added when an account is shared."))
                      : html ? QStringLiteral("{{QR_MARKUP}}")
                          : (language == QLatin1String("en") ? QStringLiteral("QR code is included in the HTML version.") : QStringLiteral("برای مشاهده QR کد، فایل HTML را باز کنید."));
    body.replace("{{QR}}", qrText);
    if (preview) {
        const QString previewNotice = fa
            ? QStringLiteral("پیش‌نمایش قالب — مقادیر نمونه هستند و دسترسی واقعی ایجاد نمی‌کنند.")
            : QStringLiteral("Template preview — sample values only; this is not a working account.");
        body.prepend(previewNotice + QStringLiteral("\n\nCoCo VPN\n\n"));
    }
    if (!html) return body;

    QString content = htmlEscape(body).replace("\n", "<br>");
    const QString escapedLink = htmlEscape(appLink);
    content.replace(escapedLink, QStringLiteral("__COCO_APP_LINK__"));
    const QRegularExpression urlPattern(QStringLiteral("https?://[^\\s<>]+"));
    QRegularExpressionMatchIterator urlMatches = urlPattern.globalMatch(content);
    QString linkedContent;
    int lastUrlEnd = 0;
    while (urlMatches.hasNext()) {
        const QRegularExpressionMatch match = urlMatches.next();
        linkedContent += content.mid(lastUrlEnd, match.capturedStart() - lastUrlEnd);
        const QString url = match.captured();
        linkedContent += QStringLiteral("<a href=\"%1\">%1</a>").arg(url);
        lastUrlEnd = match.capturedEnd();
    }
    linkedContent += content.mid(lastUrlEnd);
    content = linkedContent.replace(QStringLiteral("__COCO_APP_LINK__"),
                                    QStringLiteral("<a href=\"%1\">%1</a>").arg(escapedLink));
    const QString copyButtonLabel = fa ? QStringLiteral("کپی کانفیگ") : QStringLiteral("Copy config");
    const QString configBlock = QStringLiteral("<div class=\"config-actions\"><button type=\"button\" class=\"copy-config\" onclick=\"copyCocoConfig()\">%1</button><span id=\"copy-status\" role=\"status\" aria-live=\"polite\"></span></div><pre id=\"coco-config\" dir=\"ltr\">%2</pre>")
                                    .arg(copyButtonLabel, htmlEscape(config));
    content.replace("__COCO_CONFIG_BLOCK__", configBlock);
    content.replace(htmlEscape(qrText), QStringLiteral("__COCO_QR_MARKUP__"));
    QString qrMarkup;
    for (const QString &qr : qrCodes) {
        QString source = qr;
        source.replace(QStringLiteral("data:image/svg;base64,"), QStringLiteral("data:image/svg+xml;base64,"));
        if (source.startsWith("data:image/")) qrMarkup += QStringLiteral("<img class=\"qr\" alt=\"QR code\" src=\"%1\">").arg(htmlEscape(source));
    }
    content.replace("__COCO_QR_MARKUP__", qrMarkup.isEmpty() ? qrText : qrMarkup);
    QImage logo(QStringLiteral(":/images/cocoVpnIcon.png"));
    QByteArray logoPng;
    QBuffer logoBuffer(&logoPng);
    logoBuffer.open(QIODevice::WriteOnly);
    if (!logo.isNull()) logo.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&logoBuffer, "PNG");
    const QString logoMarkup = logoPng.isEmpty() ? QStringLiteral("<strong>CoCo VPN</strong>")
        : QStringLiteral("<img class=\"logo\" alt=\"CoCo VPN\" src=\"data:image/png;base64,%1\"><strong>CoCo VPN</strong>").arg(QString::fromLatin1(logoPng.toBase64()));
    QString fontCss;
    if (fa) {
        const auto embeddedFont = [](const QString &path) {
            QFile fontFile(path);
            return fontFile.open(QIODevice::ReadOnly) ? QString::fromLatin1(fontFile.readAll().toBase64()) : QString();
        };
        fontCss = QStringLiteral("@font-face{font-family:Shabnam;src:url(data:font/ttf;base64,%1) format('truetype');font-style:normal;font-weight:400}@font-face{font-family:Shabnam;src:url(data:font/ttf;base64,%2) format('truetype');font-style:normal;font-weight:500}@font-face{font-family:Shabnam;src:url(data:font/ttf;base64,%3) format('truetype');font-style:normal;font-weight:700}")
                       .arg(embeddedFont(QStringLiteral(":/fonts/Shabnam.ttf")),
                            embeddedFont(QStringLiteral(":/fonts/Shabnam-Medium.ttf")),
                            embeddedFont(QStringLiteral(":/fonts/Shabnam-Bold.ttf")));
    }
    const QString fontFamily = fa ? QStringLiteral("Shabnam,Arial,Tahoma,sans-serif")
                                  : QStringLiteral("Arial,Tahoma,sans-serif");
    QString htmlDocument = QStringLiteral("<!doctype html><html lang=\"%1\" dir=\"%2\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>CoCo VPN · %3</title><style>%7body{margin:0;background:#eef3ef;color:#17231d;font:16px/1.8 %8}.page{max-width:820px;margin:32px auto;padding:28px;background:#fff;border:1px solid #d8e4dc;border-radius:20px}header{display:flex;align-items:center;gap:12px;color:#176b36;font-size:19px;border-bottom:1px solid #d8e4dc;padding-bottom:16px;margin-bottom:20px}.logo{width:42px;height:42px;object-fit:contain}.config-actions{display:flex;align-items:center;gap:12px;margin:12px 0 0}.copy-config{min-height:44px;padding:9px 16px;border:0;border-radius:10px;background:#176b36;color:#fff;font:inherit;cursor:pointer}.copy-config:hover{background:#12552b}.copy-config:focus-visible{outline:3px solid #c57b32;outline-offset:2px}#copy-status{font-size:14px;color:#176b36}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#f3f7f4;padding:16px;border-radius:12px;font:13px/1.6 Consolas,monospace}.qr{display:block;width:220px;max-width:100%%;margin:18px auto}a{color:#176b36}footer{color:#637268;font-size:12px;margin-top:24px}@media print{body{background:#fff}.page{margin:0;border:0}.copy-config{display:none}}</style></head><body><main class=\"page\"><header>%6　|　%3</header><article>%4</article><footer>%5</footer></main></body></html>")
        .arg(fa ? "fa" : "en", fa ? "rtl" : "ltr", htmlEscape(protocol), content,
             fa ? QStringLiteral("این اطلاعات محرمانه است. فقط برای دریافت‌کننده موردنظر ارسال کنید.") : QStringLiteral("Connection details are private. Share only with the intended recipient."), logoMarkup)
        .arg(fontCss, fontFamily);
    const QString copiedMessage = fa ? QStringLiteral("کپی شد") : QStringLiteral("Copied");
    const QString copyFailedMessage = fa ? QStringLiteral("کپی انجام نشد") : QStringLiteral("Copy failed");
    htmlDocument.replace("</body>", QStringLiteral("<script>function copyCocoConfig(){const config=document.getElementById('coco-config');const status=document.getElementById('copy-status');const copied='%1';const failed='%2';if(!config||!status)return;const fallback=()=>{const field=document.createElement('textarea');field.value=config.innerText;field.setAttribute('readonly','');field.style.position='fixed';field.style.opacity='0';document.body.appendChild(field);field.select();let ok=false;try{ok=document.execCommand('copy')}catch(e){}field.remove();status.textContent=ok?copied:failed};if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(config.innerText).then(()=>status.textContent=copied).catch(fallback)}else fallback()}</script></body>").arg(copiedMessage, copyFailedMessage));
    return htmlDocument;
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
