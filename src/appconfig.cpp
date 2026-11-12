#include "appconfig.h"

#include "layoutengine.h"
#include "metrics.h"
#include "winapps.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRect>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace {

constexpr int kMinCanvasWidth = 420;
constexpr int kMinCanvasHeight = 260;

// 开始菜单框：5 列，最多 8 行，超出的用「更多」弹层（可搜索）看全部
constexpr int kStartMenuCols = 5;
constexpr int kStartMenuMaxRows = 8;

// 开始菜单里那些"不是程序"的项：卸载、说明、帮助、官网…
bool isNoiseShortcut(const QString &name)
{
    static const QStringList tails = {
        QStringLiteral("uninstall"), QStringLiteral("readme"),  QStringLiteral("help"),
        QStringLiteral("license"),   QStringLiteral("website"), QStringLiteral("manual"),
        QStringLiteral("卸载"),      QStringLiteral("说明"),    QStringLiteral("帮助"),
        QStringLiteral("许可协议"),  QStringLiteral("官网"),    QStringLiteral("更新程序"),
        QStringLiteral("修复"),      QStringLiteral("教程"),    QStringLiteral("文档"),
    };
    const QString lower = name.trimmed().toLower();
    for (const QString &tail : tails) {
        if (lower.endsWith(tail))
            return true;
    }
    return false;
}

QJsonObject itemToJson(const LayoutItem &it)
{
    QJsonObject o;
    o.insert("id", it.id);
    o.insert("type", it.type);
    o.insert("name", it.name);
    o.insert("x", it.x);
    o.insert("y", it.y);
    if (it.system)
        o.insert("system", true);
    if (!it.source.isEmpty())
        o.insert("source", it.source);

    if (it.isGroup()) {
        o.insert("cols", it.cols);
        o.insert("rows", it.rows);
        if (it.manualGrid)
            o.insert("manualGrid", true);
        if (!it.color.isEmpty())
            o.insert("color", it.color);
        if (it.compact)
            o.insert("compact", true);
        QJsonArray kids;
        for (const LayoutItem &c : it.children)
            kids.append(itemToJson(c));
        o.insert("children", kids);
    } else {
        o.insert("path", it.path);
        if (!it.args.isEmpty())
            o.insert("args", it.args);
    }
    return o;
}

LayoutItem itemFromJson(const QJsonObject &o)
{
    LayoutItem it;
    it.id = o.value("id").toString();
    if (it.id.isEmpty())
        it.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    it.type = o.value("type").toString("app");
    it.name = o.value("name").toString();
    it.color = o.value("color").toString();
    it.compact = o.value("compact").toBool(false);
    it.system = o.value("system").toBool(false);
    it.source = o.value("source").toString();
    if (it.system && it.source.isEmpty())
        it.source = QStringLiteral("desktop");
    it.x = o.value("x").toInt(0);
    it.y = o.value("y").toInt(0);
    it.path = o.value("path").toString();
    it.args = o.value("args").toString();
    it.cols = qBound(1, o.value("cols").toInt(3), 8);
    it.rows = qBound(1, o.value("rows").toInt(3), 8);
    it.manualGrid = o.value("manualGrid").toBool(false);

    const QJsonArray kids = o.value("children").toArray();
    for (const QJsonValue &v : kids)
        it.children.append(itemFromJson(v.toObject()));

    return it;
}

QString sanitizeFolderName(const QString &raw)
{
    QString s = raw;
    const QString bad = "\\/:*?\"<>|";
    for (const QChar &c : bad)
        s.replace(c, '_');
    s = s.trimmed();

    // Windows 会静默吃掉结尾的点和空格（"abc." 和 "abc" 是同一个目录），
    // 更要命的是 "." / ".." 会被当成目录引用：groups/.. 就是数据根目录，
    // 一旦拼进路径，removeRecursively() 会把整个 LuoDeck 目录删掉。
    while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' ')))
        s.chop(1);

    if (s.isEmpty())
        s = QStringLiteral("folder");
    return s;
}

// 分组目录必须严格位于 groups/ 的下一层。用 cleanPath 把 ".." 折叠掉之后再比前缀，
// 这样 "groups/.."（= 数据根目录）之类的名字一定会被判为非法。
bool groupDirIsInside(const QString &groupsRoot, const QString &candidate)
{
    const QString root = QDir::cleanPath(QDir(groupsRoot).absolutePath());
    const QString path = QDir::cleanPath(QDir(candidate).absolutePath());
    return path.startsWith(root + QLatin1Char('/'));
}

// 这个路径是不是桌面目录里的东西（Windows 上大小写不敏感）
bool isOnDesktopPath(const QString &normalizedPath)
{
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty() || normalizedPath.isEmpty())
        return false;

    const QString root = QDir::cleanPath(QDir::fromNativeSeparators(desktop)).toLower();
    const QString path = QDir::cleanPath(normalizedPath).toLower();
    return path.startsWith(root + QLatin1Char('/'));
}

// 图标离开某个框时，把 groups/<框>/ 里对应的镜像 .lnk 一起清掉。
// 不清的话，删删拖拖久了目录里全是不指向任何东西的死链。
void removeMirrorLink(const QString &groupsRoot, const QString &groupName,
                      const QString &sourcePath, const QVector<LayoutItem> &remaining)
{
    const QString dirPath = groupsRoot + "/" + sanitizeFolderName(groupName);
    if (!groupDirIsInside(groupsRoot, dirPath))
        return;

    const QString linkName = AppConfig::displayNameFor(sourcePath);
    // 同一个框里还有别的项映射到同名链接，就不能删
    for (const LayoutItem &c : remaining) {
        if (AppConfig::displayNameFor(c.path).compare(linkName, Qt::CaseInsensitive) == 0)
            return;
    }

    QFile::remove(dirPath + "/" + linkName + ".lnk");
}

bool isInteresting(const QFileInfo &fi)
{
    if (fi.isDir())
        return true;
    const QString suffix = fi.suffix().toLower();
    return suffix == "lnk" || suffix == "url" || suffix == "exe" || suffix == "bat"
        || suffix == "cmd" || suffix == "ps1" || suffix == "py";
}

} // namespace

AppConfig &AppConfig::instance()
{
    static AppConfig cfg;
    return cfg;
}

QString AppConfig::makeId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString AppConfig::displayNameFor(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.isDir())
        return fi.fileName();
    QString base = fi.completeBaseName();
    if (base.isEmpty())
        base = fi.fileName();
    if (base.isEmpty())
        base = fi.absoluteFilePath();
    return base;
}

bool AppConfig::isFolderPath(const QString &path)
{
    return QFileInfo(path).isDir();
}

QStringList AppConfig::palette()
{
    return {QStringLiteral("#F2B33D"), QStringLiteral("#4A90D9"), QStringLiteral("#4CAF7D"),
            QStringLiteral("#E86A92"), QStringLiteral("#9B7EDE"), QStringLiteral("#F08A4B"),
            QStringLiteral("#48B8C4")};
}

QString AppConfig::defaultDesktopColor()
{
    return QStringLiteral("#F2B33D");
}

