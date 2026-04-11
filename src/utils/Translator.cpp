#include "Translator.h"
#include <QCoreApplication>
#include <QDir>
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
    langs["zh_CN"] = "简体中文";
    langs["ja_JP"] = "日本語";
    // 可从资源文件动态扫描更多
    return langs;
}

bool Translator::setLanguage(const QString &languageCode)
{
    // 移除旧的翻译
    QCoreApplication::removeTranslator(&m_appTranslator);
    QCoreApplication::removeTranslator(&m_qtTranslator);

    if (languageCode == "en_US")
    {
        // 英文无需翻译文件
        m_currentLanguage = languageCode;
        emit languageChanged();
        return true;
    }

    // 加载应用翻译文件
    if (m_appTranslator.load(":/translations/catch_editor_" + languageCode + ".qm"))
    {
        QCoreApplication::installTranslator(&m_appTranslator);
    }
    else
    {
        // 尝试从文件系统加载
        QString path = QCoreApplication::applicationDirPath() + "/translations/catch_editor_" + languageCode + ".qm";
        if (m_appTranslator.load(path))
        {
            QCoreApplication::installTranslator(&m_appTranslator);
        }
        else
        {
            return false;
        }
    }

    // 加载 Qt 内置翻译
    if (m_qtTranslator.load(QLibraryInfo::path(QLibraryInfo::TranslationsPath) + "/qt_" + languageCode + ".qm"))
    {
        QCoreApplication::installTranslator(&m_qtTranslator);
    }

    m_currentLanguage = languageCode;
    emit languageChanged();
    return true;
}

QString Translator::currentLanguage() const
{
    return m_currentLanguage;
}