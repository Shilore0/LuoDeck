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

  "firstrun.startmenu": "要不要把开始菜单里已固定的程序也放进来？",
  "firstrun.startmenu_note": "会作为零散图标铺在面板上（不放进框里），之后随开始菜单自动同步。不想要的右键删掉即可，删过的不再自动出现。",
  "firstrun.yes": "复制过来",
  "firstrun.no": "先不用",
  "tray.startmenu": "导入开始菜单固定项",

  "group.startmenu": "开始菜单",
  "group.startmenu_empty_hint": "开始菜单里的快捷方式会自动出现在这里",
  "popup.search": "搜索…",
  "popup.empty": "这个框还是空的",
  "popup.no_match": "没有匹配的项",
  "group.open": "打开",
  "group.settings": "文件夹设置…",
  "group.desktop": "桌面",
  "group.rename": "重命名",
  "group.rename_prompt": "输入新的名称：",
  "group.capacity": "显示容量",
  "group.capacity_auto": "自动（按图标数量）",
  "group.delete": "删除这个框",
  "group.new": "新建文件夹",
  "folder.settings": "文件夹设置",
  "folder.name": "名称",
  "folder.color": "边框颜色",
  "folder.system_note": "桌面文件夹为系统自创建，无法自定义，可以在软件设置中隐藏「桌面」文件夹。",
  "item.remove": "从面板移除",
  "item.reveal": "打开所在位置",
  "item.open": "打开",
  "item.rename": "重命名…",
  "item.rename_prompt": "输入显示名称：",
  "item.args": "启动参数…",
  "item.args_prompt": "原样接在程序后面（留空就是直接打开）。\n例：-server 127.0.0.1:8080 或 \"我的 存档\" \n快捷方式会先解析出目标程序再加这些参数；.bat/.cmd/.ps1/.py 同样支持。",
  "args.label": "启动参数",
  "args.hint": "原样接在程序后面启动，留空表示直接打开。带空格的参数请用英文引号包起来。支持 .exe / .bat / .cmd / .ps1 / .py；快捷方式会先解析出目标程序再加这些参数。",
  "args.target": "指向：%1",
  "args.no_target": "指向：（解析失败，目标可能已不存在）",
  "item.hide_desktop": "从面板隐藏（不再自动出现）",
  "group.add_file": "添加文件到此框…",
  "group.sort": "按名称排序",
  "group.dissolve": "解散文件夹（图标放回面板）",
  "group.compact": "收成小文件夹",
  "group.expand": "展开为大框",
  "group.clear": "清空这个框",
  "group.empty_hint": "把文件拖进来，或右键添加",
  "group.desktop_empty_hint": "桌面上的图标会自动出现在这里",
  "canvas.add_file": "添加文件…",
  "canvas.add_folder": "添加文件夹…",
  "canvas.add_winapp": "添加 Windows 应用…",
  "winapp.title": "添加 Windows 应用",
  "winapp.hint": "这里是 Windows 商店 / UWP 应用（它们没有可以拖的文件路径，只能用这种方式加）。列表里的名字就是应用本地化后的名字，双击直接添加，也可以按住 Ctrl 多选。",
  "winapp.search": "搜索应用…",
  "winapp.add": "添加",
  "canvas.arrange": "整理布局",
  "canvas.arrange_f5": "整理布局（F5）",
  "canvas.compact": "收拢空位（保持顺序）",
  "canvas.empty": "把文件或快捷方式拖到这里，或右键 → 添加文件",
  "canvas.file_filter": "程序与快捷方式 (*.exe *.lnk *.url *.bat *.cmd *.ps1 *.py);;所有文件 (*.*)",
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
  "hotkey.rolled_back": "已回滚到上一个热键 %1。",
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
  "settings.autostart": "开机自动启动",
  "settings.autostart_note": "写入当前用户的启动项（注册表 Run），不需要管理员权限。移动过程序位置后需要重新勾选一次。",
  "settings.ok": "确定",
  "settings.cancel": "取消"
})JSON";

