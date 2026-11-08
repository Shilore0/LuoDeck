#pragma once

namespace Metrics {

// 普通图标单元
constexpr int kCellW = 62;
constexpr int kCellH = 70;
constexpr int kIconSize = 40;
constexpr int kIconTop = 6;

// 分组框内的小图标单元
constexpr int kGroupCellW = 54;
constexpr int kGroupCellH = 60;
constexpr int kGroupIcon = 32;
constexpr int kGroupIconTop = 4;

// 分组框内边距与配色
constexpr int kGroupPad = 9;
constexpr int kGroupRadius = 14;
constexpr int kGroupTitleH = 18; // 框顶部标题条高度

// 拖动阈值：小于此距离视为点击
constexpr int kDragThreshold = 4;

} // namespace Metrics
