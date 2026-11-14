#ifndef SSHHOSTKEY_H
#define SSHHOSTKEY_H

#include <QByteArray>
#include <QString>

#include <libssh/libssh.h>

namespace sshHostKey
{
    // "SHA256:abc=" or "abc" -> "abc": the form OpenSSH prints without prefix and padding.
    QString normalizeFingerprint(const QString &value);

    // OpenSSH-style SHA-256 fingerprint (base64 without padding, no prefix) of a public key blob.
    QString fingerprintOfBlob(const QByteArray &publicKeyBlob);

    // The fingerprint of the host key a connected session presents, or empty on failure.
    QString serverFingerprint(ssh_session session);

    // True when expected is empty (no pin) or equals the presented host key.
    bool matchesPin(ssh_session session, const QString &expected);
} // namespace sshHostKey

#endif // SSHHOSTKEY_H
