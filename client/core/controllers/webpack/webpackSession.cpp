#include "webpackSession.h"

#include <QJsonDocument>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>

#include "webpackLoopback.h"
#include "webpackStore.h"

namespace webpack
{
    namespace
    {
        constexpr int accessTokenMarginSecs = 60;
        constexpr qint64 googleWaitCapMs = 10 * 60 * 1000;
        const QString basePath = QStringLiteral("api/v1/auth/device-sessions");

        ApiResult localFailure(const QString &code)
        {
            ApiResult result;
            result.outcome = ApiOutcome::Rejected;
            result.errorCode = code;
            return result;
        }
    } // namespace

    Session::Session(ApiClient *api, Store *store, QObject *parent)
        : QObject(parent), m_api(api), m_store(store), m_deviceName(defaultDeviceName())
    {
        loadStored();
    }

    QString Session::defaultDeviceName()
    {
        QString name = QStringLiteral("CoCo VPN · %1 · %2").arg(QSysInfo::prettyProductName(), QSysInfo::machineHostName());
        return name.left(100);
    }

    QString Session::accessToken() const
    {
        if (m_accessToken.isEmpty() || !m_accessExpiresAt.isValid()) {
            return {};
        }
        if (m_accessExpiresAt.addSecs(-accessTokenMarginSecs) <= QDateTime::currentDateTimeUtc()) {
            return {};
        }
        return m_accessToken;
    }

    void Session::signInWithPassword(const QString &email, const QString &password, const Done &done)
    {
        QJsonObject body { { "email", email.trimmed() }, { "password", password }, { "deviceName", m_deviceName } };
        QPointer<Session> self(this);
        m_api->post(basePath, body, [self, done](const ApiResult &result) {
            if (!self) {
                return;
            }
            if (!result.isOk()) {
                if (done) {
                    done(result);
                }
                return;
            }
            self->applyTokens(result.object());
            self->reloadProfile([done, result](const ApiResult &) {
                if (done) {
                    done(result);
                }
            });
        }, { false, 30000 });
    }

    void Session::signInWithGoogle(const Done &done)
    {
        cancelGoogleSignIn();

        const Pkce pkce = Pkce::create();
        auto *loopback = new LoopbackReceiver(this);
        if (!loopback->start()) {
            loopback->deleteLater();
            if (done) {
                done(localFailure(QStringLiteral("loopback_unavailable")));
            }
            return;
        }

        m_loopback = loopback;
        m_googleVerifier = pkce.verifier;
        m_googleDone = done;
        const int generation = ++m_googleGeneration;

        QJsonObject body { { "deviceName", m_deviceName }, { "codeChallenge", pkce.challenge }, { "redirectPort", int(loopback->port()) } };
        QPointer<Session> self(this);
        m_api->post(basePath + QStringLiteral("/google"), body, [self, generation](const ApiResult &result) {
            if (!self || generation != self->m_googleGeneration) {
                return;
            }
            if (!result.isOk()) {
                self->finishGoogle(result);
                return;
            }

            const QJsonObject data = result.object();
            self->m_googleRequestId = data.value("requestId").toString();
            const QUrl signInUrl(data.value("signInUrl").toString());
            const QDateTime expires = QDateTime::fromString(data.value("expiresAtUtc").toString(), Qt::ISODateWithMs);

            if (self->m_googleRequestId.isEmpty() || signInUrl.scheme() != QLatin1String("https")) {
                self->finishGoogle(localFailure(QStringLiteral("bad_response")));
                return;
            }

            qint64 waitMs = googleWaitCapMs;
            if (expires.isValid()) {
                waitMs = qBound<qint64>(1000, QDateTime::currentDateTimeUtc().msecsTo(expires), googleWaitCapMs);
            }
            QTimer::singleShot(waitMs, self, [self, generation]() {
                if (self && generation == self->m_googleGeneration && self->m_googleDone) {
                    self->finishGoogle(localFailure(QStringLiteral("google_timed_out")));
                }
            });

            connect(self->m_loopback, &LoopbackReceiver::returned, self, [self, generation](const QMap<QString, QString> &query) {
                if (!self || generation != self->m_googleGeneration) {
                    return;
                }
                // Only the return of the request this app started counts; any other hit is ignored.
                if (query.value(QStringLiteral("request")) != self->m_googleRequestId) {
                    return;
                }
                const QString code = query.value(QStringLiteral("code"));
                if (code.isEmpty()) {
                    self->finishGoogle(localFailure(QStringLiteral("google_failed")));
                    return;
                }
                if (self->m_loopback) {
                    self->m_loopback->stop();
                }
                QJsonObject exchange { { "requestId", self->m_googleRequestId }, { "code", code }, { "codeVerifier", self->m_googleVerifier } };
                self->m_api->post(basePath + QStringLiteral("/google/exchange"), exchange, [self, generation](const ApiResult &exchanged) {
                    if (!self || generation != self->m_googleGeneration) {
                        return;
                    }
                    if (!exchanged.isOk()) {
                        self->finishGoogle(exchanged);
                        return;
                    }
                    self->applyTokens(exchanged.object());
                    self->reloadProfile([self, generation, exchanged](const ApiResult &) {
                        if (self && generation == self->m_googleGeneration) {
                            self->finishGoogle(exchanged);
                        }
                    });
                }, { false, 30000 });
            });

            const bool opened = self->m_openUrl && self->m_openUrl(signInUrl);
            if (!opened) {
                self->finishGoogle(localFailure(QStringLiteral("browser_unavailable")));
            }
        }, { false, 30000 });
    }

