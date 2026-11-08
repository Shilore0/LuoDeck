#include "i18n.h"

#include "appconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

const char *kZhCN = R"JSON({
  "app.name": "LuoDeck",
  "group.open": "打开",
  "group.settings": "文件夹设置…",
  "group.desktop": "桌面",
  "group.rename": "重命名",
  "group.rename_prompt": "输入新的名称：",
  "group.capacity": "显示容量",
  "group.delete": "删除这个框",
  "group.new": "新建文件夹",
  "folder.settings": "文件夹设置",
  "folder.name": "名称",
  "folder.color": "边框颜色",
  "folder.system_note": "桌面文件夹为系统自创建，无法自定义，可以在软件设置中隐藏「桌面」文件夹。",
  "item.remove": "从面板移除",
  "item.reveal": "打开所在位置",
  "canvas.refresh": "刷新布局",
  "tray.show": "显示面板",
  "tray.settings": "设置…",
  "tray.reload": "重新加载图标",
  "tray.config_dir": "打开配置目录",
  "tray.quit": "退出",
  "tray.hotkey_failed": "全局热键注册失败：%1。可能被输入法或其他软件占用，请打开「设置」修改热键。",
  "tray.hotkey_ok": "已注册热键 %1",
  "settings.title": "LuoDeck 设置",
  "settings.theme": "主题",
  "settings.language": "语言",
  "settings.hotkey": "全局热键",
  "settings.hotkey_note": "格式如 ctrl+space、ctrl+alt+d、win+f1。中文系统下 Ctrl+Space 常被输入法占用，注册失败会自动改用 Ctrl+Alt+Space。",
  "settings.add_hotkey": "加入选中项热键",
  "settings.python": "Python 解释器",
  "settings.python_auto": "留空则自动查找",
  "settings.python_note": "双击 .py 时用它启动。留空会自动在 PATH 中找 pyw.exe / pythonw.exe（安装 Python 时通常自带 pyw.exe，不依赖你手动配置环境变量）。",
  "settings.browse": "浏览…",
  "hotkey.no_selection": "没有检测到选中的文件。请先在资源管理器里选中文件或文件夹，再按这个热键。",
  "hotkey.added": "已加入 %1 项到面板",
  "hotkey.add_failed": "「加入选中项」热键 %1 注册失败，可能被其他软件占用。",
  "launch.failed": "无法启动 %1。文件可能已不存在，或缺少运行环境（例如没有安装 Python）。",
  "settings.open_dir": "打开配置目录",
  "settings.shell_install": "添加到右键菜单",
  "settings.shell_remove": "移除右键菜单",
  "settings.shell_note": "在文件或文件夹上右键即可「添加到 LuoDeck」，会加到面板顶层而不是某个文件夹里。Win11 需要点「显示更多选项」才能看到。",
  "settings.classic_menu": "右键菜单用经典样式（Win11）",
  "settings.classic_menu_note": "Win11 会把第三方菜单项收进「显示更多选项」。勾选后右键菜单恢复成 Win10 的长列表样式，LuoDeck 的按钮就和其他项一起直接显示。改完需要重启资源管理器或重启电脑才生效；不勾选则恢复 Win11 新样式。",
  "settings.restart_explorer": "重启资源管理器",
  "settings.hide_desktop": "隐藏「桌面」文件夹",
  "settings.hide_desktop_note": "勾选后，桌面上的图标不会出现在面板中；配置与快捷方式仍保留在 LuoDeck 目录里，取消勾选即可恢复。",
  "settings.ok": "确定",
  "settings.cancel": "取消"
})JSON";

