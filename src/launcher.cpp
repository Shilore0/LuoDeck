#include "launcher.h"

#include "appconfig.h"
#include "winapps.h"

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
#  include <objbase.h>
#  include <shellapi.h>
#  include <shlobj.h>
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

// bat/cmd/ps1 走的是 shell 命令行：换行符可以直接起新命令，
// 参数里不允许出现（多行输入框里很容易误粘进来）
QString sanitizeArgsForShell(const QString &args)
{
    QString s = args;
    s.remove(QLatin1Char('\r'));
    s.remove(QLatin1Char('\n'));
    return s.trimmed();
}

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

// 带参数启动：参数是用户写的一整串，直接当命令行尾巴丢给 CreateProcess，
// 不再经过 Qt 的逐项加引号（否则 "a b" 这种写法会被改掉）。
bool startWithArgs(const QString &program, const QString &nativeArgs, const QString &workDir)
{
    QProcess proc;
    proc.setProgram(program);
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);
#ifdef Q_OS_WIN
    proc.setNativeArguments(nativeArgs);
    proc.setCreateProcessArgumentsModifier(hideConsoleWindow);
#else
    proc.setArguments(QProcess::splitCommand(nativeArgs));
#endif
    return proc.startDetached();
}

// 找可用的 Python：设置里指定的优先，其次 PATH 里的 pythonw / pyw / python
// 注意：Store 版 Python 在 WindowsApps 下也是 0 字节别名（真身在 Program Files\WindowsApps），
// 所以不能按文件大小过滤，别名本身可以正常启动
QString resolvePython()
{
    const QString configured = AppConfig::instance().pythonPath();
    if (!configured.isEmpty() && QFile::exists(configured))
        return configured;

    const QStringList candidates = {QStringLiteral("pythonw.exe"), QStringLiteral("pyw.exe"),
                                    QStringLiteral("python.exe")};
    for (const QString &name : candidates) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty())
            return found;
    }
    return QString();
}

} // namespace

bool Launcher::run(const QString &path, const QString &args)
{
    // 商店应用（UWP）：没有真实文件，按 AUMID 激活
    if (WinApps::isShellAppPath(path))
        return WinApps::activate(WinApps::aumidFromPath(path), args.trimmed());

    const QFileInfo fi(path);
    if (!fi.exists())
        return false;

    const QString suffix = fi.suffix().toLower();
    const QString nativePath = QDir::toNativeSeparators(fi.absoluteFilePath());
    const QString workDir = fi.absolutePath();
    const QString extra = args.trimmed();
    const bool hasArgs = !extra.isEmpty();

    // 带参数启动：把程序本身找出来，参数原样接在后面。
    // 文件夹没有"命令行参数"这回事（走 startWithArgs 只会失败），直接按普通方式打开
    if (hasArgs && !fi.isDir()) {
        QString program = nativePath;
        QString dir = workDir;

        // .lnk 自己吃不下参数，先解析出目标程序，再把参数给它
        if (suffix == "lnk") {
            const QString target = fi.symLinkTarget();
            if (!target.isEmpty() && QFileInfo(target).exists()) {
                program = QDir::toNativeSeparators(target);
                dir = QFileInfo(target).absolutePath();
            }
        }

        const QString progSuffix = QFileInfo(program).suffix().toLower();
        if (progSuffix == "bat" || progSuffix == "cmd") {
            return startWithArgs(QStringLiteral("cmd.exe"),
                                 QStringLiteral("/c \"\"%1\" %2\"")
                                     .arg(program, sanitizeArgsForShell(extra)),
                                 dir);
        }
        if (progSuffix == "ps1") {
            return startWithArgs(QStringLiteral("powershell.exe"),
                                 QStringLiteral("-NoProfile -ExecutionPolicy Bypass -File \"%1\" %2")
                                     .arg(program, sanitizeArgsForShell(extra)),
                                 dir);
        }
        if (progSuffix == "py") {
            const QString python = resolvePython();
            if (python.isEmpty())
                return false;
            return startWithArgs(python,
                                 QStringLiteral("\"%1\" %2")
                                     .arg(program, sanitizeArgsForShell(extra)),
                                 dir);
        }
        if (progSuffix == "exe" || progSuffix.isEmpty()) {
            return startWithArgs(program, extra, dir);
        }
        // 其它类型没有"命令行参数"这回事，按普通方式打开
    }

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
    // 商店应用没有文件可定位，直接打开「所有应用」那个目录
    if (WinApps::isShellAppPath(path)) {
        ShellExecuteW(nullptr, L"open", L"shell:AppsFolder", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    const QFileInfo fi(path);
    const QString native = QDir::toNativeSeparators(fi.absoluteFilePath());

    // 以前是 startDetached("explorer.exe", {"/select,", path})：一是 /select, 被拆成了
    // 两个参数，二是顺带套了 CREATE_NO_WINDOW + SW_HIDE，explorer 窗口直接"隐身"，
    // 看起来就是点了没反应。改用官方接口，选中并弹出所在文件夹。
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(hrCo) && hrCo != S_FALSE;

    ITEMIDLIST *pidl = ILCreateFromPathW(reinterpret_cast<PCWSTR>(native.utf16()));
    if (pidl) {
        const HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
        if (uninit)
            CoUninitialize();
        if (SUCCEEDED(hr))
            return;
    } else if (uninit) {
        CoUninitialize();
    }

    // 兜底：至少把所在目录打开，别什么都不发生
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()));
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
