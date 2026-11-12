#include "shellintegration.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QTimer>

namespace {

QString menuText()
{
    return QStringLiteral("添加到 LuoDeck");
}

void writeVerb(const QString &baseKey, const QString &command)
{
    QSettings s(baseKey, QSettings::NativeFormat);
    s.setValue(QStringLiteral("."), menuText());
    s.setValue(QStringLiteral("Icon"), ShellIntegration::exePath() + QStringLiteral(",0"));
    s.setValue(QStringLiteral("command/."), command);
    s.sync();
}

void removeVerb(const QString &baseKey)
{
    QSettings s(baseKey, QSettings::NativeFormat);
    s.remove(QString());
    s.sync();
}

} // namespace

QString ShellIntegration::exePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

QString ShellIntegration::fileKey()
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\*\\shell\\LuoDeck");
}

QString ShellIntegration::dirKey()
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\Directory\\shell\\LuoDeck");
}

QString ShellIntegration::backgroundKey()
{
    return QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Classes\\Directory\\Background\\shell\\LuoDeck");
}

bool ShellIntegration::isInstalled()
{
    QSettings s(fileKey(), QSettings::NativeFormat);
    return s.contains(QStringLiteral("command/."));
}

bool ShellIntegration::install(QString *error)
{
    const QString exe = exePath();
    if (!QFile::exists(exe)) {
        if (error)
            *error = QStringLiteral("找不到程序自身路径：%1").arg(exe);
        return false;
    }

    // 文件与文件夹：命令行里 %1 由资源管理器替换为选中项
    const QString itemCommand = QStringLiteral("\"%1\" --add \"%2\"").arg(exe, QStringLiteral("%1"));
    writeVerb(fileKey(), itemCommand);
    writeVerb(dirKey(), itemCommand);

    // 文件夹空白处：%V 是当前目录
    const QString bgCommand = QStringLiteral("\"%1\" --add \"%2\"").arg(exe, QStringLiteral("%V"));
    writeVerb(backgroundKey(), bgCommand);

    // 不管调用方要不要 error 都回读校验：写注册表失败却返回 true，
    // 用户会以为右键菜单装好了
    QSettings check(fileKey(), QSettings::NativeFormat);
    if (!check.contains(QStringLiteral("command/."))) {
        if (error)
            *error = QStringLiteral("写入注册表失败");
        return false;
    }
    return true;
}

bool ShellIntegration::uninstall(QString *error)
{
    Q_UNUSED(error)
    removeVerb(fileKey());
    removeVerb(dirKey());
    removeVerb(backgroundKey());
    return true;
}

QString ShellIntegration::runKey()
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

bool ShellIntegration::isAutoStartEnabled()
{
    QSettings s(runKey(), QSettings::NativeFormat);
    return !s.value(QStringLiteral("LuoDeck")).toString().isEmpty();
}

bool ShellIntegration::setAutoStart(bool enabled)
{
    QSettings s(runKey(), QSettings::NativeFormat);
    if (enabled)
        s.setValue(QStringLiteral("LuoDeck"), QStringLiteral("\"%1\"").arg(exePath()));
    else
        s.remove(QStringLiteral("LuoDeck"));
    s.sync();

    return isAutoStartEnabled() == enabled;
}
