#pragma once

#include <QPixmap>
#include <QString>
#include <QVector>

// Windows 商店应用（UWP / APPX，比如 Armoury Crate、计算器、Xbox）：
// 它们在开始菜单里**不是 .lnk 文件**，而是 shell:AppsFolder 这个虚拟文件夹下的条目，
// 靠 AUMID（Application User Model ID）启动，所以既没有可以拖的路径，也不能当普通 exe 处理。
// 这里把这些事集中起来：枚举、取图标、激活。
namespace WinApps {

// 面板里存的路径形如：shell:AppsFolder\Microsoft.WindowsCalculator_8wekyb3d8bbwe!App
inline constexpr char kPrefix[] = "shell:AppsFolder\\";

struct Entry {
    QString name;  // 本地化显示名
    QString aumid; // 形如 xxx_8wekyb3d8bbwe!App
};

bool isShellAppPath(const QString &path);
QString makePath(const QString &aumid);
QString aumidFromPath(const QString &path);

// 已安装的商店应用（拿不到就返回空）
QVector<Entry> list();

// shell 里的"磁贴图"（带透明通道），拿不到返回空
QPixmap image(const QString &aumid, int size);

// 按 AUMID 启动；args 会作为命令行参数传给应用
bool activate(const QString &aumid, const QString &args = QString());

} // namespace WinApps
