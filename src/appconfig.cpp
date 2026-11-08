#include "appconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {

bool dirIsWritable(const QString &dir)
{
    QDir d(dir);
    if (!d.exists())
        return false;
    QFile probe(dir + "/.write_probe");
    if (!probe.open(QIODevice::WriteOnly))
        return false;
    probe.close();
    probe.remove();
    return true;
}

QString resolveDataDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    if (dirIsWritable(appDir))
        return appDir;

    QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (fallback.isEmpty())
        fallback = QDir::homePath() + "/.luodeck";
    QDir().mkpath(fallback);
    return fallback;
}

} // namespace

AppConfig &AppConfig::instance()
{
    static AppConfig cfg;
    return cfg;
}

void AppConfig::ensureDirectories()
{
    QDir().mkpath(m_dataDir);
    QDir().mkpath(themesDir());
    QDir().mkpath(localesDir());
    QDir().mkpath(cacheDir());
}

bool AppConfig::load()
{
    m_dataDir = resolveDataDir();
    m_configPath = m_dataDir + "/luodeck.json";
    ensureDirectories();

    QFile f(m_configPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        applyDefaults();
        importFromDesktop();
        save();
        return true;
    }

    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        applyDefaults();
        importFromDesktop();
        save();
        return true;
    }

    const QJsonObject root = doc.object();
    m_language = root.value("language").toString("zh_CN");
    m_theme = root.value("theme").toString("default");
    m_hotkey = root.value("hotkey").toString("ctrl+space");

    const QJsonObject grid = root.value("grid").toObject();
    m_iconSize = grid.value("iconSize").toInt(48);
    m_columns = grid.value("columns").toInt(6);
    if (m_iconSize < 24 || m_iconSize > 128)
        m_iconSize = 48;
    if (m_columns < 3 || m_columns > 20)
        m_columns = 6;

    m_items.clear();
    const QJsonArray arr = root.value("items").toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        TileEntry e;
        e.path = o.value("path").toString();
        if (e.path.isEmpty())
            continue;
        e.id = o.value("id").toString();
        if (e.id.isEmpty())
            e.id = makeId();
        e.name = o.value("name").toString(displayNameFor(e.path));
        e.isFolder = o.value("isFolder").toBool(isFolderPath(e.path));
        e.expanded = false;
        m_items.append(e);
    }

    return true;
}

bool AppConfig::save()
{
    if (m_dataDir.isEmpty())
        ensureDirectories();

    QJsonObject root;
    root.insert("version", 1);
    root.insert("language", m_language);
    root.insert("theme", m_theme);
    root.insert("hotkey", m_hotkey);

    QJsonObject grid;
    grid.insert("iconSize", m_iconSize);
    grid.insert("columns", m_columns);
    root.insert("grid", grid);

    QJsonArray arr;
    for (const TileEntry &e : m_items) {
        QJsonObject o;
        o.insert("id", e.id);
        o.insert("path", e.path);
        o.insert("name", e.name);
        o.insert("isFolder", e.isFolder);
        arr.append(o);
    }
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
    m_columns = 6;
    m_items.clear();
}

void AppConfig::importFromDesktop()
{
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty())
        return;

    QDir dir(desktop);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);

    int added = 0;
    for (const QFileInfo &fi : entries) {
        if (added >= 24)
            break;

        const QString suffix = fi.suffix().toLower();
        const bool folder = fi.isDir();
        const bool interesting = folder
            || suffix == "lnk" || suffix == "url" || suffix == "exe"
            || suffix == "bat" || suffix == "cmd" || suffix == "ps1";
        if (!interesting)
            continue;

        TileEntry e;
        e.id = makeId();
        e.path = QDir::fromNativeSeparators(fi.absoluteFilePath());
        e.name = displayNameFor(e.path);
        e.isFolder = folder;
        m_items.append(e);
        ++added;
    }
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

void AppConfig::addPath(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return;

    const QString normalized = QDir::fromNativeSeparators(fi.absoluteFilePath());
    for (const TileEntry &e : m_items) {
        if (e.path.compare(normalized, Qt::CaseInsensitive) == 0)
            return;
    }

    TileEntry e;
    e.id = makeId();
    e.path = normalized;
    e.name = displayNameFor(normalized);
    e.isFolder = fi.isDir();
    m_items.append(e);
    save();
    emit changed();
}

void AppConfig::removeById(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            m_items.removeAt(i);
            save();
            emit changed();
            return;
        }
    }
}

void AppConfig::moveItem(int from, int to)
{
    if (from < 0 || from >= m_items.size())
        return;
    if (to < 0 || to >= m_items.size() || from == to)
        return;
    m_items.move(from, to);
    save();
    emit changed();
}

QString AppConfig::makeId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString AppConfig::displayNameFor(const QString &path)
{
    QFileInfo fi(path);
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
