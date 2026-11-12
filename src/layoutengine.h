#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>

#include "appconfig.h"

// 布局计算：把"某个项占多大、放在哪不压到别人"这类几何问题集中在这里，
// 避免各处在 AppConfig / CanvasWidget 里各算一套（之前新图标压住桌面框就是这么来的）。
namespace LayoutEngine {

// 顶层图标按格子摆放（手机桌面那种）。格子原点/步长固定，图标永远落在格点上。
constexpr int kGridOrigin = 16;
constexpr int kCellStepX = 74; // 62 图标 + 12 间距
constexpr int kCellStepY = 82; // 70 图标 + 12 间距

// 把任意像素坐标吸附到最近的格点
QPoint snap(const QPoint &pos);
// 格点对应的格子矩形（一个图标的占位）
QRect cellRect(const QPoint &snappedPos);
// 某个矩形大致占几列几行
QSize spanOf(const QRect &rect);

// 框的外框尺寸（与 GroupItemWidget::computeSize 保持一致）
int groupWidth(int cols);
int groupHeight(int rows);

QSize itemSize(const LayoutItem &item);
QRect itemRect(const LayoutItem &item);

// 扫描 canvasWidth 宽的画布，返回第一个不与 occupied 相交、且四周留出 gap 的位置。
// 找不到空位时返回最下方（由调用方负责把画布撑高）。
QPoint findFreeSpot(const QVector<QRect> &occupied, const QSize &size, int canvasWidth,
                    int gap = 12);

// 所有顶层项的包围盒；无内容时返回空矩形
QRect boundsOf(const QVector<LayoutItem> &items);

// 顶层项占位表（给 findFreeSpot 用）
QVector<QRect> occupiedRects(const QVector<LayoutItem> &items, const QString &exceptId = QString());

} // namespace LayoutEngine
