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
  "settings.open_dir": "打开配置目录",
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
  "settings.open_dir": "Open config folder",
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

void I18n::writeIfMissing(const QString &fileName, const QByteArray &content)
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
