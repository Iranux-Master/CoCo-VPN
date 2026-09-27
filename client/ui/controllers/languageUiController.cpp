#include "languageUiController.h"

LanguageUiController::LanguageUiController(SettingsController *settingsController, LanguageModel *languageModel, QObject *parent)
    : QObject(parent), m_settingsController(settingsController), m_languageModel(languageModel)
{
    const QLocale::Language current = m_settingsController->getAppLanguage().language();
    if (current != QLocale::English && current != QLocale::Persian) {
        m_settingsController->setAppLanguage(QLocale(QLocale::English));
    }
}

void LanguageUiController::onAppLanguageChanged(const QLocale &locale)
{
    emit updateTranslations(locale);
}

void LanguageUiController::changeLanguage(const LanguageSettings::AvailableLanguageEnum language)
{
    QLocale locale = languageEnumToLocale(language);
    m_settingsController->setAppLanguage(locale);
}

int LanguageUiController::getCurrentLanguageIndex() const
{
    auto locale = m_settingsController->getAppLanguage();
    switch (locale.language()) {
    case QLocale::English: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::English); break;
    case QLocale::Persian: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::Persian); break;
    default: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::English);
    }
}

bool LanguageUiController::isPersianUi() const
{
    return m_settingsController->getAppLanguage().language() == QLocale::Persian;
}

int LanguageUiController::getLineHeightAppend() const
{
    return 0;
}

QString LanguageUiController::getCurrentLanguageName() const
{
    int index = getCurrentLanguageIndex();
    return getLocalLanguageName(static_cast<LanguageSettings::AvailableLanguageEnum>(index));
}

LanguageSettings::AvailableLanguageEnum LanguageUiController::getSystemLanguageEnum() const
{
    return QLocale::system().language() == QLocale::Persian
        ? LanguageSettings::AvailableLanguageEnum::Persian
        : LanguageSettings::AvailableLanguageEnum::English;
}

QString LanguageUiController::getCurrentSiteUrl(const QString &path) const
{
    Q_UNUSED(path);
    return QStringLiteral("https://iranux.site/cocovpn");
}

QString LanguageUiController::getCurrentDocsUrl(const QString &path) const
{
    Q_UNUSED(path);
    return QStringLiteral("https://iranux.site/cocovpn");
}

QString LanguageUiController::getCurrentHostUrl(const QString &path) const
{
    Q_UNUSED(path);
    return QStringLiteral("https://iranux.site/vps");
}

QString LanguageUiController::getLocalLanguageName(const LanguageSettings::AvailableLanguageEnum language) const
{
    QString strLanguage("");
    switch (language) {
    case LanguageSettings::AvailableLanguageEnum::English: strLanguage = "English"; break;
    case LanguageSettings::AvailableLanguageEnum::Russian: strLanguage = "Русский"; break;
    case LanguageSettings::AvailableLanguageEnum::Ukrainian: strLanguage = "Українська"; break;
    case LanguageSettings::AvailableLanguageEnum::China_cn: strLanguage = "\347\256\200\344\275\223\344\270\255\346\226\207"; break;
    case LanguageSettings::AvailableLanguageEnum::Persian: strLanguage = "فارسی"; break;
    case LanguageSettings::AvailableLanguageEnum::Arabic: strLanguage = "العربية"; break;
    case LanguageSettings::AvailableLanguageEnum::Burmese: strLanguage = "မြန်မာဘာသာ"; break;
    case LanguageSettings::AvailableLanguageEnum::Urdu: strLanguage = "اُرْدُوْ"; break;
    case LanguageSettings::AvailableLanguageEnum::Hindi: strLanguage = "हिन्दी"; break;
    case LanguageSettings::AvailableLanguageEnum::Korean: strLanguage = "한국어"; break;
    case LanguageSettings::AvailableLanguageEnum::Spanish: strLanguage = "Español"; break;
    case LanguageSettings::AvailableLanguageEnum::French: strLanguage = "Français"; break;
    default: break;
    }

    return strLanguage;
}

QLocale LanguageUiController::languageEnumToLocale(const LanguageSettings::AvailableLanguageEnum language) const
{
    switch (language) {
    case LanguageSettings::AvailableLanguageEnum::English: return QLocale::English;
    case LanguageSettings::AvailableLanguageEnum::Persian: return QLocale::Persian;
    default: return QLocale::English;
    }
}