QString AppConfig::randomPaletteColor()
{
    const QStringList colors = palette();
    if (colors.isEmpty())
        return defaultDesktopColor();
    return colors.at(QRandomGenerator::global()->bounded(colors.size()));
}

void AppConfig::ensureDirectories()
{
    QDir().mkpath(m_dataDir);
    QDir().mkpath(themesDir());
    QDir().mkpath(localesDir());
    QDir().mkpath(groupsDir());
}

QString AppConfig::groupDirFor(const QString &groupName) const
{
    return groupsDir() + "/" + sanitizeFolderName(groupName);
}

void AppConfig::ensureGroupDir(const QString &groupName)
{
    const QString path = groupDirFor(groupName);
    if (!groupDirIsInside(groupsDir(), path))
        return; // 非法名字（".." 之类）不落盘
    QDir().mkpath(path);
}

void AppConfig::mirrorShortcut(const QString &groupName, const QString &sourcePath)
{
    const QFileInfo src(sourcePath);
    if (!src.exists())
        return;

    ensureGroupDir(groupName);
    const QString dirPath = groupDirFor(groupName);
    if (!groupDirIsInside(groupsDir(), dirPath))
        return;

    const QString target = dirPath + "/" + displayNameFor(sourcePath) + ".lnk";
    if (QFile::exists(target))
        return;

    // Windows 上 QFile::link 会创建真正的 .lnk 快捷方式
    QFile::link(src.absoluteFilePath(), target);
}

void AppConfig::removeGroupDir(const QString &groupName)
{
    const QString path = groupDirFor(groupName);
    // 原来的 startsWith 挡不住 ".."：必须先规范化再比，否则会删到数据根目录
    if (!groupDirIsInside(groupsDir(), path))
        return;
    QDir dir(path);
    if (dir.exists())
        dir.removeRecursively();
}

void AppConfig::renameGroupDir(const QString &oldName, const QString &newName)
{
    const QString from = groupDirFor(oldName);
    const QString to = groupDirFor(newName);
    if (from == to)
        return;
    if (!groupDirIsInside(groupsDir(), from) || !groupDirIsInside(groupsDir(), to))
        return;
    if (!QDir(from).exists() || QDir(to).exists())
        return;
    QDir().rename(from, to);
}

bool AppConfig::load()
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_dataDir = docs + "/LuoDeck";
    m_configPath = m_dataDir + "/config.json";
    ensureDirectories();

    QFile f(m_configPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        applyDefaults();
        ensureDesktopFrame();
        syncDesktopFrame();
        syncPinnedShortcuts();
        startWatchers();
        save();
        return true;
    }

    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        // 配置坏了不要直接覆盖：先留一份副本，用户的布局还有机会手工救回来
        const QString backup = m_configPath + QStringLiteral(".bad");
        QFile::remove(backup);
        QFile::copy(m_configPath, backup);

        applyDefaults();
        ensureDesktopFrame();
        syncDesktopFrame();
        syncPinnedShortcuts();
        startWatchers();
        save();
        return true;
    }

    const QJsonObject root = doc.object();
    m_language = root.value("language").toString("zh_CN");
    m_theme = root.value("theme").toString("default");
    m_hotkey = root.value("hotkey").toString("ctrl+space");
    m_hotkeyAdd = root.value("hotkeyAdd").toString("ctrl+alt+l");
    m_pythonPath = root.value("pythonPath").toString();
    m_iconSize = qBound(24, root.value("iconSize").toInt(48), 128);
    m_hideDesktopFrame = root.value("hideDesktopFrame").toBool(false);
    m_compactHiddenDone = root.value("compactHiddenDone").toBool(false);
    m_showStartMenu = root.value("showStartMenu").toBool(false);
    // 老用户早就导入过了，默认视为"已问过"，不要在新版第一次启动时弹框
    m_startMenuAsked = root.value("startMenuAsked").toBool(true);

    m_hiddenPaths.clear();
    const QJsonArray hidden = root.value("hiddenPaths").toArray();
    for (const QJsonValue &v : hidden)
        m_hiddenPaths.append(v.toString());

    m_dismissedPins.clear();
    const QJsonArray dismissed = root.value("dismissedPins").toArray();
    for (const QJsonValue &v : dismissed)
        m_dismissedPins.append(v.toString());

    const QJsonObject canvas = root.value("canvas").toObject();
    m_canvas = QSize(qBound(480, canvas.value("width").toInt(720), 2400),
                     qBound(360, canvas.value("height").toInt(520), 1600));

    m_items.clear();
    const QJsonArray arr = root.value("items").toArray();
    for (const QJsonValue &v : arr) {
        LayoutItem it = itemFromJson(v.toObject());
        // 旧版本把开始菜单塞进一个框里，现在改成散装图标，这里把旧框丢掉
        if (it.isStartMenuFrame())
            continue;
        if (!it.isGroup() && it.path.isEmpty())
            continue;
        if (it.name.isEmpty())
            it.name = it.isGroup() ? suggestGroupName() : displayNameFor(it.path);
        if (it.isGroup() && it.color.isEmpty())
            it.color = randomPaletteColor();
        m_items.append(it);
    }

    ensureDesktopFrame();
    syncDesktopFrame();

    // 老版本里新加的图标会被放在 (20,20)，正好压在「桌面」框里看不见。
    // 这里做一次性补救：完全被系统框盖住的顶层图标挪到空位。
    const QString frameId = desktopFrameId();
    int frameIndex = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == frameId) {
            frameIndex = i;
            break;
        }
    }
    if (frameIndex >= 0 && !isHiddenItem(m_items.at(frameIndex))) {
        const QRect frameRect = LayoutEngine::itemRect(m_items.at(frameIndex));
        for (int i = 0; i < m_items.size(); ++i) {
            LayoutItem &it = m_items[i];
            if (it.isGroup() || it.id == frameId)
                continue;
            if (!frameRect.contains(LayoutEngine::itemRect(it)))
                continue;
            const QPoint spot = LayoutEngine::findFreeSpot(
                LayoutEngine::occupiedRects(laidOutItems(), it.id), LayoutEngine::itemSize(it),
                requiredCanvasSize().width());
            it.x = spot.x();
            it.y = spot.y();
        }
    }

    syncPinnedShortcuts();
    snapAllToGrid(); // 老配置是自由坐标，迁到格点
    resolveGroupOverlaps(); // 框之间也不许互相压住
    // 框变大后可能又压住图标，再推一遍
    for (const LayoutItem &it : m_items) {
        if (it.isGroup() && !isHiddenItem(it))
            pushOverlaps(it.id);
    }
    growCanvasToFit();

    // 老配置里隐藏「桌面」框之后，它的那几格还留在布局里，面板就一直空着一块。
    // 这里收拢一次（按屏幕顺序、不重新按名字排），收过就记下来，之后不再擅自搬家。
    if (m_hideDesktopFrame && !m_compactHiddenDone) {
        m_compactHiddenDone = true;
        packItems(false);
    }

    startWatchers();
    save();

    return true;
}

