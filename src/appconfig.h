#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QElapsedTimer>
#include <QStringList>
#include <QVector>

class QFileSystemWatcher;
class QTimer;

struct LayoutItem {
    QString id;
    QString type = QStringLiteral("app"); // "app" | "group"
    QString path;                         // app: 目标路径
    QString name;                         // app: 显示名 / group: 名称
    // app: 启动参数（原样接到程序后面，留空就是直接打开）
    QString args;
    QString color;                        // group: 边框颜色 #RRGGBB
    bool system = false;                  // true = 系统创建（如"桌面"），名字与颜色不可改
    // 系统框的来源："desktop" / "startmenu"；用户自己建的框为空
    QString source;
    // true = 手机那种"一格大小"的文件夹（缩略显示里面 4 个图标），false = 展开的大框
    bool compact = false;
    int x = 0;
    int y = 0;
    int cols = 3; // group 专用
    int rows = 3; // group 专用
    // 用户手动指定过容量：之后按图标数量自动定大小的逻辑不再覆盖它
    bool manualGrid = false;
    QVector<LayoutItem> children;

    bool isGroup() const { return type == QStringLiteral("group"); }
    bool isDesktopFrame() const { return isGroup() && source == QStringLiteral("desktop"); }
    bool isStartMenuFrame() const { return isGroup() && source == QStringLiteral("startmenu"); }
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
    bool showStartMenu() const { return m_showStartMenu; }
    // 首次运行是否还没问过"要不要把开始菜单固定项复制过来"
    bool needsStartMenuPrompt() const { return !m_startMenuAsked; }
    void markStartMenuAsked();
    QStringList hiddenPaths() const { return m_hiddenPaths; }

    void setLanguage(const QString &v);
    void setTheme(const QString &v);
    void setHotkey(const QString &v);
    void setHotkeyAdd(const QString &v);
    void setPythonPath(const QString &v);
    void setIconSize(int v);
    void setHideDesktopFrame(bool v);
    void setShowStartMenu(bool v);
    // 从面板移除某个路径，并记入忽略列表（桌面同步时不再加回来）
    void hidePath(const QString &path);

    QVector<LayoutItem> &items() { return m_items; }
    const QVector<LayoutItem> &items() const { return m_items; }

    // 当前真正显示在面板上的项。隐藏起来的「桌面」框不算——
    // 否则面板尺寸、空位计算里都还留着它占的那块地方。
    QVector<LayoutItem> laidOutItems() const;
    bool isHiddenItem(const LayoutItem &item) const;

    QString dataDir() const { return m_dataDir; }
    QString configPath() const { return m_configPath; }
    QString themesDir() const { return m_dataDir + "/themes"; }
    QString localesDir() const { return m_dataDir + "/i18n"; }
    QString groupsDir() const { return m_dataDir + "/groups"; }

    LayoutItem *findById(const QString &id);
    const LayoutItem *findById(const QString &id) const;
    // 找到某个 app 项所属的分组（顶层项返回 nullptr；out 指向该项本身）
    LayoutItem *parentGroupOf(const QString &childId);

    QString addApp(const QString &path, const QString &groupId = QString());
    // 添加 Windows 商店应用（UWP）：path 存成 shell:AppsFolder\<aumid>，没有真实文件
    QString addShellApp(const QString &aumid, const QString &name,
                        const QString &groupId = QString());
    QString addGroup(const QString &name = QString(), const QString &color = QString());
    void removeItem(const QString &id);
    bool renameGroup(const QString &id, const QString &newName);
    // 重命名单个图标（只改显示名，不动磁盘文件）
    bool renameItem(const QString &id, const QString &newName);
    // 设置某个图标的启动参数（原样接到程序后面）
    bool setItemArgs(const QString &id, const QString &args);
    bool setGroupColor(const QString &id, const QString &color);
    void moveItem(const QString &id, const QPoint &pos);
    // 把某个顶层图标挪到最近的空位（拖到「桌面」框上时用它，避免压住框）
    void moveItemToFreeSpot(const QString &id);
    bool setGroupGrid(const QString &id, int cols, int rows);
    // 取消手动容量，回到"按图标数量自动定大小"
    bool setGroupAutoGrid(const QString &id);

