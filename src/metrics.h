#pragma once

namespace Metrics {

// 普通图标单元
constexpr int kCellW = 62;
constexpr int kCellH = 70;
constexpr int kIconSize = 40;
constexpr int kIconTop = 6;

// 分组框内的图标单元：和外面的图标格子同节奏（62×70 + 12 间距），
// 这样框宽正好等于「列数 × 74」，放到格点上时右边缘和右边的图标严丝合缝。
constexpr int kGroupCellW = 62;
constexpr int kGroupCellH = 70;
constexpr int kGroupGap = 12;
constexpr int kGroupIcon = 28;   // 图标本身画小，但格子还是 62×70，保证外层是 74 的整数倍
constexpr int kGroupIconTop = 10;

// 分组框内边距与配色
constexpr int kGroupPad = 0; // 框宽 = 74*列-12、框高 = 82*行-12，和图标网格完全同节奏
constexpr int kGroupRadius = 6; // 方块感：圆角只留一点点，不再是药丸/胶囊
constexpr int kGroupTitleH = 16; // 框顶部标题条高度

// 拖动阈值：小于此距离视为点击
constexpr int kDragThreshold = 4;

} // namespace Metrics