    void Session::finishGoogle(const ApiResult &result)
    {
        Done done = m_googleDone;
        m_googleDone = nullptr;
        m_googleVerifier.clear();
        m_googleRequestId.clear();
        ++m_googleGeneration;
        if (m_loopback) {
            m_loopback->stop();
            m_loopback->deleteLater();
        }
        if (done) {
            done(result);
        }
    }

    void Session::cancelGoogleSignIn()
    {
        if (m_googleDone || m_loopback) {
            finishGoogle(localFailure(QStringLiteral("cancelled")));
        }
    }

    void Session::signOut(const std::function<void()> &done)
    {
        const QString sessionId = m_sessionId;
        if (sessionId.isEmpty() || accessToken().isEmpty()) {
            // Without a fresh token, still end locally; the site ends the session at its next refresh attempt
            // or when the person removes the device on the website.
            endLocally();
            if (done) {
                done();
            }
            return;
        }
        QPointer<Session> self(this);
        m_api->deleteResource(basePath + QLatin1Char('/') + QString::fromLatin1(QUrl::toPercentEncoding(sessionId)),
                              [self, done](const ApiResult &) {
                                  if (self) {
                                      self->endLocally();
                                  }
                                  if (done) {
                                      done();
                                  }
                              });
    }

    void Session::reloadProfile(const Done &done)
    {
        QPointer<Session> self(this);
        m_api->get(basePath + QStringLiteral("/current"), [self, done](const ApiResult &result) {
            if (self && result.isOk()) {
                const QJsonObject data = result.object();
                self->m_profile.email = data.value("email").toString();
                self->m_profile.displayName = data.value("displayName").toString();
                self->m_profile.walletCurrency = data.value("walletCurrency").toString();
                self->saveStored();
                emit self->profileChanged();
            }
            if (done) {
                done(result);
            }
        });
    }