    // 把顶层图标拖进某个框
    bool moveItemIntoGroup(const QString &itemId, const QString &groupId);
    // 把一个图标拖到另一个图标上：合成一个新文件夹（手机桌面那种），返回新框 id
    QString mergeIntoFolder(const QString &draggedId, const QString &targetId);
    // 解散：删掉文件夹本身，里面的图标放回面板
    bool dissolveGroup(const QString &id);
    // 大框 <-> 小文件夹 切换
    bool setGroupCompact(const QString &id, bool compact);
    // 把和新框重叠的图标挪到空位
    // 返回被挪走的项数（用于决定界面要不要重建）
    int pushOverlaps(const QString &keepId);
    // 文件夹按图标数量自动定大小：2 格起，最多 3×3，再多就得展开看
    void fitFolderGrid(int index);
    // 框变大后如果压住别的框，把自己挪到空位
    void settleGroup(int index);
    // 把所有非系统框理一遍，确保没有互相压住
    void resolveGroupOverlaps();
    // 老配置迁到网格：吸附格点并解冲突
    void snapAllToGrid();
    // 把框内图标拖到画布上，成为独立图标
    QString detachChild(const QString &childId, const QPoint &pos);
    // 框内重排：把第 from 个图标挪到第 to 个位置（其余顺延），顺序会写进 config.json
    bool moveChildToIndex(const QString &groupId, int from, int to);

    // 一键把顶层项按顺序重新排布，保证互不重叠
    void autoArrange();
    // 不排序，只把显示中的项按当前顺序重新摆紧（隐藏「桌面」框后收回空位用）
    void reflowLayout();
    bool clearGroup(const QString &id);
    bool sortGroup(const QString &id);

    // 内容需要的画布尺寸（宽度用配置值，高度随内容增长）
    QRect contentBounds() const;
    QSize requiredCanvasSize() const;
    void growCanvasToFit();

    QString suggestGroupName() const;
    QString desktopFrameId() const;
    void syncDesktopFrame();   // 与桌面目录对账（不落盘）
    void refreshDesktop();     // 对账 + 落盘 + 通知界面
    // 开始菜单"已固定"：Windows 把固定项存在加密库里读不出来，
    // 但 pinned 到开始菜单根目录的散装 .lnk 与它几乎一一对应，就按这个导入（零散图标，不进框）
    void syncPinnedShortcuts();
    // 把自动导入的图标按行摆整齐（避开框和用户自己放的图标）
    void arrangePinnedIcons();
    // force=false 时按节流限制频率（面板每次弹出都会调，重扫要几十毫秒）
    void refreshPinned(bool force = false);
    QStringList collectPinnedShortcuts() const;

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
    // 按格子重新摆放显示中的项；sortByName=true 时先按「框在前、图标按名字」排序
    void packItems(bool sortByName);
    QVector<int> laidOutIndexes() const;
    void ensureDirectories();
    QString groupDirFor(const QString &groupName) const;
    void ensureGroupDir(const QString &groupName);
    void mirrorShortcut(const QString &groupName, const QString &sourcePath);
    void removeGroupDir(const QString &groupName);
    void renameGroupDir(const QString &oldName, const QString &newName);
    void fitDesktopGrid();
    void startWatchers();

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
    // 隐藏「桌面」框之后是否已经把空出来的位置收拢过（只做一次，不干扰用户后来手动摆的位置）
    bool m_compactHiddenDone = false;
    bool m_showStartMenu = false;       // 是否导入开始菜单的固定项（首次运行由用户决定）
    bool m_startMenuAsked = false;      // 首次运行的询问是否已经问过
    QStringList m_dismissedPins;        // 用户从面板删掉的固定项，不再自动加回来
    QStringList m_hiddenPaths;
    QVector<LayoutItem> m_items;

    // 桌面 / 开始菜单目录监听：内容变化后面板即时跟随
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
    QStringList m_watchDirs;
    QElapsedTimer m_startMenuClock;
};
