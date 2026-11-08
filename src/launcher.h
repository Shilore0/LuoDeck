#pragma once

#include <QString>

class Launcher
{
public:
    static bool run(const QString &path);
    static void revealInExplorer(const QString &path);
    static void openContainingFolder(const QString &path);
};