bool AppConfig::save()
{
    if (m_dataDir.isEmpty())
        ensureDirectories();

    QJsonObject root;
    root.insert("version", 2);
    root.insert("language", m_language);
    root.insert("theme", m_theme);
    root.insert("hotkey", m_hotkey);
    root.insert("hotkeyAdd", m_hotkeyAdd);
    root.insert("pythonPath", m_pythonPath);
    root.insert("iconSize", m_iconSize);
    root.insert("hideDesktopFrame", m_hideDesktopFrame);
    root.insert("compactHiddenDone", m_compactHiddenDone);
    root.insert("showStartMenu", m_showStartMenu);
    root.insert("startMenuAsked", m_startMenuAsked);

    QJsonArray hiddenArr;
    for (const QString &p : m_hiddenPaths)
        hiddenArr.append(p);
    root.insert("hiddenPaths", hiddenArr);

    QJsonArray dismissedArr;
    for (const QString &p : m_dismissedPins)
        dismissedArr.append(p);
    root.insert("dismissedPins", dismissedArr);

    QJsonObject canvas;
    canvas.insert("width", m_canvas.width());
    canvas.insert("height", m_canvas.height());
    root.insert("canvas", canvas);

    QJsonArray arr;
    for (const LayoutItem &it : m_items)
        arr.append(itemToJson(it));
    root.insert("items", arr);

    QSaveFile f(m_configPath);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "LuoDeck: cannot open config for writing:" << m_configPath;
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        qWarning() << "LuoDeck: config write failed (disk full / permission?):" << m_configPath;
        return false;
    }
    return true;
}

void AppConfig::applyDefaults()
{
    m_language = "zh_CN";
    m_theme = "default";
    m_hotkey = "ctrl+space";
    m_hotkeyAdd = "ctrl+alt+l";
    m_pythonPath.clear();
    m_iconSize = 48;
    m_canvas = QSize(720, 520);
    m_hideDesktopFrame = false;
    m_showStartMenu = false;
    m_startMenuAsked = false;
    m_hiddenPaths.clear();
    m_items.clear();
}

void AppConfig::ensureDesktopFrame()
{
    for (const LayoutItem &it : m_items) {
        if (it.isDesktopFrame())
            return;
    }

    LayoutItem frame;
    frame.id = makeId();
    frame.type = "group";
    frame.name = QStringLiteral("桌面");
    frame.system = true;
    frame.source = QStringLiteral("desktop");
    frame.color = defaultDesktopColor();
    frame.cols = 4;
    frame.rows = 4;
    frame.x = 16;
    frame.y = 16;

    m_items.prepend(frame);

    // 让已有分组避开桌面框，避免首次升级后两个框叠在一起
    const QRect frameRect = LayoutEngine::itemRect(frame);
    for (LayoutItem &it : m_items) {
        if (!it.isGroup() || it.system)
            continue;
        if (!LayoutEngine::itemRect(it).intersects(frameRect))
            continue;

        const QPoint spot = LayoutEngine::findFreeSpot(
            LayoutEngine::occupiedRects(laidOutItems(), it.id), LayoutEngine::itemSize(it),
            requiredCanvasSize().width());
        it.x = spot.x();
        it.y = spot.y();
    }
}

void AppConfig::fitDesktopGrid()
{
    LayoutItem *frame = nullptr;
    for (LayoutItem &it : m_items) {
        if (it.isDesktopFrame()) {
            frame = &it;
            break;
        }
    }
    if (!frame)
        return;

    const int cols = qMax(1, frame->cols);
    const int need = (frame->children.size() + cols - 1) / cols;
    // 贴着内容走：图标少的时候不要把框撑得老大
    frame->rows = qBound(2, need, 8);
}

void AppConfig::syncDesktopFrame()
{
    LayoutItem *frame = nullptr;
    for (LayoutItem &it : m_items) {
        if (it.isDesktopFrame()) {
            frame = &it;
            break;
        }
    }
    if (!frame)
        return;

    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty())
        return;

    QDir dir(desktop);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    QStringList onDisk;
    for (const QFileInfo &fi : entries) {
        if (!isInteresting(fi))
            continue;
        onDisk.append(QDir::fromNativeSeparators(fi.absoluteFilePath()));
    }

    const QSet<QString> diskSet(onDisk.cbegin(), onDisk.cend());

    QVector<LayoutItem> kept;
    kept.reserve(frame->children.size());
    for (const LayoutItem &child : frame->children) {
        if (diskSet.contains(child.path))
            kept.append(child);
    }

    QSet<QString> keptPaths;
    for (const LayoutItem &child : kept)
        keptPaths.insert(child.path);

    for (const QString &path : onDisk) {
        if (keptPaths.contains(path))
            continue;
        if (m_hiddenPaths.contains(path)) // 用户主动隐藏过，不要再加回来
            continue;
        LayoutItem child;
        child.id = makeId();
        child.type = "app";
        child.path = path;
        child.name = displayNameFor(path);
        kept.append(child);
        mirrorShortcut(frame->name, path);
    }

    frame->children = kept;
    fitDesktopGrid();
    pushOverlaps(frame->id);

    // 桌面上的东西统一收进「桌面」框：如果同一个文件还散在面板顶层，把顶层那份去掉，
    // 否则会出现"桌面框里有一个、外面还挂着一个"的重复图标。
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const LayoutItem &it = m_items.at(i);
        if (it.isGroup() || it.path.isEmpty())
            continue;
        if (diskSet.contains(it.path))
            m_items.removeAt(i);
    }
}

void AppConfig::startWatchers()
{
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);

        // 资源管理器保存/重命名时会短时间连续触发多次事件，攒一下再对账
        m_debounce = new QTimer(this);
        m_debounce->setSingleShot(true);
        m_debounce->setInterval(300);
        connect(m_debounce, &QTimer::timeout, this, [this] {
            // 目录被替换后 watcher 会掉线，这里顺手补回来
            for (const QString &dir : m_watchDirs) {
                if (QDir(dir).exists() && !m_watcher->directories().contains(dir))
                    m_watcher->addPath(dir);
            }
            refreshDesktop();
            refreshPinned(true);
        });

        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
            m_debounce->start();
        });
    }

    m_watchDirs.clear();
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktop.isEmpty())
        m_watchDirs << desktop;

    const QString appData = qEnvironmentVariable("APPDATA");
    const QString programData = qEnvironmentVariable("ProgramData");
    if (!appData.isEmpty())
        m_watchDirs << QDir::fromNativeSeparators(appData
                                                  + "/Microsoft/Windows/Start Menu/Programs");
    if (!programData.isEmpty())
        m_watchDirs << QDir::fromNativeSeparators(programData
                                                  + "/Microsoft/Windows/Start Menu/Programs");

    for (const QString &dir : m_watchDirs) {
        if (QDir(dir).exists() && !m_watcher->directories().contains(dir))
            m_watcher->addPath(dir);
    }
}

// 系统框的"指纹"：尺寸 + 子项路径。用来判断同步后到底有没有变化
static QString frameFingerprint(const QVector<LayoutItem> &items, bool desktop)
{
    QString out = QString::number(items.size()) + QLatin1Char('#');
    for (const LayoutItem &it : items) {
        const bool match = desktop ? it.isDesktopFrame() : it.isStartMenuFrame();
        if (!match)
            continue;
        out += QStringLiteral("%1x%2|").arg(it.cols).arg(it.rows);
        for (const LayoutItem &c : it.children)
            out += c.path + QLatin1Char(';');
        break;
    }
    return out;
}