const char *kEnUS = R"JSON({
  "app.name": "LuoDeck",
  "group.open": "Open",
  "group.settings": "Folder settings...",
  "group.desktop": "Desktop",
  "group.rename": "Rename",
  "group.rename_prompt": "Enter a new name:",
  "group.capacity": "Capacity",
  "group.delete": "Delete this frame",
  "group.new": "New folder",
  "folder.settings": "Folder settings",
  "folder.name": "Name",
  "folder.color": "Border color",
  "folder.system_note": "The Desktop folder is created by the system and cannot be customized. You can hide it in Settings.",
  "item.remove": "Remove from panel",
  "item.reveal": "Open file location",
  "canvas.refresh": "Refresh layout",
  "tray.show": "Show panel",
  "tray.settings": "Settings...",
  "tray.reload": "Reload icons",
  "tray.config_dir": "Open config folder",
  "tray.quit": "Quit",
  "tray.hotkey_failed": "Failed to register hotkey: %1. It may be taken by the IME or another app. Change it in Settings.",
  "tray.hotkey_ok": "Hotkey %1 registered",
  "settings.title": "LuoDeck Settings",
  "settings.theme": "Theme",
  "settings.language": "Language",
  "settings.hotkey": "Global hotkey",
  "settings.hotkey_note": "Format: ctrl+space, ctrl+alt+d, win+f1. On Chinese systems Ctrl+Space is often taken by the IME; LuoDeck falls back to Ctrl+Alt+Space.",
  "settings.add_hotkey": "Add-selection hotkey",
  "settings.python": "Python interpreter",
  "settings.python_auto": "Auto-detect if empty",
  "settings.python_note": "Used to launch .py files. Leave empty to auto-detect pyw.exe / pythonw.exe from PATH (installing Python normally provides pyw.exe, no manual env setup needed).",
  "settings.browse": "Browse...",
  "hotkey.no_selection": "No selected files found. Select files or folders in Explorer first, then press the hotkey.",
  "hotkey.added": "Added %1 item(s) to the panel",
  "hotkey.add_failed": "Failed to register the add-selection hotkey %1; it may be taken by another app.",
  "launch.failed": "Failed to launch %1. The file may be missing, or its runtime is unavailable (for example Python is not installed).",
  "settings.open_dir": "Open config folder",
  "settings.shell_install": "Add to context menu",
  "settings.shell_remove": "Remove from context menu",
  "settings.shell_note": "Right-click any file or folder and pick \"Add to LuoDeck\"; it is added to the panel root, not into a folder. On Windows 11 it sits under \"Show more options\".",
  "settings.classic_menu": "Use classic context menu (Windows 11)",
  "settings.classic_menu_note": "Windows 11 tucks third-party entries under \"Show more options\". Enabling this restores the classic Windows 10 context menu, so the LuoDeck entry appears directly. Restart Explorer or reboot to apply; uncheck to go back.",
  "settings.restart_explorer": "Restart Explorer",
  "settings.hide_desktop": "Hide the Desktop folder",
  "settings.hide_desktop_note": "When checked, desktop icons are not shown in the panel. Config and shortcuts stay in the LuoDeck folder; uncheck to restore.",
  "settings.ok": "OK",
  "settings.cancel": "Cancel"
})JSON";

} // namespace

I18n &I18n::instance()
{
    static I18n i;
    return i;
}

void I18n::writeIfMissing(const QString &fileName, const QByteArray &builtin)
{
    const QString path = m_dir + "/" + fileName;
    const QJsonObject builtinObj = QJsonDocument::fromJson(builtin).object();

    if (!QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(builtin);
        return;
    }

    // 文件已存在：只补齐缺失的键，保留用户的翻译
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly))
        return;
    const QByteArray raw = in.readAll();
    in.close();

    QJsonObject existing = QJsonDocument::fromJson(raw).object();
    bool changed = false;
    for (auto it = builtinObj.constBegin(); it != builtinObj.constEnd(); ++it) {
        if (!existing.contains(it.key())) {
            existing.insert(it.key(), it.value());
            changed = true;
        }
    }
    if (!changed)
        return;

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    out.write(QJsonDocument(existing).toJson(QJsonDocument::Indented));
}

void I18n::ensureDefaults()
{
    m_dir = AppConfig::instance().localesDir();
    QDir().mkpath(m_dir);
    writeIfMissing("zh_CN.json", QByteArray(kZhCN));
    writeIfMissing("en_US.json", QByteArray(kEnUS));
}

QStringList I18n::available() const
{
    QStringList out;
    QDir dir(m_dir);
    const QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
    for (const QString &f : files)
        out << QFileInfo(f).completeBaseName();
    if (out.isEmpty())
        out << "zh_CN";
    return out;
}

void I18n::load(const QString &langCode)
{
    ensureDefaults();

    m_dir = AppConfig::instance().localesDir();
    QString target = langCode.isEmpty() ? QStringLiteral("zh_CN") : langCode;

    QString path = m_dir + "/" + target + ".json";
    if (!QFile::exists(path))
        path = m_dir + "/zh_CN.json";

    QFile f(path);
    m_dict.clear();
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isObject()) {
            const QJsonObject o = doc.object();
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                m_dict.insert(it.key(), it.value().toString());
        }
    }

    m_current = target;
    emit languageChanged(m_current);
}

QString I18n::t(const QString &key) const
{
    return m_dict.value(key, key);
}

QString I18n::t(const QString &key, const QString &arg1) const
{
    return QString(m_dict.value(key, key)).replace("%1", arg1);
}
