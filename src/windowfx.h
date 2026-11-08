#pragma once

#include <QIcon>
#include <QRect>

class QWidget;

// 生成多尺寸应用图标（无需外部资源文件）
QIcon luoDeckIcon();

// 让系统标题栏跟随主题明暗，避免深色模式把标题栏涂黑
void applyFrameTheme(QWidget *window);

// 把对话框摆在主面板旁边，避免遮住面板
void placeBeside(QWidget *dialog, const QRect &panelGlobalRect);
