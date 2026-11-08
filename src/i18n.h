#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class I18n : public QObject
{
    Q_OBJECT
public:
    static I18n &instance();

    void load(const QString &langCode);
    QString current() const { return m_current; }
    QStringList available() const;

    QString t(const QString &key) const;
    QString t(const QString &key, const QString &arg1) const;

    void ensureDefaults();

signals:
    void languageChanged(const QString &langCode);

private:
    I18n() = default;
    void writeIfMissing(const QString &fileName, const QByteArray &content);

    QHash<QString, QString> m_dict;
    QString m_current = "zh_CN";
    QString m_dir;
};
