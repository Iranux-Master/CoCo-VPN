#ifndef EXPORTUICONTROLLER_H
#define EXPORTUICONTROLLER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

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
    Q_PROPERTY(QVariantList accountGroups READ accountGroups NOTIFY accountGroupsChanged)
    Q_PROPERTY(QVariantList shareTemplates READ shareTemplates NOTIFY shareTemplatesChanged)
    Q_PROPERTY(int batchProgress READ batchProgress NOTIFY batchProgressChanged)
    Q_PROPERTY(int batchTotal READ batchTotal NOTIFY batchProgressChanged)
    Q_PROPERTY(bool batchRunning READ batchRunning NOTIFY batchProgressChanged)

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
    void setConfigFromString(const QString &config, const QString &fileName);

    void updateClientManagementModel(const QString &serverId, int containerIndex);

    void revokeConfig(int row, const QString &serverId, int containerIndex);

    void renameClient(int row, const QString &clientName, const QString &serverId, int containerIndex);

    QVariantList accountGroups() const;
    QVariantList shareTemplates() const;
    int batchProgress() const;
    int batchTotal() const;
    bool batchRunning() const;
    Q_INVOKABLE void startAccountBatch(const QString &serverId, const QString &serverName,
                                       const QString &baseName, int count, const QVariantList &containers);
    Q_INVOKABLE void saveShareTemplate(const QString &name, const QString &body);
    Q_INVOKABLE void deleteShareTemplate(const QString &id);
    Q_INVOKABLE void deleteAccountGroup(const QString &id);
    Q_INVOKABLE QString renderAccountTemplate(const QString &groupId, const QString &templateBody);
    Q_INVOKABLE QString renderAccountsTemplate(const QVariantList &groupIds, const QString &templateBody);
    Q_INVOKABLE QVariantMap accountGroup(const QString &id) const;

signals:
    void generateConfig(int type);
    void revokeConfigFinished();
    void exportErrorOccurred(const QString &errorMessage);
    void exportErrorOccurred(ErrorCode errorCode);

    void exportConfigChanged();

    void saveFile(const QString &fileName, const QString &data);
    void accountBatchFinished(int succeeded, int failed);
    void accountGroupsChanged();
    void shareTemplatesChanged();
    void batchProgressChanged();

private:
    int getQrCodesCount();
    void clearPreviousConfig();
    void applyExportResult(const ExportController::ExportResult &result);
    void createNextBatchAccount();
    void saveAccountGroups();
    void saveShareTemplates();
    void persistBatchGroup();
    QString renderAccountTemplateFragment(const QVariantMap &group, const QString &templateBody) const;

    ExportController* m_exportController;
    SecureQSettings* m_settings;
    QVariantList m_accountGroups;
    QVariantList m_shareTemplates;
    QVariantList m_batchContainers;
    QVariantMap m_batchGroup;
    QString m_batchServerId;
    QString m_batchServerName;
    QString m_batchBaseName;
    int m_batchCount = 0;
    int m_batchIndex = 0;
    int m_batchProtocolIndex = 0;
    int m_batchSucceeded = 0;
    int m_batchFailed = 0;
    bool m_batchRunning = false;

    QString m_config;
    QString m_nativeConfigString;
    QList<QString> m_qrCodes;
};

#endif // EXPORTUICONTROLLER_H
