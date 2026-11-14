#ifndef WEBPACK_SECURESETTINGSSTORE_H
#define WEBPACK_SECURESETTINGSSTORE_H

#include "secureQSettings.h"
#include "webpackStore.h"

namespace webpack
{
    class SecureSettingsStore : public Store
    {
    public:
        explicit SecureSettingsStore(SecureQSettings *settings) : m_settings(settings) {}

        QVariant value(const QString &key, const QVariant &defaultValue = {}) const override
        {
            return m_settings->value(key, defaultValue);
        }
        void setValue(const QString &key, const QVariant &value) override { m_settings->setValue(key, value); }
        void remove(const QString &key) override { m_settings->remove(key); }

    private:
        SecureQSettings *m_settings;
    };
} // namespace webpack

#endif // WEBPACK_SECURESETTINGSSTORE_H
