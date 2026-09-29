#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "secureQSettings.h"
#include "core/utils/qrCodeUtils.h"
#include "core/controllers/selfhosted/exportController.h"
#include "ui/controllers/selfhosted/exportUiController.h"

class ShareTemplatesTest : public QObject
{
    Q_OBJECT

private slots:
    void catalogCoversAccessProtocolsAndServices();
    void builtInGuidesExistInBothLanguages();
    void languageAndTemplateOverridesPersist();
    void shareDocumentContainsEscapedConfigAndQr();
    void templatePreviewUsesDraftAndMarksSampleData();
    void oversizedAccessQrIsSkipped();
    void savingGuideDoesNotReplaceConnectionConfig();
    void savedClientCredentialsAreExcludedFromBackups();
};

void ShareTemplatesTest::catalogCoversAccessProtocolsAndServices()
{
    SecureQSettings settings(QStringLiteral("CoCoVpnTests"), QStringLiteral("ShareTemplates"), nullptr, false);
    settings.clearSettings();
    ExportController exporter(nullptr, nullptr);
    ExportUiController controller(&exporter, &settings);

    const QVariantList kinds = controller.shareTemplateKinds();
    QCOMPARE(kinds.size(), 14);
    QStringList ids;
    for (const QVariant &kind : kinds) ids.append(kind.toMap().value(QStringLiteral("id")).toString());
    QVERIFY(ids.contains(QStringLiteral("full")));
    QVERIFY(ids.contains(QStringLiteral("wireguard")));
    QVERIFY(ids.contains(QStringLiteral("mtproxy")));
    QVERIFY(ids.contains(QStringLiteral("tproxy")));
}

void ShareTemplatesTest::builtInGuidesExistInBothLanguages()
{
    SecureQSettings settings(QStringLiteral("CoCoVpnTests"), QStringLiteral("ShareTemplates"), nullptr, false);
    settings.clearSettings();
    ExportController exporter(nullptr, nullptr);
    ExportUiController controller(&exporter, &settings);

    for (const QString &kind : {QStringLiteral("full"), QStringLiteral("wireguard"), QStringLiteral("openvpn"),
                                QStringLiteral("mtproxy"), QStringLiteral("sftp"), QStringLiteral("dns")}) {
        for (const QString &language : {QStringLiteral("fa"), QStringLiteral("en")}) {
            const QString body = controller.shareTemplate(kind, language);
            QVERIFY2(!body.isEmpty(), qPrintable(kind + QLatin1Char('/') + language));
            QVERIFY(body.contains(QStringLiteral("{{CONFIG}}")));
        }
    }
}

void ShareTemplatesTest::languageAndTemplateOverridesPersist()
{
    SecureQSettings settings(QStringLiteral("CoCoVpnTests"), QStringLiteral("ShareTemplates"), nullptr, false);
    settings.clearSettings();
    ExportController exporter(nullptr, nullptr);
    ExportUiController controller(&exporter, &settings);
    controller.setShareLanguage(QStringLiteral("en"));
    QCOMPARE(controller.shareLanguage(), QStringLiteral("en"));

    const QString custom = QStringLiteral("Custom {{SERVER}} {{CONFIG}}");
    controller.saveShareTemplate(QStringLiteral("wireguard"), QStringLiteral("en"), custom);
    QCOMPARE(controller.shareTemplate(QStringLiteral("wireguard"), QStringLiteral("en")), custom);
    controller.resetShareTemplate(QStringLiteral("wireguard"), QStringLiteral("en"));
    QVERIFY(controller.shareTemplate(QStringLiteral("wireguard"), QStringLiteral("en")) != custom);

    const QString personal = QStringLiteral("Hello {{NAME}} at {{SERVER}}\n{{CONFIG}}");
    const QString personalId = controller.saveCustomShareTemplate(QStringLiteral("wireguard"), QStringLiteral("en"),
        QString(), QStringLiteral("Personal guide"), personal);
    QVERIFY(!personalId.isEmpty());
    controller.setDefaultShareTemplate(QStringLiteral("wireguard"), QStringLiteral("en"), personalId);
    QCOMPARE(controller.defaultShareTemplateId(QStringLiteral("wireguard"), QStringLiteral("en")), personalId);
    QCOMPARE(controller.shareTemplate(QStringLiteral("wireguard"), QStringLiteral("en")), personal);
}

