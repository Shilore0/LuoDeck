#include "launcher.h"

#include "appconfig.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
void hideConsoleWindow(QProcess::CreateProcessArguments *args)
{
    args->flags |= CREATE_NO_WINDOW;
    args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
    args->startupInfo->wShowWindow = SW_HIDE;
}
#endif

bool startSilent(const QString &program, const QStringList &arguments, const QString &workDir)
{
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(arguments);
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);
#ifdef Q_OS_WIN
    proc.setCreateProcessArgumentsModifier(hideConsoleWindow);
#endif
    return proc.startDetached();
}

// 找可用的无窗口 Python：设置里指定的优先，其次 PATH 里的 pyw / pythonw
// 注意：Windows 的「应用执行别名」会在 WindowsApps 下放 0 字节空壳，
// 直接运行只会弹应用商店，必须跳过
QString resolvePython()
{
    const QString configured = AppConfig::instance().pythonPath();
    if (!configured.isEmpty()) {
        const QFileInfo fi(configured);
        if (fi.exists() && fi.size() > 4096)
            return configured;
    }

    const QStringList candidates = {QStringLiteral("pyw.exe"), QStringLiteral("pythonw.exe"),
                                    QStringLiteral("python.exe")};
    for (const QString &name : candidates) {
        const QString found = QStandardPaths::findExecutable(name);
        if (found.isEmpty())
            continue;
        const QFileInfo fi(found);
        if (fi.size() <= 4096) // Store 别名占位符，跳过
            continue;
        return found;
    }
    return QString();
}

} // namespace

bool Launcher::run(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return false;

    const QString suffix = fi.suffix().toLower();
    const QString nativePath = QDir::toNativeSeparators(fi.absoluteFilePath());
    const QString workDir = fi.absolutePath();

    if (suffix == "bat" || suffix == "cmd") {
        return startSilent("cmd.exe", {"/c", nativePath}, workDir);
    }

    if (suffix == "ps1") {
        return startSilent("powershell.exe",
                           {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", nativePath},
                           workDir);
    }

    if (suffix == "py") {
        const QString python = resolvePython();
        if (python.isEmpty())
            return false; // 没有可用的 Python，交给上层提示
        return startSilent(python, {nativePath}, workDir);
    }

    // exe / lnk / url / 文件夹 / 其他文档：交给 Shell 打开，行为与资源管理器一致
    return QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
}

void Launcher::revealInExplorer(const QString &path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    startSilent("explorer.exe", {"/select,", native}, QString());
#else
    QProcess::startDetached("xdg-open", {QFileInfo(path).absolutePath()});
#endif
}

void Launcher::openContainingFolder(const QString &path)
{
    const QFileInfo fi(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath()
                                                              : fi.absolutePath()));
}
