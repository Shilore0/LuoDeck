#pragma once

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QVector>

struct LayoutItem {
    QString id;
    QString type = QStringLiteral("app"); // "app" | "group"
    QString path;                         // app: 目标路径
    QString name;                         // app: 显示名 / group: 名称
    int x = 0;
    int y = 0;
    int cols = 3; // group 专用
    int rows = 3; // group 专用
    QVector<LayoutItem> children;

    bool isGroup() const { return type == QStringLiteral("group"); }
    int capacity() const { return qMax(1, cols * rows); }
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
    QSize canvasSize() const { return m_canvas; }

    void setLanguage(const QString &v);
    void setTheme(const QString &v);
    void setHotkey(const QString &v);
    void setIconSize(int v);

    QVector<LayoutItem> &items() { return m_items; }
    const QVector<LayoutItem> &items() const { return m_items; }

    QString dataDir() const { return m_dataDir; }
    QString configPath() const { return m_configPath; }
    QString themesDir() const { return m_dataDir + "/themes"; }
    QString localesDir() const { return m_dataDir + "/i18n"; }
    QString groupsDir() const { return m_dataDir + "/groups"; }

    LayoutItem *findById(const QString &id);
    const LayoutItem *findById(const QString &id) const;
    LayoutItem *findParentOf(const QString &id);

    QString addApp(const QString &path, const QString &groupId = QString());
    QString addGroup(const QString &name = QString());
    void removeItem(const QString &id);
    bool renameGroup(const QString &id, const QString &newName);
    void moveItem(const QString &id, const QPoint &pos);
    bool setGroupGrid(const QString &id, int cols, int rows);
    bool detachFromGroup(const QString &id, const QPoint &pos);

    QString suggestGroupName() const;
    QString groupDirFor(const QString &groupName) const;

    void notifyChanged();

    static QString makeId();
    static QString displayNameFor(const QString &path);
    static bool isFolderPath(const QString &path);

signals:
    void changed();
    void layoutChanged();

private:
    AppConfig() = default;

    void applyDefaults();
    void seedFromDesktop();
    void ensureDirectories();
    void ensureGroupDir(const QString &groupName);
    void mirrorShortcut(const QString &groupName, const QString &sourcePath);
    void removeGroupDir(const QString &groupName);
    void renameGroupDir(const QString &oldName, const QString &newName);

    QString m_dataDir;
    QString m_configPath;
    QString m_language = "zh_CN";
    QString m_theme = "default";
    QString m_hotkey = "ctrl+space";
    int m_iconSize = 48;
    QSize m_canvas{720, 520};
    QVector<LayoutItem> m_items;
};