const char *kEnUS = R"JSON({
  "app.name": "LuoDeck",
  "firstrun.startmenu": "Also bring in the programs pinned in your Start Menu?",
  "firstrun.startmenu_note": "They go onto the panel as loose icons (no frame) and stay in sync with the Start Menu. Delete any you do not want; removed ones stay removed.",
  "firstrun.yes": "Copy them over",
  "firstrun.no": "Not now",
  "tray.startmenu": "Import pinned Start Menu items",
  "group.startmenu": "Start Menu",
  "group.startmenu_empty_hint": "Start Menu shortcuts appear here automatically",
  "popup.search": "Search...",
  "popup.empty": "Nothing in this frame yet",
  "popup.no_match": "No matches",
  "group.open": "Open",
  "group.settings": "Folder settings...",
  "group.desktop": "Desktop",
  "group.rename": "Rename",
  "group.rename_prompt": "Enter a new name:",
  "group.capacity": "Capacity",
  "group.capacity_auto": "Automatic (by icon count)",
  "group.delete": "Delete this frame",
  "group.new": "New folder",
  "folder.settings": "Folder settings",
  "folder.name": "Name",
  "folder.color": "Border color",
  "folder.system_note": "The Desktop folder is created by the system and cannot be customized. You can hide it in Settings.",
  "item.remove": "Remove from panel",
  "item.reveal": "Open file location",
  "item.open": "Open",
  "item.rename": "Rename...",
  "item.rename_prompt": "Display name:",
  "item.args": "Launch arguments...",
  "item.args_prompt": "Appended to the program as-is (empty = open normally).\nExample: -server 127.0.0.1:8080 or \"my save\"\nShortcuts are resolved to their target first; .bat/.cmd/.ps1/.py are supported too.",
  "args.label": "Launch arguments",
  "args.hint": "Appended to the program as-is; leave empty to open normally. Quote arguments that contain spaces. Works for .exe / .bat / .cmd / .ps1 / .py; shortcuts are resolved to their target first.",
  "args.target": "Target: %1",
  "args.no_target": "Target: (cannot resolve; the target may be gone)",
  "item.hide_desktop": "Hide from panel (won't reappear)",
  "group.add_file": "Add files to this frame...",
  "group.sort": "Sort by name",
  "group.dissolve": "Dissolve folder (release icons)",
  "group.compact": "Collapse to folder icon",
  "group.expand": "Expand to panel",
  "group.clear": "Clear this frame",
  "group.empty_hint": "Drop files here, or right-click",
  "group.desktop_empty_hint": "Icons on your desktop appear here automatically",
  "canvas.add_file": "Add file...",
  "canvas.add_folder": "Add folder...",
  "canvas.add_winapp": "Add Windows app...",
  "winapp.title": "Add Windows app",
  "winapp.hint": "These are Microsoft Store / UWP apps. They have no file path you can drag in, so this is the way to add them. Double-click to add, or Ctrl-click to pick several.",
  "winapp.search": "Search apps...",
  "winapp.add": "Add",
  "canvas.arrange": "Tidy up layout",
  "canvas.arrange_f5": "Tidy up layout (F5)",
  "canvas.compact": "Close gaps (keep order)",
  "canvas.empty": "Drop files or shortcuts here, or right-click to add",
  "canvas.file_filter": "Programs & shortcuts (*.exe *.lnk *.url *.bat *.cmd *.ps1 *.py);;All files (*.*)",
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
  "hotkey.rolled_back": "Rolled back to the previous hotkey %1.",
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
  "settings.autostart": "Start with Windows",
  "settings.autostart_note": "Adds an entry to the current user's Run key; no admin rights needed. Re-check it after moving the executable.",
  "settings.ok": "OK",
  "settings.cancel": "Cancel"
})JSON";

} // namespace

namespace {

// 语言名只允许出现在 locales 目录下的文件名里，挡住 config.json 手改 "../../xxx" 的可能
bool isValidLangName(const QString &name)
{
    if (name.isEmpty())
        return false;
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))
        || name.contains(QStringLiteral("..")))
        return false;
    return true;
}

bool readDict(const QString &path, QHash<QString, QString> *out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return false;
    const QJsonObject o = doc.object();
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        out->insert(it.key(), it.value().toString());
    return true;
}

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
    if (!isValidLangName(target))
        target = QStringLiteral("zh_CN");

    QString path = m_dir + "/" + target + ".json";
    if (!QFile::exists(path)) {
        // 语言文件不存在：回落中文。m_current 必须一起改，
        // 否则设置里的语言下拉框会显示一个其实没生效的语言。
        target = QStringLiteral("zh_CN");
        path = m_dir + "/zh_CN.json";
    }

    QHash<QString, QString> dict;
    readDict(path, &dict);
    if (dict.isEmpty() && target != QLatin1String("zh_CN")) {
        // 文件存在但解析不出内容（空文件 / 损坏的 JSON）：同样回落中文
        target = QStringLiteral("zh_CN");
        dict.clear();
        readDict(m_dir + "/zh_CN.json", &dict);
    }

    m_dict = dict;
    m_current = target;
    emit languageChanged(m_current);
}

QString I18n::t(const QString &key) const
{
    return m_dict.value(key, key);
}

QString I18n::t(const QString &key, const QString &arg1) const
{
    // 只替换第一个 %1：参数本身可能也含 "%1"，用 QString::replace 会替换出多层嵌套
    QString s = m_dict.value(key, key);
    const int pos = s.indexOf(QStringLiteral("%1"));
    if (pos >= 0)
        s.replace(pos, 2, arg1);
    return s;
}
