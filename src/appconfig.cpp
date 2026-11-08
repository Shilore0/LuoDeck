#include "appconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

    if (it.isGroup()) {
        o.insert("cols", it.cols);
        o.insert("rows", it.rows);
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
    QString base = displayNameFor(sourcePath);
    QString target = dirPath + "/" + base + ".lnk";

    int n = 2;
    while (QFile::exists(target)) {
        const QString candidate = dirPath + "/" + base + " (" + QString::number(n) + ").lnk";
        if (QFileInfo(candidate).canonicalFilePath() == src.canonicalFilePath())
            return;
        target = candidate;
        ++n;
        if (n > 99)
            return;
    }

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
    if (!QDir(from).exists())
        return;
    if (QDir(to).exists())
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
        seedFromDesktop();
        save();
        return true;
    }

    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        applyDefaults();
        seedFromDesktop();
        save();
        return true;
    }

    const QJsonObject root = doc.object();
    m_language = root.value("language").toString("zh_CN");
    m_theme = root.value("theme").toString("default");
    m_hotkey = root.value("hotkey").toString("ctrl+space");
    m_iconSize = qBound(24, root.value("iconSize").toInt(48), 128);

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
        m_items.append(it);
    }

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
    m_items.clear();
}

void AppConfig::seedFromDesktop()
{
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty())
        return;

    QDir dir(desktop);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    QVector<QString> paths;
    for (const QFileInfo &fi : entries) {
        if (!isInteresting(fi))
            continue;
        paths.append(QDir::fromNativeSeparators(fi.absoluteFilePath()));
        if (paths.size() >= 24)
            break;
    }

    if (paths.isEmpty())
        return;

    // 首个分组：装前 9 项，让用户一打开就看到"框"的效果
    LayoutItem group;
    group.id = makeId();
    group.type = "group";
    group.name = QStringLiteral("文件夹1");
    group.cols = 3;
    group.rows = 3;
    group.x = 20;
    group.y = 20;

    int consumed = 0;
    for (const QString &p : paths) {
        if (consumed >= 9)
            break;
        LayoutItem child;
        child.id = makeId();
        child.type = "app";
        child.path = p;
        child.name = displayNameFor(p);
        group.children.append(child);
        ++consumed;
    }
    m_items.append(group);

    // 其余平铺在右侧
    int col = 0;
    int row = 0;
    const int startX = 20 + 3 * 62 + 40;
    for (int i = consumed; i < paths.size(); ++i) {
        LayoutItem it;
        it.id = makeId();
        it.type = "app";
        it.path = paths.at(i);
        it.name = displayNameFor(paths.at(i));
        it.x = startX + col * 68;
        it.y = 24 + row * 76;
        ++col;
        if (it.x + 68 > m_canvas.width() - 20) {
            ++row;
            col = 0;
            it.x = startX;
            it.y = 24 + row * 76;
        }
        m_items.append(it);
    }

    // 把分组里的快捷方式镜像到 groups/文件夹1/
    for (const LayoutItem &c : group.children)
        mirrorShortcut(group.name, c.path);
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

LayoutItem *AppConfig::findParentOf(const QString &id)
{
    for (LayoutItem &it : m_items) {
        for (const LayoutItem &c : it.children) {
            if (c.id == id)
                return &it;
        }
    }
    return nullptr;
}

QString AppConfig::suggestGroupName() const
{
    int index = 1;
    QSet<QString> used;
    for (const LayoutItem &it : m_items) {
        if (it.isGroup())
            used.insert(it.name);
    }
    while (used.contains(QStringLiteral("文件夹%1").arg(index)))
        ++index;
    return QStringLiteral("文件夹%1").arg(index);
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
    const int index = m_items.size();
    item.x = 20 + (index % perRow) * 68;
    item.y = 20 + ((index / perRow) % 5) * 76;

    m_items.append(item);
    save();
    emit layoutChanged();
    emit changed();
    return item.id;
}

QString AppConfig::addGroup(const QString &name)
{
    LayoutItem group;
    group.id = makeId();
    group.type = "group";
    group.name = name.isEmpty() ? suggestGroupName() : name;
    group.cols = 3;
    group.rows = 3;
    group.x = 40 + (m_items.size() % 3) * 40;
    group.y = 40 + (m_items.size() % 3) * 40;

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
        if (m_items.at(i).id == id) {
            if (m_items.at(i).isGroup())
                removeGroupDir(m_items.at(i).name);
            m_items.removeAt(i);
            save();
            emit layoutChanged();
            emit changed();
            return;
        }
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
    if (!group || !group->isGroup())
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

void AppConfig::moveItem(const QString &id, const QPoint &pos)
{
    LayoutItem *item = findById(id);
    if (!item)
        return;

    item->x = qMax(0, pos.x());
    item->y = qMax(0, pos.y());

    save();
    emit layoutChanged();
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

bool AppConfig::detachFromGroup(const QString &id, const QPoint &pos)
{
    for (LayoutItem &parent : m_items) {
        for (int j = 0; j < parent.children.size(); ++j) {
            if (parent.children.at(j).id != id)
                continue;

            LayoutItem child = parent.children.takeAt(j);
            child.x = qMax(0, pos.x());
            child.y = qMax(0, pos.y());
            m_items.append(child);

            save();
            emit layoutChanged();
            emit changed();
            return true;
        }
    }
    return false;
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

void AppConfig::notifyChanged()
{
    save();
    emit layoutChanged();
    emit changed();
}