// 面板上"自动导入的固定项"清单，用来判断同步后有没有变化
static QString pinnedFingerprint(const QVector<LayoutItem> &items)
{
    QStringList paths;
    for (const LayoutItem &it : items) {
        if (!it.isGroup() && it.source == QLatin1String("startpin"))
            paths << it.path;
    }
    paths.sort();
    return paths.join(QLatin1Char(';'));
}

void AppConfig::refreshDesktop()
{
    // 每次弹出面板都会调一次，没变化时不要写盘、也不要让界面重建
    const QString before = frameFingerprint(m_items, true);
    syncDesktopFrame();
    if (frameFingerprint(m_items, true) == before)
        return;

    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
}

QStringList AppConfig::collectPinnedShortcuts() const
{
    QStringList roots;
    const QString appData = qEnvironmentVariable("APPDATA");
    const QString programData = qEnvironmentVariable("ProgramData");
    if (!appData.isEmpty())
        roots << QDir::fromNativeSeparators(appData
                                            + "/Microsoft/Windows/Start Menu/Programs");
    if (!programData.isEmpty())
        roots << QDir::fromNativeSeparators(programData
                                            + "/Microsoft/Windows/Start Menu/Programs");

    // 只看根目录：子文件夹里是"全部应用"里的其它程序，
    // 根目录下这些散装 .lnk 才是"已固定"里那批（pin 一个没有开始菜单条目的程序时，
    // Windows 就是在根目录建一个 .lnk）。
    struct Entry { QString path; QString name; };
    QVector<Entry> entries;
    QSet<QString> seen;

    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists())
            continue;

        // 注意：指向文件夹/驱动器的 .lnk 会被 Qt 判成目录，用 QDir::Files 会漏掉这些项，
        // 所以这里取所有条目再按扩展名筛。
        const QFileInfoList files = dir.entryInfoList(
            {QStringLiteral("*.lnk"), QStringLiteral("*.url")},
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
        for (const QFileInfo &fi : files) {
            const QString name = fi.completeBaseName();
            if (name.isEmpty())
                continue;

            const QString key = name.toLower();
            if (seen.contains(key) || isNoiseShortcut(name))
                continue;
            seen.insert(key);
            entries.append({QDir::fromNativeSeparators(fi.absoluteFilePath()), name});
        }
    }

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    QStringList out;
    out.reserve(entries.size());
    for (const Entry &e : entries)
        out.append(e.path);

    return out;
}

void AppConfig::syncPinnedShortcuts()
{
    if (!m_showStartMenu)
        return;

    const QStringList wanted = collectPinnedShortcuts();

    QSet<QString> wantedSet;
    for (const QString &p : wanted)
        wantedSet.insert(p.toLower());

    // 1) 已经不在开始菜单根目录的（取消固定 / 卸载）从面板上撤掉
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const LayoutItem &it = m_items.at(i);
        if (it.isGroup() || it.source != QLatin1String("startpin"))
            continue;
        if (!wantedSet.contains(it.path.toLower()))
            m_items.removeAt(i);
    }

    // 2) 新出现的加进来；用户手动删过的、隐藏过的不再打扰
    //    注意：已经被用户收进文件夹里的也算"已经有了"，否则同步会再补一份到面板上
    QSet<QString> have;
    for (const LayoutItem &it : m_items) {
        if (it.isGroup()) {
            for (const LayoutItem &c : it.children) {
                if (!c.path.isEmpty())
                    have.insert(c.path.toLower());
            }
        } else if (it.source == QLatin1String("startpin")) {
            have.insert(it.path.toLower());
        }
    }

    const int before = have.size();
    for (const QString &path : wanted) {
        const QString key = path.toLower();
        if (have.contains(key) || m_hiddenPaths.contains(path) || m_dismissedPins.contains(path))
            continue;

        LayoutItem item;
        item.id = makeId();
        item.type = "app";
        item.path = path;
        item.name = displayNameFor(path);
        item.source = QStringLiteral("startpin");

        const QPoint spot = LayoutEngine::findFreeSpot(LayoutEngine::occupiedRects(laidOutItems()),
                                                       LayoutEngine::itemSize(item),
                                                       requiredCanvasSize().width());
        item.x = spot.x();
        item.y = spot.y();

        have.insert(key);
        m_items.append(item);
    }

    // 有新增就重排一次，避免新图标零零散散插在缝里
    if (have.size() != before)
        arrangePinnedIcons();
}

void AppConfig::arrangePinnedIcons()
{
    const int pad = 16;
    const int gap = 12;
    const int layoutW = qMax(kMinCanvasWidth, m_canvas.width());
    const int cellW = Metrics::kCellW;
    const int cellH = Metrics::kCellH;

    // 障碍物：所有框 + 用户自己的图标（用户手动摆的位置不动）
    QVector<QRect> obstacles;
    for (const LayoutItem &it : m_items) {
        if (it.isGroup() || it.source != QLatin1String("startpin"))
            obstacles.append(LayoutEngine::itemRect(it));
    }

    QVector<int> todo;
    for (int i = 0; i < m_items.size(); ++i) {
        const LayoutItem &it = m_items.at(i);
        if (!it.isGroup() && it.source == QLatin1String("startpin"))
            todo.append(i);
    }

    int x = pad;
    int y = pad;
    // 兜底上限：框把整行都占满时，扫描式摆放会一路往下推，画布能被撑到几千像素高
    const int yLimit = pad + 120 * (cellH + gap);

    while (!todo.isEmpty()) {
        if (y > yLimit) {
            const int index = todo.takeFirst();
            const QSize sz = LayoutEngine::itemSize(m_items.at(index));
            const QPoint spot = LayoutEngine::findFreeSpot(obstacles, sz, layoutW);
            m_items[index].x = spot.x();
            m_items[index].y = spot.y();
            obstacles.append(QRect(spot, sz));
            continue;
        }

        const QRect cell(x, y, cellW, cellH);

        bool blocked = false;
        for (const QRect &o : obstacles) {
            if (cell.intersects(o.adjusted(-gap, -gap, gap, gap))) {
                blocked = true;
                break;
            }
        }

        if (blocked) {
            x += cellW + gap;
            if (x + cellW > layoutW - pad) {
                x = pad;
                y += cellH + gap;
            }
            continue;
        }

        const int index = todo.takeFirst();
        m_items[index].x = x;
        m_items[index].y = y;
        obstacles.append(QRect(x, y, cellW, cellH));

        x += cellW + gap;
        if (x + cellW > layoutW - pad) {
            x = pad;
            y += cellH + gap;
        }
    }
}

void AppConfig::refreshPinned(bool force)
{
    if (!m_showStartMenu)
        return;

    // 每次弹出面板都会调一次，扫一次要几十毫秒，没必要那么频繁
    if (!force && m_startMenuClock.isValid() && m_startMenuClock.elapsed() < 15000)
        return;
    m_startMenuClock.restart();

    const QString before = pinnedFingerprint(m_items);
    syncPinnedShortcuts();
    if (pinnedFingerprint(m_items) == before)
        return;

    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
}

