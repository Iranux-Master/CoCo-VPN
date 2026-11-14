#include "sshHostKey.h"

#include <QCryptographicHash>

namespace sshHostKey
{
    QString normalizeFingerprint(const QString &value)
    {
        QString trimmed = value.trimmed();
        if (trimmed.startsWith(QLatin1String("SHA256:"))) {
            trimmed = trimmed.mid(7);
        }
        while (trimmed.endsWith(QLatin1Char('='))) {
            trimmed.chop(1);
        }
        return trimmed;
    }

    QString fingerprintOfBlob(const QByteArray &publicKeyBlob)
    {
        return normalizeFingerprint(QString::fromLatin1(QCryptographicHash::hash(publicKeyBlob, QCryptographicHash::Sha256).toBase64()));
    }

    QString serverFingerprint(ssh_session session)
    {
        ssh_key key = nullptr;
        if (ssh_get_server_publickey(session, &key) != SSH_OK || key == nullptr) {
            return {};
        }
        unsigned char *hash = nullptr;
        size_t length = 0;
        const int rc = ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &length);
        ssh_key_free(key);
        if (rc != SSH_OK || hash == nullptr) {
            return {};
        }
        const QByteArray digest(reinterpret_cast<const char *>(hash), int(length));
        ssh_clean_pubkey_hash(&hash);
        return normalizeFingerprint(QString::fromLatin1(digest.toBase64()));
    }

    bool matchesPin(ssh_session session, const QString &expected)
    {
        const QString pin = normalizeFingerprint(expected);
        if (pin.isEmpty()) {
            return true;
        }
        const QString presented = serverFingerprint(session);
        return !presented.isEmpty() && presented == pin;
    }
} // namespace sshHostKey
