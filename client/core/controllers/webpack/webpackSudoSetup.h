#ifndef WEBPACKSUDOSETUP_H
#define WEBPACKSUDOSETUP_H

#include <QString>

#include "core/utils/commonStructs.h"

namespace webpack
{
    // Makes the login of a webpack server use sudo without a password, which CoCo's installer
    // needs. Blocking: call it from a worker thread (QtConcurrent). It connects with the device key
    // to the pinned host key, returns at once when "sudo -n true" already works, and otherwise runs
    // webpack_grant_sudo.sh under "sudo -S" with the login password on stdin.
    struct SudoSetupResult
    {
        enum class Outcome { Ready, AlreadyReady, HostKeyMismatch, Unreachable, AuthFailed, WrongPassword, ScriptFailed };
        Outcome outcome = Outcome::Unreachable;
        bool ok() const { return outcome == Outcome::Ready || outcome == Outcome::AlreadyReady; }
        QString code() const;
    };

    // scriptTemplate is webpack_grant_sudo.sh; credentials.secretData is the private key.
    SudoSetupResult setUpPasswordlessSudo(const amnezia::ServerCredentials &credentials, const QString &password,
                                          const QString &scriptTemplate, int timeoutSecs = 60);

    // Pure: the remote command line for the script (base64 in, so no quoting can break it).
    QString sudoBootstrapCommand(const QString &script);
} // namespace webpack

#endif // WEBPACKSUDOSETUP_H
