#ifndef EXPORTUICONTROLLER_H
#define EXPORTUICONTROLLER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>

#include "core/controllers/selfhosted/exportController.h"
#include "core/utils/errorCodes.h"
#include "secureQSettings.h"

class ExportUiController : public QObject
{
    Q_OBJECT
public:
    explicit ExportUiController(ExportController* exportController, SecureQSettings* settings, QObject *parent = nullptr);

    Q_PROPERTY(QList<QString> qrCodes READ getQrCodes NOTIFY exportConfigChanged)
    Q_PROPERTY(int qrCodesCount READ getQrCodesCount NOTIFY exportConfigChanged)
    Q_PROPERTY(QString config READ getConfig NOTIFY exportConfigChanged)
    Q_PROPERTY(QString nativeConfigString READ getNativeConfigString NOTIFY exportConfigChanged)
    Q_PROPERTY(QString shareLanguage READ shareLanguage NOTIFY shareLanguageChanged)
    Q_PROPERTY(QString shareKind READ shareKind NOTIFY sharingContextChanged)
    Q_PROPERTY(QString shareServer READ shareServer NOTIFY sharingContextChanged)
    Q_PROPERTY(QString shareAccount READ shareAccount NOTIFY sharingContextChanged)
    Q_PROPERTY(QString shareCreatedAt READ shareCreatedAt NOTIFY sharingContextChanged)
    Q_PROPERTY(bool shareNativeFormat READ shareNativeFormat NOTIFY sharingContextChanged)
    Q_PROPERTY(QVariantList shareTemplateKinds READ shareTemplateKinds CONSTANT)

public slots:
    void generateFullAccessConfig(const QString &serverId);

    void generateConnectionConfig(const QString &serverId, int containerIndex, const QString &clientName);
    void generateOpenVpnConfig(const QString &serverId, const QString &clientName);
    void generateWireGuardConfig(const QString &serverId, const QString &clientName);
    void generateAwgConfig(const QString &serverId, int containerIndex, const QString &clientName);
    void generateXrayConfig(const QString &serverId, const QString &clientName);
    void generateQrFromString(const QString &text);
    void generateQrFromStringRaw(const QString &text);

    QString getConfig();
    QString getNativeConfigString();
    QList<QString> getQrCodes();

    void exportConfig(const QString &fileName);
    bool setConfigFromString(const QString &config, const QString &fileName);

    void updateClientManagementModel(const QString &serverId, int containerIndex);

    void revokeConfig(int row, const QString &serverId, int containerIndex);

    void renameClient(int row, const QString &clientName, const QString &serverId, int containerIndex);

    QString shareLanguage() const;
    QString shareKind() const;
    QString shareServer() const;
    QString shareAccount() const;
    QString shareCreatedAt() const;
    bool shareNativeFormat() const;
    QVariantList shareTemplateKinds() const;
    Q_INVOKABLE void setShareLanguage(const QString &language);
    Q_INVOKABLE QString shareTemplate(const QString &kind, const QString &language) const;
    Q_INVOKABLE QVariantList shareTemplates(const QString &kind, const QString &language) const;
    Q_INVOKABLE QString defaultShareTemplateId(const QString &kind, const QString &language) const;
    Q_INVOKABLE void setDefaultShareTemplate(const QString &kind, const QString &language, const QString &templateId);
    Q_INVOKABLE void saveShareTemplate(const QString &kind, const QString &language, const QString &body);
    Q_INVOKABLE QString saveCustomShareTemplate(const QString &kind, const QString &language,
                                                const QString &templateId, const QString &name,
                                                const QString &body);
    Q_INVOKABLE void resetShareTemplate(const QString &kind, const QString &language);
    Q_INVOKABLE QString renderShareDocument(const QString &kind, const QString &language,
                                            const QString &serverName, const QString &accountName,
                                            const QString &createdAt, const QString &config,
                                            const QStringList &qrCodes, bool nativeFormat, bool html) const;
    Q_INVOKABLE void setSharingContext(const QString &kind, const QString &serverName,
                                       const QString &accountName, const QString &createdAt,
                                       bool nativeFormat = false);

signals:
    void generateConfig(int type);
    void revokeConfigFinished();
    void exportErrorOccurred(const QString &errorMessage);
    void exportErrorOccurred(ErrorCode errorCode);

    void exportConfigChanged();

    void saveFile(const QString &fileName, const QString &data);
    void shareLanguageChanged();
    void shareTemplateChanged();
    void sharingContextChanged();

private:
    int getQrCodesCount();
    void clearPreviousConfig();
    void applyExportResult(const ExportController::ExportResult &result);
    QString defaultShareTemplate(const QString &kind, const QString &language) const;
    QString htmlEscape(const QString &value) const;

    ExportController* m_exportController;
    SecureQSettings* m_settings;
    QString m_shareLanguage;
    QString m_shareKind = QStringLiteral("wireguard");
    QString m_shareServer;
    QString m_shareAccount;
    QString m_shareCreatedAt;
    bool m_shareNativeFormat = false;

    QString m_config;
    QString m_nativeConfigString;
    QList<QString> m_qrCodes;
};

#endif // EXPORTUICONTROLLER_H