void ShareTemplatesTest::shareDocumentContainsEscapedConfigAndQr()
{
    SecureQSettings settings(QStringLiteral("CoCoVpnTests"), QStringLiteral("ShareTemplates"), nullptr, false);
    settings.clearSettings();
    ExportController exporter(nullptr, nullptr);
    ExportUiController controller(&exporter, &settings);
    const QString config = QStringLiteral("<secret>\nPrivateKey = sample");
    const QStringList qrCodes{QStringLiteral("data:image/svg+xml;base64,PHN2Zy8+")};

    const QString html = controller.renderShareDocument(QStringLiteral("wireguard"), QStringLiteral("en"),
        QStringLiteral("Test server"), QStringLiteral("Test user"), QStringLiteral("2026-09-29"), config, qrCodes, false, true);
    QVERIFY(html.startsWith(QStringLiteral("<!doctype html>")));
    QVERIFY(html.contains(QStringLiteral("CoCo VPN")));
    QVERIFY(html.contains(QStringLiteral("&lt;secret&gt;")));
    QVERIFY(html.contains(QStringLiteral("data:image/svg+xml;base64,PHN2Zy8+")));
    QVERIFY(html.contains(QStringLiteral("class=\"copy-config\"")));
    QVERIFY(html.contains(QStringLiteral("copyCocoConfig()")));
    QVERIFY(html.contains(QStringLiteral("font-family:Shabnam")));
    QVERIFY(html.contains(QStringLiteral("font-weight:700")));
    QVERIFY(!html.contains(QStringLiteral("<secret>")));

    const QString text = controller.renderShareDocument(QStringLiteral("wireguard"), QStringLiteral("en"),
        QStringLiteral("Test server"), QStringLiteral("Test user"), QStringLiteral("2026-09-29"), config, {}, false, false);
    QVERIFY(text.contains(config));
    QVERIFY(text.contains(QStringLiteral("QR code is included in the HTML version.")));
}

void ShareTemplatesTest::templatePreviewUsesDraftAndMarksSampleData()
{
    SecureQSettings settings(QStringLiteral("CoCoVpnTests"), QStringLiteral("ShareTemplates"), nullptr, false);
    settings.clearSettings();
    ExportController exporter(nullptr, nullptr);
    ExportUiController controller(&exporter, &settings);
    const QString draft = QStringLiteral("Guide for {{PROTOCOL}} at {{SERVER}}\n{{NAME}}\n{{CONFIG}}\n{{QR}}");

    const QString html = controller.renderShareTemplatePreview(QStringLiteral("wireguard"), QStringLiteral("en"), draft, true);
    QVERIFY(html.contains(QStringLiteral("Template preview — sample values only")));
    QVERIFY(html.contains(QStringLiteral("Example server")));
    QVERIFY(html.contains(QStringLiteral("Example account")));
    QVERIFY(html.contains(QStringLiteral("a real QR code is added")));
    QVERIFY(!html.contains(QStringLiteral("{{SERVER}}")));

    const QString text = controller.renderShareTemplatePreview(QStringLiteral("wireguard"), QStringLiteral("fa"), draft, false);
    QVERIFY(text.contains(QStringLiteral("پیش‌نمایش قالب")));
    QVERIFY(text.contains(QStringLiteral("نمونه سرور")));
    QVERIFY(text.contains(QStringLiteral("QR واقعی")));
    QVERIFY(!text.contains(QStringLiteral("{{CONFIG}}")));
}

void ShareTemplatesTest::oversizedAccessQrIsSkipped()
{
    const QByteArray oversizedConfig(850 * 65, 'x');
    QVERIFY(qrCodeUtils::generateQrCodeImageSeries(oversizedConfig).isEmpty());
}

void ShareTemplatesTest::savingGuideDoesNotReplaceConnectionConfig()
{
    SecureQSettings settings(QStringLiteral("CoCoVpnTests"), QStringLiteral("ShareTemplates"), nullptr, false);
    settings.clearSettings();
    ExportController exporter(nullptr, nullptr);
    ExportUiController controller(&exporter, &settings);

    const QString config = QStringLiteral("vpn://same-existing-access");
    QVERIFY(!controller.setConfigFromString(config, QString())); // Load the active config without writing it.

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString guide = QStringLiteral("<html>sharing guide</html>");
    const QString path = directory.filePath(QStringLiteral("guide.html"));
    QVERIFY(controller.saveRenderedShareDocument(path, guide));
    QCOMPARE(controller.getConfig(), config);

    QFile savedGuide(path);
    QVERIFY(savedGuide.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(savedGuide.readAll()), guide);
}

void ShareTemplatesTest::savedClientCredentialsAreExcludedFromBackups()
{
    SecureQSettings settings(QStringLiteral("CoCoVpnTests"), QStringLiteral("ShareTemplates"), nullptr, false);
    settings.clearSettings();
    settings.setValue(QStringLiteral("Sharing/clientConfigs/test"),
                      QVariantMap{{QStringLiteral("config"), QStringLiteral("PRIVATE-CONFIG-MARKER")}});

    const QByteArray backup = settings.backupAppConfig();
    QVERIFY(!backup.contains("PRIVATE-CONFIG-MARKER"));
}

QTEST_MAIN(ShareTemplatesTest)
#include "testShareTemplates.moc"
