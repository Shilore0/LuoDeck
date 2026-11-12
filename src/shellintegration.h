#pragma once

#include <QString>

// Windows 资源管理器右键菜单集成（写入 HKCU，无需管理员权限）
class ShellIntegration
{
public:
    static bool isInstalled();
    static bool install(QString *error = nullptr);
    static bool uninstall(QString *error = nullptr);
    static QString exePath();

    // 开机自启：写 HKCU\...\CurrentVersion\Run，无需管理员权限
    static bool isAutoStartEnabled();
    static bool setAutoStart(bool enabled);

private:
    static QString fileKey();
    static QString dirKey();
    static QString backgroundKey();
    static QString runKey();
};
