#pragma once

#include <QObject>
#include <QTranslator>
#include <QMap>

class Translator : public QObject
{
    Q_OBJECT
public:
    static Translator &instance();

    QMap<QString, QString> availableLanguages() const;
    bool setLanguage(const QString &languageCode);
    QString currentLanguage() const;

signals:
    void languageChanged();

private:
    Translator();
    ~Translator();

    QTranslator m_appTranslator;
    QTranslator m_qtTranslator;
    QString m_currentLanguage;
};