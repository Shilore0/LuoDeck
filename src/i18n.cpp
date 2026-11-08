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
  "ui.hint": "把文件或文件夹拖到这里添加",
  "ui.empty": "还没有任何项目，把文件拖进来试试",
  "ui.expand": "展开",
  "ui.collapse": "收起",
  "tray.show": "显示面板",
  "tray.reload": "重新加载",
  "tray.config_dir": "打开配置目录",
  "tray.quit": "退出",
  "tray.hotkey_failed": "全局热键注册失败：%1。可能被输入法或其他软件占用（Ctrl+Space 常被中文输入法占用），请修改 luodeck.json 中的 hotkey 字段后重启。",
  "tray.hotkey_ok": "已注册热键 %1",
  "ctx.remove": "从面板移除",
  "ctx.reveal": "打开所在位置",
  "ctx.open_folder": "在资源管理器中打开"
})JSON";

const char *kEnUS = R"JSON({
  "app.name": "LuoDeck",
  "ui.hint": "Drag files or folders here to add",
  "ui.empty": "Nothing here yet. Drag some files in.",
  "ui.expand": "Expand",
  "ui.collapse": "Collapse",
  "tray.show": "Show panel",
  "tray.reload": "Reload",
  "tray.config_dir": "Open config folder",
  "tray.quit": "Quit",
  "tray.hotkey_failed": "Failed to register hotkey: %1. It may be taken by the IME or another app. Edit \"hotkey\" in luodeck.json and restart.",
  "tray.hotkey_ok": "Hotkey %1 registered",
  "ctx.remove": "Remove from panel",
  "ctx.reveal": "Open file location",
  "ctx.open_folder": "Open in Explorer"
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
