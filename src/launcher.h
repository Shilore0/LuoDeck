#pragma once

#include <QString>

class Launcher
{
public:
    // args 是用户在右键「启动参数」里写的原始命令行尾巴，留空就是原来的打开方式
    static bool run(const QString &path, const QString &args = QString());
    static void revealInExplorer(const QString &path);
    static void openContainingFolder(const QString &path);
};
