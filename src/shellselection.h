#pragma once

#include <QStringList>

// 读取资源管理器中当前选中的文件/文件夹（通过 IShellWindows）
class ShellSelection
{
public:
    static QStringList currentSelection();
};
