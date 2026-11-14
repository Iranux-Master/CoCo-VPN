#include "webpackSudoSetup.h"

#include <QByteArray>
#include <QElapsedTimer>

#include <libssh/libssh.h>

#include "core/utils/selfhosted/sshHostKey.h"
#include "webpackPurchases.h"

namespace webpack
{
    namespace
    {
        struct Run
        {
            bool started = false;
            int exitStatus = -1;
            QByteArray out;
            QByteArray err;
        };

        Run exec(ssh_session session, const QString &command, const QByteArray &stdinData, int timeoutSecs)
        {
            Run run;
            ssh_channel channel = ssh_channel_new(session);
            if (!channel) {
                return run;
            }
            if (ssh_channel_open_session(channel) != SSH_OK) {
                ssh_channel_free(channel);
                return run;
            }
            const QByteArray commandBytes = command.toUtf8();
            if (ssh_channel_request_exec(channel, commandBytes.constData()) != SSH_OK) {
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                return run;
            }
            run.started = true;
            if (!stdinData.isEmpty()) {
                ssh_channel_write(channel, stdinData.constData(), uint32_t(stdinData.size()));
            }
            ssh_channel_send_eof(channel);

            QElapsedTimer timer;
            timer.start();
            char buffer[4096];
            while (!ssh_channel_is_eof(channel) && timer.elapsed() < qint64(timeoutSecs) * 1000) {
                const int n = ssh_channel_read_timeout(channel, buffer, sizeof(buffer), 0, 500);
                if (n > 0) {
                    run.out.append(buffer, n);
                }
                const int e = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 1);
                if (e > 0) {
                    run.err.append(buffer, e);
                }
                if (n == SSH_ERROR || e == SSH_ERROR) {
                    break;
                }
            }
            int e = 0;
            while ((e = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 1)) > 0) {
                run.err.append(buffer, e);
            }
            run.exitStatus = ssh_channel_get_exit_status(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return run;
        }
    } // namespace

    QString SudoSetupResult::code() const
    {
        switch (outcome) {
        case Outcome::Ready: return QStringLiteral("ready");
        case Outcome::AlreadyReady: return QStringLiteral("already_ready");
        case Outcome::HostKeyMismatch: return QStringLiteral("host_key_mismatch");
        case Outcome::Unreachable: return QStringLiteral("unreachable");
        case Outcome::AuthFailed: return QStringLiteral("auth_failed");
        case Outcome::WrongPassword: return QStringLiteral("wrong_password");
        case Outcome::ScriptFailed: return QStringLiteral("script_failed");
        }
        return QStringLiteral("unreachable");
    }

    QString sudoBootstrapCommand(const QString &script)
    {
        const QByteArray encoded = script.toUtf8().toBase64();
        return QStringLiteral("sudo -k; sudo -S -p '' sh -c \"$(printf %s %1 | base64 -d)\"").arg(QString::fromLatin1(encoded));
    }

    SudoSetupResult setUpPasswordlessSudo(const amnezia::ServerCredentials &credentials, const QString &password,
                                          const QString &scriptTemplate, int timeoutSecs)
    {
        SudoSetupResult result;
        if (!isValidLoginUsername(credentials.userName)) {
            result.outcome = SudoSetupResult::Outcome::AuthFailed;
            return result;
        }

        ssh_session session = ssh_new();
        if (!session) {
            return result;
        }
        int port = credentials.port > 0 ? credentials.port : 22;
        long timeout = 20;
        int verbosity = SSH_LOG_NOLOG;
        const QByteArray host = credentials.hostName.toUtf8();
        const QByteArray user = credentials.userName.toUtf8();
        ssh_options_set(session, SSH_OPTIONS_HOST, host.constData());
        ssh_options_set(session, SSH_OPTIONS_PORT, &port);
        ssh_options_set(session, SSH_OPTIONS_USER, user.constData());
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);
        ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);

        auto finish = [&](SudoSetupResult::Outcome outcome) {
            if (ssh_is_connected(session)) {
                ssh_disconnect(session);
            }
            ssh_free(session);
            result.outcome = outcome;
            return result;
        };

        if (ssh_connect(session) != SSH_OK) {
            return finish(SudoSetupResult::Outcome::Unreachable);
        }
        // Never talk to a server webpack did not confirm: the pin is required here.
        if (sshHostKey::normalizeFingerprint(credentials.hostKeySha256).isEmpty()
            || !sshHostKey::matchesPin(session, credentials.hostKeySha256)) {
            return finish(SudoSetupResult::Outcome::HostKeyMismatch);
        }

        ssh_key key = nullptr;
        const QByteArray pem = credentials.secretData.toLatin1();
        if (ssh_pki_import_privkey_base64(pem.constData(), nullptr, nullptr, nullptr, &key) != SSH_OK || !key) {
            return finish(SudoSetupResult::Outcome::AuthFailed);
        }
        const int auth = ssh_userauth_publickey(session, nullptr, key);
        ssh_key_free(key);
        if (auth != SSH_AUTH_SUCCESS) {
            return finish(SudoSetupResult::Outcome::AuthFailed);
        }

        const Run check = exec(session, QStringLiteral("sudo -n true"), {}, timeoutSecs);
        if (check.started && check.exitStatus == 0) {
            return finish(SudoSetupResult::Outcome::AlreadyReady);
        }
        if (password.isEmpty()) {
            return finish(SudoSetupResult::Outcome::WrongPassword);
        }

        QString script = scriptTemplate;
        script.replace(QStringLiteral("$LOGIN_USER"), credentials.userName);
        const Run setup = exec(session, sudoBootstrapCommand(script), password.toUtf8() + '\n', timeoutSecs);
        if (!setup.started) {
            return finish(SudoSetupResult::Outcome::Unreachable);
        }
        if (!setup.out.contains("COCO_SUDO_OK")) {
            const bool refused = setup.err.contains("incorrect password") || setup.err.contains("Sorry, try again")
                    || setup.err.contains("no password was provided") || (setup.exitStatus == 1 && setup.out.isEmpty());
            return finish(refused ? SudoSetupResult::Outcome::WrongPassword : SudoSetupResult::Outcome::ScriptFailed);
        }

        const Run verify = exec(session, QStringLiteral("sudo -k; sudo -n true"), {}, timeoutSecs);
        return finish(verify.started && verify.exitStatus == 0 ? SudoSetupResult::Outcome::Ready : SudoSetupResult::Outcome::ScriptFailed);
    }
} // namespace webpack
