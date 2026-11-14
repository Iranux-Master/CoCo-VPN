#ifndef WEBPACKSESSION_H
#define WEBPACKSESSION_H

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>

#include "webpackApiClient.h"

namespace webpack
{
    class LoopbackReceiver;
    class Store;

    struct AccountProfile
    {
        QString email;
        QString displayName;
        QString walletCurrency;
    };

    // The webpack.ir device session (API_REFERENCE section 3.4b): password or Google sign-in, the
    // rotating refresh token, sign-out. Keeps only the refresh token and the profile (in the Store,
    // encrypted); the access token lives in memory. Refreshes one at a time: a replayed refresh token
    // ends the session on the site.
    class Session : public QObject, public TokenSource
    {
        Q_OBJECT

    public:
        enum class State { SignedOut, SignedIn, Offline };
        Q_ENUM(State)

        using Done = std::function<void(const ApiResult &result)>;
        using UrlOpener = std::function<bool(const QUrl &url)>;

        Session(ApiClient *api, Store *store, QObject *parent = nullptr);

        State state() const { return m_state; }
        bool isSignedIn() const { return m_state != State::SignedOut; }
        AccountProfile profile() const { return m_profile; }

        void setDeviceName(const QString &name) { m_deviceName = name; }
        QString deviceName() const { return m_deviceName; }
        void setUrlOpener(const UrlOpener &opener) { m_openUrl = opener; }

        void signInWithPassword(const QString &email, const QString &password, const Done &done);
        // Opens the browser; done runs when the exchange finished, failed, or after ten minutes.
        void signInWithGoogle(const Done &done);
        void cancelGoogleSignIn();
        void signOut(const std::function<void()> &done = {});
        // Reads the profile again (GET /auth/device-sessions/current).
        void reloadProfile(const Done &done = {});

        // TokenSource
        QString accessToken() const override;
        void refreshAccessToken(const std::function<void(bool ok)> &done) override;

        static QString defaultDeviceName();

    signals:
        void stateChanged();
        void profileChanged();

    private:
        void applyTokens(const QJsonObject &tokens);
        void loadStored();
        void saveStored();
        void endLocally();
        void setState(State state);
        void finishGoogle(const ApiResult &result);

        ApiClient *m_api;
        Store *m_store;
        State m_state = State::SignedOut;
        QString m_sessionId;
        QString m_refreshToken;
        QDateTime m_refreshExpiresAt;
        QString m_accessToken;
        QDateTime m_accessExpiresAt;
        AccountProfile m_profile;
        QString m_deviceName;
        UrlOpener m_openUrl;

        bool m_refreshing = false;
        QList<std::function<void(bool)>> m_refreshWaiters;

        QPointer<LoopbackReceiver> m_loopback;
        QString m_googleRequestId;
        QString m_googleVerifier;
        Done m_googleDone;
        int m_googleGeneration = 0;
    };
} // namespace webpack

#endif // WEBPACKSESSION_H
