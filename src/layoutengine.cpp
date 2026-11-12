#include "layoutengine.h"

#include "metrics.h"

namespace {

constexpr int kPad = 16; // 画布内边距（与网格原点一致）
constexpr int kStep = 8;

} // namespace

namespace LayoutEngine {

int groupWidth(int cols)
{
    const int c = qBound(1, cols, 8);
    return 2 * Metrics::kGroupPad + c * Metrics::kGroupCellW + (c - 1) * Metrics::kGroupGap;
}

int groupHeight(int rows)
{
    // 标题条是从第一行格子里"借"的 16px，所以总高度仍然是 82 的整数倍
    const int r = qBound(1, rows, 8);
    return 2 * Metrics::kGroupPad + r * Metrics::kGroupCellH + (r - 1) * Metrics::kGroupGap;
}

QSize itemSize(const LayoutItem &item)
{
    // 小文件夹和大图标一样大，正好占一个格子
    if (item.isGroup() && !item.compact)
        return QSize(groupWidth(item.cols), groupHeight(item.rows));
    return QSize(Metrics::kCellW, Metrics::kCellH);
}

QRect itemRect(const LayoutItem &item)
{
    return QRect(QPoint(item.x, item.y), itemSize(item));
}

QVector<QRect> occupiedRects(const QVector<LayoutItem> &items, const QString &exceptId)
{
    QVector<QRect> out;
    out.reserve(items.size());
    for (const LayoutItem &it : items) {
        if (!exceptId.isEmpty() && it.id == exceptId)
            continue;
        out.append(itemRect(it));
    }
    return out;
}

QRect boundsOf(const QVector<LayoutItem> &items)
{
    if (items.isEmpty())
        return QRect();

    QRect r = itemRect(items.first());
    for (const LayoutItem &it : items)
        r = r.united(itemRect(it));
    return r;
}

QPoint snap(const QPoint &pos)
{
    const int gx = qMax(0, qRound((pos.x() - kGridOrigin) / double(kCellStepX)));
    const int gy = qMax(0, qRound((pos.y() - kGridOrigin) / double(kCellStepY)));
    return QPoint(kGridOrigin + gx * kCellStepX, kGridOrigin + gy * kCellStepY);
}

QRect cellRect(const QPoint &snappedPos)
{
    return QRect(snappedPos, QSize(Metrics::kCellW, Metrics::kCellH));
}

QSize spanOf(const QRect &rect)
{
    const int cols = qMax(1, qRound(rect.width() / double(kCellStepX)));
    const int rows = qMax(1, qRound(rect.height() / double(kCellStepY)));
    return QSize(cols, rows);
}

QPoint findFreeSpot(const QVector<QRect> &occupied, const QSize &size, int canvasWidth, int gap)
{
    int maxBottom = kPad;
    for (const QRect &r : occupied)
        maxBottom = qMax(maxBottom, r.bottom() + gap + kStep);

    const int w = size.width();
    const int h = size.height();
    const int rightLimit = qMax(kPad + w, canvasWidth - kPad);

    // 画布连这一个都放不下：直接摞到最下面，交给调用方决定要不要加宽
    if (kPad + w > canvasWidth - kPad)
        return QPoint(kPad, maxBottom);

    for (int y = kPad; y <= maxBottom + size.height(); y += kCellStepY) {
        for (int x = kPad; x + w <= rightLimit; x += kCellStepX) {
            const QRect cand(x, y, w, h);
            bool hit = false;
            for (const QRect &r : occupied) {
                if (cand.intersects(r.adjusted(-gap, -gap, gap, gap))) {
                    hit = true;
                    break;
                }
            }
            if (!hit)
                return cand.topLeft();
        }
    }

    return QPoint(kPad, maxBottom);
}

} // namespace LayoutEngine