    void Session::refreshAccessToken(const std::function<void(bool ok)> &done)
    {
        if (m_refreshToken.isEmpty()) {
            if (done) {
                done(false);
            }
            return;
        }
        if (done) {
            m_refreshWaiters.append(done);
        }
        if (m_refreshing) {
            return;
        }
        m_refreshing = true;

        QPointer<Session> self(this);
        // Once sent, the site may already have rotated the token: the answer is always applied.
        m_api->post(basePath + QStringLiteral("/refresh"), QJsonObject { { "refreshToken", m_refreshToken } },
                    [self](const ApiResult &result) {
                        if (!self) {
                            return;
                        }
                        bool ok = false;
                        if (result.isOk()) {
                            self->applyTokens(result.object());
                            ok = !self->m_accessToken.isEmpty();
                        } else if (result.outcome == ApiOutcome::SignedOut || result.errorCode == QLatin1String("invalid_token")) {
                            // The site refused this refresh token (expired, revoked, password changed, replayed).
                            self->endLocally();
                        } else {
                            // Unreachable or limited for a moment: keep the sign-in, say offline.
                            self->m_accessToken.clear();
                            self->setState(State::Offline);
                        }
                        self->m_refreshing = false;
                        const auto waiters = self->m_refreshWaiters;
                        self->m_refreshWaiters.clear();
                        for (const auto &waiter : waiters) {
                            waiter(ok);
                        }
                    },
                    { false, 30000 });
    }

    void Session::applyTokens(const QJsonObject &tokens)
    {
        m_sessionId = tokens.value("sessionId").toString(m_sessionId);
        m_accessToken = tokens.value("accessToken").toString();
        m_accessExpiresAt = QDateTime::fromString(tokens.value("accessTokenExpiresAtUtc").toString(), Qt::ISODateWithMs);
        m_refreshToken = tokens.value("refreshToken").toString();
        m_refreshExpiresAt = QDateTime::fromString(tokens.value("refreshTokenExpiresAtUtc").toString(), Qt::ISODateWithMs);
        if (m_refreshToken.isEmpty()) {
            endLocally();
            return;
        }
        saveStored();
        setState(State::SignedIn);
    }

    void Session::loadStored()
    {
        const QJsonObject stored = QJsonDocument::fromJson(m_store->value(keys::session).toByteArray()).object();
        m_refreshToken = stored.value("refreshToken").toString();
        if (m_refreshToken.isEmpty()) {
            return;
        }
        m_sessionId = stored.value("sessionId").toString();
        m_refreshExpiresAt = QDateTime::fromString(stored.value("refreshTokenExpiresAtUtc").toString(), Qt::ISODateWithMs);
        m_profile.email = stored.value("email").toString();
        m_profile.displayName = stored.value("displayName").toString();
        m_profile.walletCurrency = stored.value("walletCurrency").toString();
        if (m_refreshExpiresAt.isValid() && m_refreshExpiresAt <= QDateTime::currentDateTimeUtc()) {
            endLocally();
            return;
        }
        // Signed in until the first refresh says otherwise.
        m_state = State::SignedIn;
    }

    void Session::saveStored()
    {
        if (m_refreshToken.isEmpty()) {
            return;
        }
        const QJsonObject stored { { "sessionId", m_sessionId },
                                   { "refreshToken", m_refreshToken },
                                   { "refreshTokenExpiresAtUtc", m_refreshExpiresAt.toString(Qt::ISODateWithMs) },
                                   { "email", m_profile.email },
                                   { "displayName", m_profile.displayName },
                                   { "walletCurrency", m_profile.walletCurrency } };
        m_store->setValue(keys::session, QJsonDocument(stored).toJson(QJsonDocument::Compact));
    }

    void Session::endLocally()
    {
        m_sessionId.clear();
        m_refreshToken.clear();
        m_accessToken.clear();
        m_accessExpiresAt = {};
        m_refreshExpiresAt = {};
        m_profile = {};
        m_store->remove(keys::session);
        emit profileChanged();
        setState(State::SignedOut);
    }

    void Session::setState(State state)
    {
        if (m_state == state) {
            return;
        }
        m_state = state;
        emit stateChanged();
    }
} // namespace webpack