void AppConfig::markStartMenuAsked()
{
    if (m_startMenuAsked)
        return;
    m_startMenuAsked = true;
    save();
}

void AppConfig::setShowStartMenu(bool v)
{
    if (m_showStartMenu == v)
        return;

    m_showStartMenu = v;
    if (v) {
        syncPinnedShortcuts();
    } else {
        for (int i = m_items.size() - 1; i >= 0; --i) {
            const LayoutItem &it = m_items.at(i);
            if (!it.isGroup() && it.source == QLatin1String("startpin"))
                m_items.removeAt(i);
        }
    }

    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
}

LayoutItem *AppConfig::findById(const QString &id)
{
    for (LayoutItem &it : m_items) {
        if (it.id == id)
            return &it;
        for (LayoutItem &c : it.children) {
            if (c.id == id)
                return &c;
        }
    }
    return nullptr;
}

const LayoutItem *AppConfig::findById(const QString &id) const
{
    for (const LayoutItem &it : m_items) {
        if (it.id == id)
            return &it;
        for (const LayoutItem &c : it.children) {
            if (c.id == id)
                return &c;
        }
    }
    return nullptr;
}

LayoutItem *AppConfig::parentGroupOf(const QString &childId)
{
    for (LayoutItem &it : m_items) {
        if (!it.isGroup())
            continue;
        for (const LayoutItem &c : it.children) {
            if (c.id == childId)
                return &it;
        }
    }
    return nullptr;
}

QString AppConfig::desktopFrameId() const
{
    for (const LayoutItem &it : m_items) {
        if (it.isDesktopFrame())
            return it.id;
    }
    return QString();
}

QString AppConfig::suggestGroupName() const
{
    QSet<QString> used;
    for (const LayoutItem &it : m_items) {
        if (it.isGroup())
            used.insert(it.name.trimmed());
    }

    int index = 1;
    while (used.contains(QStringLiteral("文件夹 %1").arg(index)))
        ++index;
    return QStringLiteral("文件夹 %1").arg(index);
}

QString AppConfig::addApp(const QString &path, const QString &groupId)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return QString();

    const QString normalized = QDir::fromNativeSeparators(fi.absoluteFilePath());

    // 用户重新添加时，解除此前的隐藏 / 已删除状态
    m_hiddenPaths.removeAll(normalized);
    m_dismissedPins.removeAll(normalized);

    LayoutItem *group = groupId.isEmpty() ? nullptr : findById(groupId);
    if (group && !group->isGroup())
        group = nullptr;
    // 只有「桌面」框能接手动加入，别的系统框不行
    if (group && group->system && !group->isDesktopFrame())
        group = nullptr;

    // 桌面上的文件必须收进「桌面」框：放到顶层的话，下一次 syncDesktopFrame()
    // 会因为"顶层不许有桌面文件"把它删掉，用户体验就是"加了又自己消失"。
    const bool onDesktop = isOnDesktopPath(normalized);
    if (!group && onDesktop) {
        for (LayoutItem &it : m_items) {
            if (it.isDesktopFrame()) {
                group = &it;
                break;
            }
        }
    }
    // 反过来，「桌面」框也不收桌面以外的东西
    if (group && group->isDesktopFrame() && !onDesktop)
        group = nullptr;

    // 同一路径不重复添加：已经在目标位置就当作已完成，避免出现两个一样的图标
    if (group) {
        for (const LayoutItem &c : group->children) {
            if (!c.isGroup() && c.path.compare(normalized, Qt::CaseInsensitive) == 0)
                return c.id;
        }
    } else {
        for (const LayoutItem &c : m_items) {
            if (!c.isGroup() && c.path.compare(normalized, Qt::CaseInsensitive) == 0)
                return c.id;
        }
    }

    LayoutItem item;
    item.id = makeId();
    item.type = "app";
    item.path = normalized;
    item.name = displayNameFor(normalized);

    if (group) {
        group->children.append(item);
        if (group->isDesktopFrame()) {
            // 桌面框贴内容走，不用手动加行
            fitDesktopGrid();
        } else if (!group->compact && group->children.size() > group->capacity()
                   && group->rows < 8) {
            // 大框满了往下长一格；小文件夹保持一格外观，多出来的在弹出层里看
            ++group->rows;
        }
        mirrorShortcut(group->name, normalized);
        growCanvasToFit();
        save();
        emit layoutChanged();
        emit changed();
        return item.id;
    }

    const QSize sz = LayoutEngine::itemSize(item);
    const QPoint spot = LayoutEngine::findFreeSpot(LayoutEngine::occupiedRects(laidOutItems()), sz,
                                                  requiredCanvasSize().width());
    item.x = spot.x();
    item.y = spot.y();

    m_items.append(item);
    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return item.id;
}

QString AppConfig::addShellApp(const QString &aumid, const QString &name, const QString &groupId)
{
    if (aumid.isEmpty())
        return QString();

    const QString path = WinApps::makePath(aumid);

    LayoutItem *group = groupId.isEmpty() ? nullptr : findById(groupId);
    if (group && (!group->isGroup() || group->system))
        group = nullptr;

    // 同一个应用不重复添加
    if (group) {
        for (const LayoutItem &c : group->children) {
            if (!c.isGroup() && c.path.compare(path, Qt::CaseInsensitive) == 0)
                return c.id;
        }
    } else {
        for (const LayoutItem &c : m_items) {
            if (!c.isGroup() && c.path.compare(path, Qt::CaseInsensitive) == 0)
                return c.id;
        }
    }

    LayoutItem item;
    item.id = makeId();
    item.type = "app";
    item.path = path;
    item.name = name.trimmed().isEmpty() ? aumid.section(QLatin1Char('!'), 0, 0) : name.trimmed();

    if (group) {
        group->children.append(item);
        if (!group->compact && group->children.size() > group->capacity() && group->rows < 8)
            ++group->rows;
        growCanvasToFit();
        save();
        emit layoutChanged();
        emit changed();
        return item.id;
    }

    const QSize sz = LayoutEngine::itemSize(item);
    const QPoint spot = LayoutEngine::findFreeSpot(LayoutEngine::occupiedRects(laidOutItems()), sz,
                                                  requiredCanvasSize().width());
    item.x = spot.x();
    item.y = spot.y();

    m_items.append(item);
    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return item.id;
}

QString AppConfig::addGroup(const QString &name, const QString &color)
{
    LayoutItem group;
    group.id = makeId();
    group.type = "group";
    group.name = name.isEmpty() ? suggestGroupName() : name;
    group.color = color.isEmpty() ? randomPaletteColor() : color;
    group.compact = false; // 默认展开
    group.cols = 2;
    group.rows = 2;

    const QPoint spot = LayoutEngine::findFreeSpot(LayoutEngine::occupiedRects(laidOutItems()),
                                                  LayoutEngine::itemSize(group),
                                                  requiredCanvasSize().width());
    group.x = spot.x();
    group.y = spot.y();

    m_items.append(group);
    ensureGroupDir(group.name);
    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return group.id;
}

