#include "appconfig.h"

#include "metrics.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRect>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

namespace {

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

    if (it.isGroup()) {
        o.insert("cols", it.cols);
        o.insert("rows", it.rows);
        if (!it.color.isEmpty())
            o.insert("color", it.color);
        QJsonArray kids;
        for (const LayoutItem &c : it.children)
            kids.append(itemToJson(c));
        o.insert("children", kids);
    } else {
        o.insert("path", it.path);
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
    it.system = o.value("system").toBool(false);
    it.x = o.value("x").toInt(0);
    it.y = o.value("y").toInt(0);
    it.path = o.value("path").toString();
    it.cols = qBound(1, o.value("cols").toInt(3), 8);
    it.rows = qBound(1, o.value("rows").toInt(3), 8);

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
    if (s.isEmpty())
        s = QStringLiteral("folder");
    return s;
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
    QDir().mkpath(groupDirFor(groupName));
}

void AppConfig::mirrorShortcut(const QString &groupName, const QString &sourcePath)
{
    ensureGroupDir(groupName);

    const QFileInfo src(sourcePath);
    if (!src.exists())
        return;

    const QString dirPath = groupDirFor(groupName);
    const QString target = dirPath + "/" + displayNameFor(sourcePath) + ".lnk";
    if (QFile::exists(target))
        return;

    // Windows 上 QFile::link 会创建真正的 .lnk 快捷方式
    QFile::link(src.absoluteFilePath(), target);
}

void AppConfig::removeGroupDir(const QString &groupName)
{
    const QString path = groupDirFor(groupName);
    if (!path.startsWith(groupsDir()))
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
        save();
        return true;
    }

    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        applyDefaults();
        ensureDesktopFrame();
        syncDesktopFrame();
        save();
        return true;
    }

    const QJsonObject root = doc.object();
    m_language = root.value("language").toString("zh_CN");
    m_theme = root.value("theme").toString("default");
    m_hotkey = root.value("hotkey").toString("ctrl+space");
    m_iconSize = qBound(24, root.value("iconSize").toInt(48), 128);
    m_hideDesktopFrame = root.value("hideDesktopFrame").toBool(false);

    const QJsonObject canvas = root.value("canvas").toObject();
    m_canvas = QSize(qBound(480, canvas.value("width").toInt(720), 2400),
                     qBound(360, canvas.value("height").toInt(520), 1600));

    m_items.clear();
    const QJsonArray arr = root.value("items").toArray();
    for (const QJsonValue &v : arr) {
        LayoutItem it = itemFromJson(v.toObject());
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
    root.insert("iconSize", m_iconSize);
    root.insert("hideDesktopFrame", m_hideDesktopFrame);

    QJsonObject canvas;
    canvas.insert("width", m_canvas.width());
    canvas.insert("height", m_canvas.height());
    root.insert("canvas", canvas);

    QJsonArray arr;
    for (const LayoutItem &it : m_items)
        arr.append(itemToJson(it));
    root.insert("items", arr);

    QSaveFile f(m_configPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

void AppConfig::applyDefaults()
{
    m_language = "zh_CN";
    m_theme = "default";
    m_hotkey = "ctrl+space";
    m_iconSize = 48;
    m_canvas = QSize(720, 520);
    m_hideDesktopFrame = false;
    m_items.clear();
}

void AppConfig::ensureDesktopFrame()
{
    for (const LayoutItem &it : m_items) {
        if (it.isGroup() && it.system)
            return;
    }

    LayoutItem frame;
    frame.id = makeId();
    frame.type = "group";
    frame.name = QStringLiteral("桌面");
    frame.system = true;
    frame.color = defaultDesktopColor();
    frame.cols = 4;
    frame.rows = 4;
    frame.x = 20;
    frame.y = 20;

    m_items.prepend(frame);

    // 让已有分组避开桌面框，避免首次升级后两个框叠在一起
    const int frameW = frame.cols * (Metrics::kGroupCellW + 2) + 2 * Metrics::kGroupPad;
    const int frameH = frame.rows * (Metrics::kGroupCellH + 2) + 2 * Metrics::kGroupPad
        + Metrics::kGroupTitleH;
    const QRect frameRect(frame.x, frame.y, frameW, frameH);

    for (LayoutItem &it : m_items) {
        if (!it.isGroup() || it.system)
            continue;
        const int w = it.cols * (Metrics::kGroupCellW + 2) + 2 * Metrics::kGroupPad;
        const int h = it.rows * (Metrics::kGroupCellH + 2) + 2 * Metrics::kGroupPad
            + Metrics::kGroupTitleH;
        const QRect r(it.x, it.y, w, h);
        if (!r.intersects(frameRect))
            continue;

        int nx = frameRect.right() + 24;
        if (nx + w > m_canvas.width() - 10)
            nx = 20;
        it.x = nx;
        if (it.y < frameRect.bottom() + 16 && nx == 20)
            it.y = frameRect.bottom() + 16;
    }
}

void AppConfig::syncDesktopFrame()
{
    LayoutItem *frame = nullptr;
    for (LayoutItem &it : m_items) {
        if (it.isGroup() && it.system) {
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
        LayoutItem child;
        child.id = makeId();
        child.type = "app";
        child.path = path;
        child.name = displayNameFor(path);
        kept.append(child);
        mirrorShortcut(frame->name, path);
    }

    frame->children = kept;
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
    return const_cast<AppConfig *>(this)->findById(id);
}

QString AppConfig::desktopFrameId() const
{
    for (const LayoutItem &it : m_items) {
        if (it.isGroup() && it.system)
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

    LayoutItem item;
    item.id = makeId();
    item.type = "app";
    item.path = normalized;
    item.name = displayNameFor(normalized);

    if (!groupId.isEmpty()) {
        LayoutItem *group = findById(groupId);
        if (group && group->isGroup()) {
            group->children.append(item);
            mirrorShortcut(group->name, normalized);
            save();
            emit layoutChanged();
            emit changed();
            return item.id;
        }
    }

    const int perRow = qMax(3, (m_canvas.width() - 40) / 68);
    const int index = m_items.size() - 1;
    item.x = 20 + (qMax(0, index) % perRow) * 68;
    item.y = 20 + ((qMax(0, index) / perRow) % 5) * 76;

    m_items.append(item);
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
    group.cols = 3;
    group.rows = 3;
    group.x = 40 + (m_items.size() % 4) * 44;
    group.y = 40 + (m_items.size() % 4) * 44;

    m_items.append(group);
    ensureGroupDir(group.name);
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
                parent.children.removeAt(j);
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

    item->x = qMax(0, pos.x());
    item->y = qMax(0, pos.y());

    // 只落盘，不发 layoutChanged：位置变了但结构没变，
    // 广播会让画布销毁重建所有控件，松手时肉眼可见地卡一下
    save();
}

bool AppConfig::setGroupGrid(const QString &id, int cols, int rows)
{
    LayoutItem *group = findById(id);
    if (!group || !group->isGroup())
        return false;

    group->cols = qBound(1, cols, 8);
    group->rows = qBound(1, rows, 8);

    save();
    emit layoutChanged();
    emit changed();
    return true;
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

void AppConfig::setIconSize(int v)
{
    const int clamped = qBound(24, v, 128);
    if (m_iconSize == clamped)
        return;
    m_iconSize = clamped;
    save();
    emit changed();
}

void AppConfig::setHideDesktopFrame(bool v)
{
    if (m_hideDesktopFrame == v)
        return;
    m_hideDesktopFrame = v;
    save();
    emit layoutChanged();
    emit changed();
}

void AppConfig::notifyChanged()
{
    save();
    emit layoutChanged();
    emit changed();
}
