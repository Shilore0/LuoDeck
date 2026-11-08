#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>

class Theme : public QObject
{
    Q_OBJECT
public:
    static Theme &instance();

    void load(const QString &name);
    void ensureDefaults();

    QString current() const { return m_name; }
    QStringList available() const { return m_available; }

    int radius() const { return m_radius; }
    int spacing() const { return m_spacing; }
    int iconSize() const { return m_iconSize; }
    int columns() const { return m_columns; }

    QColor color(const QString &key) const;
    QString styleSheet() const;

signals:
    void themeChanged();

private:
    Theme() = default;
    bool readFile(const QString &path);
    void writeIfMissing(const QString &fileName, const QByteArray &content);
    void refreshAvailable();

    QString m_name = "default";
    QString m_dir;
    QStringList m_available;

    int m_radius = 12;
    int m_spacing = 8;
    int m_iconSize = 48;
    int m_columns = 6;

    QColor m_window{0xF3, 0xF3, 0xF3, 0xF0};
    QColor m_surface{0xFF, 0xFF, 0xFF, 0xFF};
    QColor m_surfaceHover{0xE8, 0xEA, 0xED};
    QColor m_text{0x20, 0x21, 0x24};
    QColor m_textMuted{0x5F, 0x63, 0x68};
    QColor m_accent{0x1A, 0x73, 0xE8};
    QColor m_border{0xDA, 0xDC, 0xE0};
};