void AppConfig::removeItem(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        LayoutItem &it = m_items[i];
        if (it.id != id)
            continue;
        if (it.system)
            return; // 系统框不允许删除，只能隐藏
        if (!it.isGroup() && it.source == QLatin1String("startpin")
            && !m_dismissedPins.contains(it.path))
            m_dismissedPins.append(it.path); // 用户不想要这一项，别再自动加回来
        if (it.isGroup())
            removeGroupDir(it.name);
        m_items.removeAt(i);
        save();
        emit layoutChanged();
        emit changed();
        return;
    }

    for (LayoutItem &parent : m_items) {
        for (int j = 0; j < parent.children.size(); ++j) {
            if (parent.children.at(j).id == id) {
                const LayoutItem removed = parent.children.at(j);
                parent.children.removeAt(j);
                if (parent.isGroup() && !parent.system)
                    removeMirrorLink(groupsDir(), parent.name, removed.path, parent.children);
                save();
                emit layoutChanged();
                emit changed();
                return;
            }
        }
    }
}

bool AppConfig::renameGroup(const QString &id, const QString &newName)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup() || group->system)
        return false;

    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == group->name)
        return false;

    const QString oldName = group->name;
    renameGroupDir(oldName, trimmed);
    group->name = trimmed;

    save();
    emit layoutChanged();
    emit changed();
    return true;
}

bool AppConfig::setItemArgs(const QString &id, const QString &args)
{
    LayoutItem *item = findById(id);
    if (!item || item->isGroup())
        return false;

    const QString trimmed = args.trimmed();
    if (trimmed == item->args)
        return false;

    item->args = trimmed;
    // 只影响启动行为，不用重建布局
    save();
    emit changed();
    return true;
}

bool AppConfig::renameItem(const QString &id, const QString &newName)
{
    LayoutItem *item = findById(id);
    if (!item)
        return false;

    if (item->isGroup())
        return renameGroup(id, newName);

    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == item->name)
        return false;

    item->name = trimmed;
    save();
    emit layoutChanged();
    emit changed();
    return true;
}

bool AppConfig::setGroupColor(const QString &id, const QString &color)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup() || group->system)
        return false;
    if (!color.startsWith('#'))
        return false;

    group->color = color;
    save();
    emit itemVisualChanged(id);
    emit changed();
    return true;
}

void AppConfig::moveItem(const QString &id, const QPoint &pos)
{
    LayoutItem *item = findById(id);
    if (!item)
        return;

    const QPoint snapped = LayoutEngine::snap(pos);
    item->x = snapped.x();
    item->y = snapped.y();

    // 拖到右边/下边时画布要跟着长，否则内容会被窗口切掉
    growCanvasToFit();

    // 只落盘，不发 layoutChanged：位置变了但结构没变，
    // 广播会让画布销毁重建所有控件，松手时肉眼可见地卡一下
    save();
}

void AppConfig::moveItemToFreeSpot(const QString &id)
{
    LayoutItem *item = findById(id);
    if (!item)
        return;

    const QPoint spot = LayoutEngine::findFreeSpot(
        LayoutEngine::occupiedRects(laidOutItems(), id), LayoutEngine::itemSize(*item),
        requiredCanvasSize().width());

    item->x = spot.x();
    item->y = spot.y();
    save();
}

bool AppConfig::setGroupGrid(const QString &id, int cols, int rows)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup())
        return false;

    group->cols = qBound(1, cols, 8);
    group->rows = qBound(1, rows, 8);
    // 用户自己定了容量，就别再让「按图标数量自动定大小」把它改回去
    group->manualGrid = true;
    if (group->compact)
        group->compact = false; // 给了容量就是想看见它，顺手展开

    int gIndex = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            gIndex = i;
            break;
        }
    }
    settleGroup(gIndex);
    pushOverlaps(id);
    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return true;
}

bool AppConfig::setGroupAutoGrid(const QString &id)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup())
        return false;

    group->manualGrid = false;
    if (group->compact) {
        save();
        emit layoutChanged();
        emit changed();
        return true;
    }

    int index = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            index = i;
            break;
        }
    }
    fitFolderGrid(index);
    settleGroup(index);
    pushOverlaps(id);
    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return true;
}

int AppConfig::pushOverlaps(const QString &keepId)
{
    const LayoutItem *keep = findById(keepId);
    if (!keep || isHiddenItem(*keep))
        return 0; // 藏起来的框不显示，不该把别人挤走

    int moved = 0;
    const QRect keepRect = LayoutEngine::itemRect(*keep);
    for (LayoutItem &it : m_items) {
        if (it.id == keepId || it.isGroup())
            continue; // 只挪图标，框不动
        if (!LayoutEngine::itemRect(it).intersects(keepRect.adjusted(-8, -8, 8, 8)))
            continue;

        const QPoint spot = LayoutEngine::findFreeSpot(
            LayoutEngine::occupiedRects(laidOutItems(), it.id), LayoutEngine::itemSize(it),
            requiredCanvasSize().width());
        it.x = spot.x();
        it.y = spot.y();
        ++moved;
    }
    return moved;
}

bool AppConfig::dissolveGroup(const QString &id)
{
    int index = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return false;

    LayoutItem &group = m_items[index];
    if (!group.isGroup() || group.system)
        return false;

    QVector<LayoutItem> kids = group.children;
    const QPoint origin(group.x, group.y); // 文件夹原来的位置，图标尽量散在这附近
    m_items.removeAt(index);

    auto spotFree = [this](const QRect &cand) {
        for (const QRect &r : LayoutEngine::occupiedRects(laidOutItems())) {
            if (cand.intersects(r.adjusted(-8, -8, 8, 8)))
                return false;
        }
        return true;
    };

    for (LayoutItem &child : kids) {
        const QSize sz = LayoutEngine::itemSize(child);
        QPoint spot;
        bool placed = false;

        // 先试文件夹原来那一格，再往右/往下扩两圈
        for (int ring = 0; ring <= 2 && !placed; ++ring) {
            for (int dy = 0; dy <= ring && !placed; ++dy) {
                for (int dx = 0; dx <= ring && !placed; ++dx) {
                    if (ring > 0 && dx < ring && dy < ring)
                        continue;
                    const QPoint cand(origin.x() + dx * LayoutEngine::kCellStepX,
                                      origin.y() + dy * LayoutEngine::kCellStepY);
                    if (cand.x() < 0 || cand.y() < 0)
                        continue;
                    if (!spotFree(QRect(cand, sz)))
                        continue;
                    spot = cand;
                    placed = true;
                }
            }
        }

        if (!placed) {
            spot = LayoutEngine::findFreeSpot(LayoutEngine::occupiedRects(laidOutItems()), sz,
                                              requiredCanvasSize().width());
        }

        child.x = spot.x();
        child.y = spot.y();
        m_items.append(child);
    }

    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return true;
}

bool AppConfig::setGroupCompact(const QString &id, bool compact)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup() || group->system)
        return false;
    if (group->compact == compact)
        return false;

    group->compact = compact;
    if (compact) {
        group->cols = qMax(2, group->cols);
        group->rows = qMax(2, group->rows);
    }

    // 尺寸变了，可能压到邻居
    int index = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            index = i;
            break;
        }
    }
    settleGroup(index);
    pushOverlaps(id);
    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return true;
}

