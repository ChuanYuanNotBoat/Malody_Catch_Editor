#include "Translator.h"
#include <QCoreApplication>
#include <QLibraryInfo>

Translator::Translator() : QObject(nullptr) {}

Translator::~Translator()
{
    QCoreApplication::removeTranslator(&m_appTranslator);
    QCoreApplication::removeTranslator(&m_qtTranslator);
}

Translator &Translator::instance()
{
    static Translator inst;
    return inst;
}

QMap<QString, QString> Translator::availableLanguages() const
{
    QMap<QString, QString> langs;
    langs["en_US"] = "English (US)";
    langs["zh_CN"] = QString::fromUtf8(u8"简体中文");
    langs["ja_JP"] = QString::fromUtf8(u8"日本語");
    return langs;
}

bool Translator::setLanguage(const QString &languageCode)
{
    QCoreApplication::removeTranslator(&m_appTranslator);
    QCoreApplication::removeTranslator(&m_qtTranslator);

    if (languageCode == "en_US")
    {
        m_currentLanguage = languageCode;
        emit languageChanged();
        return true;
    }

    if (m_appTranslator.load(":/translations/catch_editor_" + languageCode + ".qm"))
    {
        QCoreApplication::installTranslator(&m_appTranslator);
    }
    else
    {
        const QString path = QCoreApplication::applicationDirPath() + "/translations/catch_editor_" + languageCode + ".qm";
        if (m_appTranslator.load(path))
        {
            QCoreApplication::installTranslator(&m_appTranslator);
        }
        else
        {
            return false;
        }
    }

    if (m_qtTranslator.load(QLibraryInfo::path(QLibraryInfo::TranslationsPath) + "/qt_" + languageCode + ".qm"))
        QCoreApplication::installTranslator(&m_qtTranslator);

    m_currentLanguage = languageCode;
    emit languageChanged();
    return true;
}

QString Translator::currentLanguage() const
{
    return m_currentLanguage;
}
