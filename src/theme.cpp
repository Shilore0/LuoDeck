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
    const QString s = o.value(key).toString().trimmed();
    if (s.isEmpty())
        return fallback;

    // 主题文件里写的是 CSS 风格的 #RRGGBBAA（透明度在最后），
    // 而 QColor 把 8 位十六进制当成 #AARRGGBB 解析 —— 直接丢给它，
    // "#17181CF2" 会被读成「α=0x17 + 蓝色 #181CF2」，深色主题就变成一片蓝。
    if (s.size() == 9 && s.startsWith(QLatin1Char('#'))) {
        bool ok = false;
        const uint v = s.mid(1).toUInt(&ok, 16);
        if (ok) {
            return QColor(int((v >> 24) & 0xff), int((v >> 16) & 0xff), int((v >> 8) & 0xff),
                          int(v & 0xff));
        }
    }

    const QColor c(s);
    return c.isValid() ? c : fallback;
}

// 主题名只允许是 themes 目录下的文件名，挡住 config.json 手改 "../../xxx" 的可能
bool isValidThemeName(const QString &name)
{
    if (name.isEmpty())
        return false;
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))
        || name.contains(QStringLiteral("..")))
        return false;
    return true;
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
    // 边界保护：主题文件可以随便写，别让 radius/spacing 变成 0 或负数把绘制搞坏
    m_radius = qBound(0, root.value("radius").toInt(m_radius), 40);
    m_spacing = qBound(0, root.value("spacing").toInt(m_spacing), 40);
    m_iconSize = qBound(24, root.value("iconSize").toInt(m_iconSize), 128);
    m_columns = qBound(1, root.value("columns").toInt(m_columns), 12);

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
    if (!isValidThemeName(target))
        target = QStringLiteral("default");

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
    const QString text = m_text.name(QColor::HexRgb);
    const QString muted = m_textMuted.name(QColor::HexRgb);
    const QString surface = m_surface.name(QColor::HexRgb);
    const QString hover = m_surfaceHover.name(QColor::HexRgb);
    const QString border = m_border.name(QColor::HexRgb);
    const QString accent = m_accent.name(QColor::HexRgb);

    QString qss;
    qss += QString("QWidget { color: %1; font-family: \"Microsoft YaHei UI\", \"Segoe UI\";"
                   " font-size: 13px; }")
               .arg(text);

    // 自绘控件不受样式表影响，避免出现半透明重影
    qss += "MainWindow, CanvasWidget, GroupItemWidget, AppItemWidget, IconTile,"
           " QListView#TileView { background: transparent; border: none; outline: none; }";
    qss += "QLabel { background: transparent; }";

    // 对话框：必须有明确底色，否则深色系统主题下会出现黑底黑字
    qss += QString("QDialog { background-color: %1; }").arg(surface);
    qss += QString("QDialog QLabel { color: %1; }").arg(text);

    // 输入类控件
    qss += QString("QLineEdit, QComboBox, QSpinBox, QPlainTextEdit, QTextEdit"
                   " { background: %1; color: %2;"
                   " border: 1px solid %3; border-radius: 6px; padding: 5px 8px;"
                   " min-height: 20px; selection-background-color: %4; }")
               .arg(surface, text, border, accent);
    qss += QString("QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus"
                   " { border: 1px solid %1; }")
               .arg(accent);
    qss += QString("QLineEdit:disabled, QComboBox:disabled { color: %1; background: %2; }")
               .arg(muted, hover);
    qss += QString("QComboBox QAbstractItemView { background: %1; color: %2;"
                   " border: 1px solid %3; selection-background-color: %4; outline: none; }")
               .arg(surface, text, border, accent);

    // 按钮
    qss += QString("QPushButton { background: %1; color: %2; border: none;"
                   " border-radius: 6px; padding: 6px 16px; min-height: 20px; }")
               .arg(hover, text);
    qss += QString("QPushButton:hover { background: %1; }").arg(border);
    qss += QString("QPushButton:pressed { background: %1; color: %2; }").arg(accent, surface);
    qss += QString("QPushButton:disabled { color: %1; background: %2; }").arg(muted, hover);

    // 复选框
    qss += QString("QCheckBox { color: %1; spacing: 8px; }").arg(text);
    qss += QString("QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %1;"
                   " border-radius: 4px; background: %2; }")
               .arg(border, surface);
    qss += QString("QCheckBox::indicator:checked { background: %1; border-color: %1; }").arg(accent);

    // 滚动条
    qss += QString("QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
                   "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 28px; }"
                   "QScrollBar::handle:vertical:hover { background: %2; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
               .arg(rgba(QColor(m_border.red(), m_border.green(), m_border.blue(), 160)),
                    rgba(QColor(m_textMuted.red(), m_textMuted.green(), m_textMuted.blue(), 170)));

    // 列表 / 表格 / 树：这些控件默认走系统调色板，在深色主题下就是"黑字黑底"，
    // 必须显式给底色和字色（之前选择应用、下拉列表都糊在这一条上）
    qss += QString("QListWidget, QListView, QTreeView, QTableView, QTableWidget"
                   " { background: %1; color: %2; border: 1px solid %3;"
                   " border-radius: 8px; outline: none; alternate-background-color: %4; }")
               .arg(surface, text, border, hover);
    qss += QString("QListWidget::item, QListView::item, QTreeView::item, QTableView::item"
                   " { color: %1; padding: 3px; border-radius: 6px; }")
               .arg(text);
    qss += QString("QListWidget::item:hover, QListView::item:hover, QTreeView::item:hover"
                   " { background: %1; }")
               .arg(hover);
    qss += QString("QListWidget::item:selected, QListView::item:selected,"
                   " QTreeView::item:selected, QTableView::item:selected"
                   " { background: %1; color: %2; }")
               .arg(accent, surface);
    qss += QString("QHeaderView::section { background: %1; color: %2; border: none;"
                   " border-bottom: 1px solid %3; padding: 5px 8px; }")
               .arg(hover, text, border);

    // 消息框 / 输入框这类"系统壳"也统一一下，免得白底黑字或者字看不清
    qss += QString("QMessageBox, QInputDialog { background-color: %1; }").arg(surface);
    qss += QString("QMessageBox QLabel, QInputDialog QLabel { color: %1; }").arg(text);

    // 悬浮提示
    qss += QString("QToolTip { background: %1; color: %2; border: 1px solid %3;"
                   " border-radius: 6px; padding: 5px 8px; }")
               .arg(hover, text, border);

    // 分组框 / 单选 / 进度条
    qss += QString("QGroupBox { color: %1; border: 1px solid %2; border-radius: 8px;"
                   " margin-top: 10px; padding-top: 6px; }")
               .arg(text, border);
    qss += QString("QGroupBox::title { subcontrol-origin: margin; left: 10px;"
                   " padding: 0 4px; color: %1; }")
               .arg(muted);
    qss += QString("QRadioButton { color: %1; spacing: 8px; }").arg(text);
    qss += QString("QProgressBar { background: %1; color: %2; border: 1px solid %3;"
                   " border-radius: 6px; text-align: center; min-height: 16px; }")
               .arg(surface, text, border);
    qss += QString("QProgressBar::chunk { background: %1; border-radius: 5px; }").arg(accent);

    // 横向滚动条（和竖向一致）
    qss += QString("QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }"
                   "QScrollBar::handle:horizontal { background: %1; border-radius: 3px; min-width: 28px; }"
                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
                   "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }")
               .arg(rgba(QColor(m_border.red(), m_border.green(), m_border.blue(), 160)));

    // 菜单
    qss += QString("QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 5px; }")
               .arg(surface, border);
    qss += QString("QMenu::item { padding: 7px 26px 7px 14px; border-radius: 5px; color: %1; }").arg(text);
    qss += QString("QMenu::item:selected { background: %1; }").arg(hover);
    qss += QString("QMenu::item:disabled { color: %1; }").arg(muted);
    qss += QString("QMenu::separator { height: 1px; background: %1; margin: 5px 8px; }").arg(border);

    return qss;
}