QString AppConfig::mergeIntoFolder(const QString &draggedId, const QString &targetId)
{
    int ia = -1;
    int ib = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        const LayoutItem &it = m_items.at(i);
        if (it.isGroup())
            continue;
        if (it.id == draggedId)
            ia = i;
        else if (it.id == targetId)
            ib = i;
    }
    if (ia < 0 || ib < 0 || ia == ib)
        return QString();

    // 文件夹落在"被压住的那个图标"的格子，符合直觉
    LayoutItem folder;
    folder.id = makeId();
    folder.type = "group";
    folder.name = suggestGroupName();
    folder.color = randomPaletteColor();
    folder.compact = false; // 默认展开成大框
    folder.cols = 2;
    folder.rows = 1;
    folder.x = m_items.at(ib).x;
    folder.y = m_items.at(ib).y;

    folder.children.append(m_items.at(ia));
    folder.children.append(m_items.at(ib));

    // 先删大的下标，避免错位
    const int hi = qMax(ia, ib);
    const int lo = qMin(ia, ib);
    m_items.removeAt(hi);
    m_items.removeAt(lo);

    const QString folderId = folder.id;
    m_items.append(folder);
    ensureGroupDir(folder.name);

    const int folderIndex = m_items.size() - 1;
    fitFolderGrid(folderIndex);
    settleGroup(folderIndex);
    pushOverlaps(folderId);
    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return folderId;
}

void AppConfig::snapAllToGrid()
{
    for (LayoutItem &it : m_items) {
        const QPoint snapped = LayoutEngine::snap(QPoint(it.x, it.y));
        it.x = snapped.x();
        it.y = snapped.y();
    }

    // 吸附后可能有两项落到同一格，多出来的挪到空位
    for (int i = 0; i < m_items.size(); ++i) {
        LayoutItem &it = m_items[i];
        if (it.isGroup())
            continue;

        const QRect r = LayoutEngine::itemRect(it);
        bool clash = false;
        for (int j = 0; j < m_items.size(); ++j) {
            if (i == j || isHiddenItem(m_items.at(j)))
                continue;
            if (LayoutEngine::itemRect(m_items.at(j)).intersects(r)) {
                clash = true;
                break;
            }
        }
        if (!clash)
            continue;

        const QPoint spot = LayoutEngine::findFreeSpot(
            LayoutEngine::occupiedRects(laidOutItems(), it.id), LayoutEngine::itemSize(it),
            requiredCanvasSize().width());
        it.x = spot.x();
        it.y = spot.y();
    }
}

void AppConfig::fitFolderGrid(int index)
{
    if (index < 0 || index >= m_items.size())
        return;

    LayoutItem &g = m_items[index];
    if (!g.isGroup() || g.system || g.compact || g.manualGrid)
        return;

    // 2 个以内 2×1，3~4 个 2×2，5~6 个 3×2，7 个以上 3×3（再多就得点开看）
    const int n = g.children.size();
    int cols = 2;
    int rows = 1;
    if (n > 2) {
        cols = 2;
        rows = 2;
    }
    if (n > 4) {
        cols = 3;
        rows = 2;
    }
    if (n > 6) {
        cols = 3;
        rows = 3;
    }

    g.cols = cols;
    g.rows = rows;
}

void AppConfig::settleGroup(int index)
{
    if (index < 0 || index >= m_items.size())
        return;

    const QString id = m_items.at(index).id;
    const QRect self = LayoutEngine::itemRect(m_items.at(index));

    for (int j = 0; j < m_items.size(); ++j) {
        if (j == index || !m_items.at(j).isGroup())
            continue;
        if (isHiddenItem(m_items.at(j))) // 隐藏的框不显示，也不该把别人挤走
            continue;
        if (!self.intersects(LayoutEngine::itemRect(m_items.at(j))))
            continue;

        const QPoint spot = LayoutEngine::findFreeSpot(
            LayoutEngine::occupiedRects(laidOutItems(), id), LayoutEngine::itemSize(m_items.at(index)),
            requiredCanvasSize().width());
        m_items[index].x = spot.x();
        m_items[index].y = spot.y();
        return;
    }
}

void AppConfig::resolveGroupOverlaps()
{
    // 只挪用户自己的框，系统「桌面」框不动
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).isGroup() && !m_items.at(i).system)
            settleGroup(i);
}

bool AppConfig::moveItemIntoGroup(const QString &itemId, const QString &groupId)
{
    int groupIndex = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == groupId) {
            groupIndex = i;
            break;
        }
    }
    if (groupIndex < 0)
        return false;
    // 系统「桌面」框只跟随桌面目录，拖进去无效
    if (!m_items.at(groupIndex).isGroup() || m_items.at(groupIndex).system)
        return false;

    int itemIndex = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).isGroup() && m_items.at(i).id == itemId) {
            itemIndex = i;
            break;
        }
    }
    if (itemIndex < 0 || itemIndex == groupIndex)
        return false;

    const LayoutItem item = m_items.at(itemIndex);

    // 注意：removeAt 会让后面的元素整体前移，之前拿到的指针/引用都会失效，
    // 所以这里全程用下标，删完再重新定位。
    m_items.removeAt(itemIndex);
    if (itemIndex < groupIndex)
        --groupIndex;

    m_items[groupIndex].children.append(item);
    mirrorShortcut(m_items.at(groupIndex).name, item.path);

    fitFolderGrid(groupIndex);
    settleGroup(groupIndex);
    pushOverlaps(m_items.at(groupIndex).id);

    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
    return true;
}

QString AppConfig::detachChild(const QString &childId, const QPoint &pos)
{
    for (int g = 0; g < m_items.size(); ++g) {
        LayoutItem &group = m_items[g];
        if (!group.isGroup())
            continue;
        for (int i = 0; i < group.children.size(); ++i) {
            if (group.children.at(i).id != childId)
                continue;

            // 「桌面」框里的图标就是桌面上的文件，拖出来没有意义（下次同步还会回来）
            if (group.system)
                return QString();

            LayoutItem child = group.children.at(i);
            group.children.removeAt(i);
            removeMirrorLink(groupsDir(), group.name, child.path, group.children);
            child.id = makeId();
            child.x = qMax(0, pos.x() - Metrics::kCellW / 2);
            child.y = qMax(0, pos.y() - Metrics::kCellH / 2);

            const QString newId = child.id;
            m_items.append(child);

            growCanvasToFit();
            save();
            emit layoutChanged();
            emit changed();
            return newId;
        }
    }
    return QString();
}

void AppConfig::autoArrange()
{
    packItems(true);
}

void AppConfig::reflowLayout()
{
    packItems(false);
}

QVector<int> AppConfig::laidOutIndexes() const
{
    QVector<int> out;
    out.reserve(m_items.size());
    for (int i = 0; i < m_items.size(); ++i) {
        if (isHiddenItem(m_items.at(i)))
            continue;
        out.append(i);
    }
    return out;
}

bool AppConfig::isHiddenItem(const LayoutItem &item) const
{
    return m_hideDesktopFrame && item.isGroup() && item.system;
}

QVector<LayoutItem> AppConfig::laidOutItems() const
{
    QVector<LayoutItem> out;
    out.reserve(m_items.size());
    for (const LayoutItem &it : m_items) {
        if (isHiddenItem(it))
            continue;
        out.append(it);
    }
    return out;
}

