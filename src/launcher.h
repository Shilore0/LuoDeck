#pragma once

#include <QString>

class Launcher
{
public:
    // args 是用户在右键「启动参数」里写的原始命令行尾巴，留空就是原来的打开方式
    static bool run(const QString &path, const QString &args = QString());
    // 以管理员身份启动（UAC 提权）。.bat/.cmd 交给 shell 提权，
    // .ps1/.py 换成对应的解释器再提权；商店应用没有提权这条路，返回 false
    static bool runAsAdmin(const QString &path, const QString &args = QString());
    static void revealInExplorer(const QString &path);
    static void openContainingFolder(const QString &path);
};
