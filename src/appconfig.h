#pragma once

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

struct LayoutItem {
    QString id;
    QString type = QStringLiteral("app"); // "app" | "group"
    QString path;                         // app: 目标路径
    QString name;                         // app: 显示名 / group: 名称
    QString color;                        // group: 边框颜色 #RRGGBB
    bool system = false;                  // true = 系统创建（如"桌面"），名字与颜色不可改
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
    QString hotkeyAdd() const { return m_hotkeyAdd; }
    QString pythonPath() const { return m_pythonPath; }
    int iconSize() const { return m_iconSize; }
    QSize canvasSize() const { return m_canvas; }
    bool hideDesktopFrame() const { return m_hideDesktopFrame; }

    void setLanguage(const QString &v);
    void setTheme(const QString &v);
    void setHotkey(const QString &v);
    void setHotkeyAdd(const QString &v);
    void setPythonPath(const QString &v);
    void setIconSize(int v);
    void setHideDesktopFrame(bool v);

    QVector<LayoutItem> &items() { return m_items; }
    const QVector<LayoutItem> &items() const { return m_items; }

    QString dataDir() const { return m_dataDir; }
    QString configPath() const { return m_configPath; }
    QString themesDir() const { return m_dataDir + "/themes"; }
    QString localesDir() const { return m_dataDir + "/i18n"; }
    QString groupsDir() const { return m_dataDir + "/groups"; }

    LayoutItem *findById(const QString &id);
    const LayoutItem *findById(const QString &id) const;

    QString addApp(const QString &path, const QString &groupId = QString());
    QString addGroup(const QString &name = QString(), const QString &color = QString());
    void removeItem(const QString &id);
    bool renameGroup(const QString &id, const QString &newName);
    bool setGroupColor(const QString &id, const QString &color);
    void moveItem(const QString &id, const QPoint &pos);
    bool setGroupGrid(const QString &id, int cols, int rows);

    QString suggestGroupName() const;
    QString desktopFrameId() const;
    void syncDesktopFrame();

    void notifyChanged();

    static QString makeId();
    static QString displayNameFor(const QString &path);
    static bool isFolderPath(const QString &path);
    static QStringList palette();
    static QString randomPaletteColor();
    static QString defaultDesktopColor();

signals:
    void changed();
    void layoutChanged();
    // 只影响观感（如颜色）的改动，不需要重建整个画布
    void itemVisualChanged(const QString &id);

private:
    AppConfig() = default;

    void applyDefaults();
    void ensureDesktopFrame();
    void ensureDirectories();
    QString groupDirFor(const QString &groupName) const;
    void ensureGroupDir(const QString &groupName);
    void mirrorShortcut(const QString &groupName, const QString &sourcePath);
    void removeGroupDir(const QString &groupName);
    void renameGroupDir(const QString &oldName, const QString &newName);

    QString m_dataDir;
    QString m_configPath;
    QString m_language = "zh_CN";
    QString m_theme = "default";
    QString m_hotkey = "ctrl+space";
    QString m_hotkeyAdd = "ctrl+alt+l";
    QString m_pythonPath;
    int m_iconSize = 48;
    QSize m_canvas{720, 520};
    bool m_hideDesktopFrame = false;
    QVector<LayoutItem> m_items;
};
