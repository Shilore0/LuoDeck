#include "theme.h"

#include "appconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

const char *kDefaultLight = R"JSON({
  "name": "Default",
  "radius": 12,
  "spacing": 8,
  "colors": {
    "window": "#F5F6F8F2",
    "surface": "#FFFFFF",
    "surfaceHover": "#E9ECF1",
    "text": "#1F2328",
    "textMuted": "#5A6169",
    "accent": "#2F6FED",
    "border": "#DDE1E6"
  }
})JSON";

const char *kDefaultDark = R"JSON({
  "name": "Dark",
  "radius": 12,
  "spacing": 8,
  "colors": {
    "window": "#17181CF2",
    "surface": "#1E1F24",
    "surfaceHover": "#2B2D33",
    "text": "#E8EAED",
    "textMuted": "#9AA0A6",
    "accent": "#7AA2F7",
    "border": "#33363D"
  }
})JSON";

QString rgba(const QColor &c)
{
    return QString("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

QColor parseColor(const QJsonObject &o, const QString &key, const QColor &fallback)
{
    const QString s = o.value(key).toString();
    if (s.isEmpty())
        return fallback;
    const QColor c(s);
    return c.isValid() ? c : fallback;
}

} // namespace

Theme &Theme::instance()
{
    static Theme t;
    return t;
}

void Theme::writeIfMissing(const QString &fileName, const QByteArray &content)
{
    const QString path = m_dir + "/" + fileName;
    if (QFile::exists(path))
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(content);
    f.close();
}

void Theme::ensureDefaults()
{
    m_dir = AppConfig::instance().themesDir();
    QDir().mkpath(m_dir);
    writeIfMissing("default.json", QByteArray(kDefaultLight));
    writeIfMissing("dark.json", QByteArray(kDefaultDark));
    refreshAvailable();
}

void Theme::refreshAvailable()
{
    m_available.clear();
    QDir dir(m_dir);
    const QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
    for (const QString &f : files)
        m_available << QFileInfo(f).completeBaseName();
    if (m_available.isEmpty())
        m_available << "default";
}

bool Theme::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    m_radius = root.value("radius").toInt(m_radius);
    m_spacing = root.value("spacing").toInt(m_spacing);
    m_iconSize = root.value("iconSize").toInt(m_iconSize);
    m_columns = root.value("columns").toInt(m_columns);

    const QJsonObject c = root.value("colors").toObject();
    m_window = parseColor(c, "window", m_window);
    m_surface = parseColor(c, "surface", m_surface);
    m_surfaceHover = parseColor(c, "surfaceHover", m_surfaceHover);
    m_text = parseColor(c, "text", m_text);
    m_textMuted = parseColor(c, "textMuted", m_textMuted);
    m_accent = parseColor(c, "accent", m_accent);
    m_border = parseColor(c, "border", m_border);
    return true;
}

void Theme::load(const QString &name)
{
    ensureDefaults();

    QString target = name.isEmpty() ? QStringLiteral("default") : name;
    if (!readFile(m_dir + "/" + target + ".json")) {
        target = "default";
        if (!readFile(m_dir + "/default.json"))
            return;
    }

    m_name = target;
    emit themeChanged();
}

QColor Theme::color(const QString &key) const
{
    if (key == "window") return m_window;
    if (key == "surface") return m_surface;
    if (key == "surfaceHover") return m_surfaceHover;
    if (key == "text") return m_text;
    if (key == "textMuted") return m_textMuted;
    if (key == "accent") return m_accent;
    if (key == "border") return m_border;
    return m_text;
}

QString Theme::styleSheet() const
{
    QString qss;
    qss += QString("QWidget { color: %1; font-family: \"Microsoft YaHei UI\", \"Segoe UI\"; font-size: 13px; }")
               .arg(m_text.name(QColor::HexRgb));
    qss += "QListView#TileView { background: transparent; border: none; outline: none; }";
    qss += "QListView#TileView::item { border: none; }";

    qss += QString("QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
                   "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 28px; }"
                   "QScrollBar::handle:vertical:hover { background: %2; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
               .arg(rgba(QColor(m_border.red(), m_border.green(), m_border.blue(), 160)),
                    rgba(QColor(m_textMuted.red(), m_textMuted.green(), m_textMuted.blue(), 170)));

    qss += QString("QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 5px; }")
               .arg(m_surface.name(QColor::HexRgb), m_border.name(QColor::HexRgb));
    qss += QString("QMenu::item { padding: 7px 26px 7px 14px; border-radius: 5px; color: %1; }")
               .arg(m_text.name(QColor::HexRgb));
    qss += QString("QMenu::item:selected { background: %1; }").arg(m_surfaceHover.name(QColor::HexRgb));
    qss += QString("QMenu::separator { height: 1px; background: %1; margin: 5px 8px; }")
               .arg(m_border.name(QColor::HexRgb));

    qss += QString("QLabel#EmptyHint { color: %1; }").arg(m_textMuted.name(QColor::HexRgb));
    return qss;
}
