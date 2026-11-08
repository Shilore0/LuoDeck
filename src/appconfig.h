#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct TileEntry {
    QString id;
    QString path;
    QString name;
    bool isFolder = false;
    bool expanded = false;
};

class AppConfig : public QObject
{
    Q_OBJECT
public:
    static AppConfig &instance();

    bool load();
    bool save();

    QString language() const { return m_language; }
    QString theme() const { return m_theme; }
    QString hotkey() const { return m_hotkey; }
    int iconSize() const { return m_iconSize; }
    int columns() const { return m_columns; }

    QVector<TileEntry> &items() { return m_items; }
    const QVector<TileEntry> &items() const { return m_items; }

    void setLanguage(const QString &v);
    void setTheme(const QString &v);
    void setHotkey(const QString &v);

    QString dataDir() const { return m_dataDir; }
    QString configPath() const { return m_configPath; }
    QString themesDir() const { return m_dataDir + "/themes"; }
    QString localesDir() const { return m_dataDir + "/i18n"; }
    QString cacheDir() const { return m_dataDir + "/cache"; }

    void addPath(const QString &path);
    void removeById(const QString &id);
    void moveItem(int from, int to);

    static QString makeId();
    static QString displayNameFor(const QString &path);
    static bool isFolderPath(const QString &path);

signals:
    void changed();

private:
    AppConfig() = default;

    void applyDefaults();
    void importFromDesktop();
    void ensureDirectories();

    QString m_dataDir;
    QString m_configPath;
    QString m_language = "zh_CN";
    QString m_theme = "default";
    QString m_hotkey = "ctrl+space";
    int m_iconSize = 48;
    int m_columns = 6;
    QVector<TileEntry> m_items;
    bool m_dirty = false;
};