void AppConfig::packItems(bool sortByName)
{
    const int pad = 16;
    const int gap = 12;
    const int stepX = LayoutEngine::kCellStepX;
    const int stepY = LayoutEngine::kCellStepY;

    // 列数按当前画布宽度算
    const int cols = qMax(1, (requiredCanvasSize().width() - 2 * pad + gap) / stepX);

    QVector<int> order = laidOutIndexes();

    // 1) 排序：框在前（容量大的在前），图标在后按名字排。
    //    收拢模式（sortByName=false）则按屏幕上的先上下后左右的顺序，尽量保持原样。
    if (sortByName) {
        std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
            const LayoutItem &x = m_items.at(a);
            const LayoutItem &y = m_items.at(b);
            if (x.isGroup() != y.isGroup())
                return x.isGroup();
            if (x.isGroup() && x.capacity() != y.capacity())
                return x.capacity() > y.capacity();
            return QString::localeAwareCompare(x.name, y.name) < 0;
        });
    } else {
        std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
            const LayoutItem &x = m_items.at(a);
            const LayoutItem &y = m_items.at(b);
            if (x.y != y.y)
                return x.y < y.y;
            return x.x < y.x;
        });
    }

    // 2) 按"格子占位"摆放：框占 cols×rows 个格子，图标占 1 格
    QSet<int> used;
    // 列宽上界远小于 1024，乘 1024 只是为了留足余量，避免 (gx,gy) 撞到同一个 key
    auto key = [](int gx, int gy) { return gy * 1024 + gx; };

    for (int index : order) {
        LayoutItem &it = m_items[index];
        const QSize sz = LayoutEngine::itemSize(it);
        const int wc = qMax(1, (sz.width() + gap) / stepX);
        const int hc = qMax(1, (sz.height() + gap) / stepY);

        bool placed = false;
        for (int gy = 0; gy < 200 && !placed; ++gy) {
            for (int gx = 0; gx + wc <= cols; ++gx) {
                bool free = true;
                for (int dy = 0; dy < hc && free; ++dy) {
                    for (int dx = 0; dx < wc && free; ++dx) {
                        if (used.contains(key(gx + dx, gy + dy)))
                            free = false;
                    }
                }
                if (!free)
                    continue;

                it.x = pad + gx * stepX;
                it.y = pad + gy * stepY;
                for (int dy = 0; dy < hc; ++dy)
                    for (int dx = 0; dx < wc; ++dx)
                        used.insert(key(gx + dx, gy + dy));
                placed = true;
                break;
            }
        }
    }

    growCanvasToFit();
    save();
    emit layoutChanged();
    emit changed();
}

bool AppConfig::clearGroup(const QString &id)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup() || group->system)
        return false;

    group->children.clear();
    save();
    emit layoutChanged();
    emit changed();
    return true;
}

bool AppConfig::sortGroup(const QString &id)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup())
        return false;

    std::sort(group->children.begin(), group->children.end(),
              [](const LayoutItem &a, const LayoutItem &b) {
                  return QString::localeAwareCompare(a.name, b.name) < 0;
              });

    save();
    emit layoutChanged();
    emit changed();
    return true;
}

bool AppConfig::moveChildToIndex(const QString &groupId, int from, int to)
{
    LayoutItem *group = findById(groupId);
    if (!group || !group->isGroup())
        return false;

    const int count = group->children.size();
    if (from < 0 || from >= count)
        return false;

    // to 用"目标项原来的下标"表达：先摘出自己，再插到那个位置，
    // 中间那些项整体前移，拖一下就能把图标挪到任意一格。
    to = qBound(0, to, count - 1);
    if (from == to)
        return false;

    const LayoutItem child = group->children.at(from);
    group->children.removeAt(from);
    group->children.insert(to, child);

    save();
    emit layoutChanged();
    emit changed();
    return true;
}

QRect AppConfig::contentBounds() const
{
    // 隐藏的「桌面」框不参与：否则面板会一直留着它那一片空白
    return LayoutEngine::boundsOf(laidOutItems());
}

QSize AppConfig::requiredCanvasSize() const
{
    const QRect b = contentBounds();

    // 面板贴着内容走：东西少的时候不留一大片空白，东西多了自动长大（超高就滚动）。
    // 配置里的 canvas 宽度作为上限，避免默认面板被拉得过宽。
    int w = m_canvas.width();
    int h = m_canvas.height();

    if (!b.isEmpty()) {
        const int needW = b.right() + 24;
        const int needH = b.bottom() + 24;

        w = needW <= m_canvas.width() ? qMax(kMinCanvasWidth, needW) : needW;
        h = qMax(kMinCanvasHeight, needH);
    }

    return QSize(w, h);
}

void AppConfig::growCanvasToFit()
{
    const QRect b = contentBounds();
    if (b.isEmpty())
        return;

    const int needW = b.right() + 20;
    if (needW > m_canvas.width())
        m_canvas.setWidth(qMin(needW, 2400));
}

void AppConfig::setLanguage(const QString &v)
{
    if (m_language == v)
        return;
    m_language = v;
    save();
    emit changed();
}

void AppConfig::setTheme(const QString &v)
{
    if (m_theme == v)
        return;
    m_theme = v;
    save();
    emit changed();
}

void AppConfig::setHotkey(const QString &v)
{
    if (m_hotkey == v)
        return;
    m_hotkey = v;
    save();
    emit changed();
}

void AppConfig::setHotkeyAdd(const QString &v)
{
    if (m_hotkeyAdd == v)
        return;
    m_hotkeyAdd = v;
    save();
    emit changed();
}

void AppConfig::setPythonPath(const QString &v)
{
    if (m_pythonPath == v)
        return;
    m_pythonPath = v;
    save();
    emit changed();
}

void AppConfig::setIconSize(int v)
{
    const int clamped = qBound(24, v, 128);
    if (m_iconSize == clamped)
        return;
    m_iconSize = clamped;
    save();
    emit changed();
}

void AppConfig::hidePath(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(path);

    if (!m_hiddenPaths.contains(normalized))
        m_hiddenPaths.append(normalized);

    for (LayoutItem &group : m_items) {
        for (int i = group.children.size() - 1; i >= 0; --i) {
            if (group.children.at(i).path.compare(normalized, Qt::CaseInsensitive) == 0)
                group.children.removeAt(i);
        }
    }
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!m_items.at(i).isGroup()
            && m_items.at(i).path.compare(normalized, Qt::CaseInsensitive) == 0)
            m_items.removeAt(i);
    }

    save();
    emit layoutChanged();
    emit changed();
}

void AppConfig::setHideDesktopFrame(bool v)
{
    if (m_hideDesktopFrame == v)
        return;
    m_hideDesktopFrame = v;

    // 隐藏后原位置会空出一大片：顺手把显示中的东西摆紧（只收拢，不重新排序），
    // 恢复显示时也一样，免得「桌面」框又压在手边的图标上。
    m_compactHiddenDone = true;
    packItems(false);
}

void AppConfig::notifyChanged()
{
    save();
    emit layoutChanged();
    emit changed();
}
