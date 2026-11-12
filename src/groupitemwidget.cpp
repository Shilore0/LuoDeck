#include "groupitemwidget.h"

#include "iconprovider.h"
#include "icontile.h"
#include "layoutengine.h"
#include "i18n.h"
#include "metrics.h"
#include "theme.h"

#include <QCursor>
#include <QPainter>

namespace {

QString frameTitle(const LayoutItem &item)
{
    if (item.isDesktopFrame())
        return I18n::instance().t("group.desktop");
    if (item.isStartMenuFrame())
        return I18n::instance().t("group.startmenu");
    return item.name;
}

} // namespace

GroupItemWidget::GroupItemWidget(const LayoutItem &item, QWidget *parent)
    : ItemWidget(item.id, parent)
    , m_item(item)
    , m_title(frameTitle(item))
{
    setFixedSize(computeSize());
    rebuildTiles();
}

QSize GroupItemWidget::computeSize() const
{
    // 小文件夹：跟普通图标一样占一格
    if (m_item.compact)
        return QSize(Metrics::kCellW, Metrics::kCellH);

    return QSize(LayoutEngine::groupWidth(m_item.cols),
                 LayoutEngine::groupHeight(m_item.rows));
}

void GroupItemWidget::setLayoutItem(const LayoutItem &item)
{
    m_item = item;
    m_title = frameTitle(item);
}

void GroupItemWidget::setColor(const QString &color)
{
    if (m_item.color == color)
        return;
    m_item.color = color;
    update();
}

void GroupItemWidget::relayoutTiles()
{
    const int cols = qMax(1, m_item.cols);
    for (int i = 0; i < m_tiles.size(); ++i) {
        const int r = i / cols;
        const int c = i % cols;

        // 第 0 行要让出顶部标题条的高度，否则图标会盖住名字、点名字点不到框
        const int x = c * (Metrics::kGroupCellW + Metrics::kGroupGap);
        int y = r * (Metrics::kGroupCellH + Metrics::kGroupGap);
        int h = Metrics::kGroupCellH;
        if (r == 0) {
            y = Metrics::kGroupTitleH;
            h = Metrics::kGroupCellH - Metrics::kGroupTitleH;
        }

        m_tiles.at(i)->setFixedSize(Metrics::kGroupCellW, h);
        m_tiles.at(i)->move(x, y);
    }
}

int GroupItemWidget::tileIndexAt(const QPoint &localPos) const
{
    // 只认真正画出来的图标；溢出的「打开」格不是排序目标
    for (int i = 0; i < m_visibleCount && i < m_tiles.size(); ++i) {
        if (m_tiles.at(i)->geometry().contains(localPos))
            return i;
    }
    return -1;
}

void GroupItemWidget::setDropIndex(int index)
{
    if (index == m_dragChildIndex)
        index = -1;
    if (index == m_dropIndex)
        return;

    if (m_dropIndex >= 0 && m_dropIndex < m_tiles.size())
        m_tiles.at(m_dropIndex)->setDropTarget(false);
    m_dropIndex = index;
    if (m_dropIndex >= 0 && m_dropIndex < m_tiles.size())
        m_tiles.at(m_dropIndex)->setDropTarget(true);
}

void GroupItemWidget::clearDragState()
{
    setDropIndex(-1);
    m_dragChildId.clear();
    m_dragChildIndex = -1;
}

void GroupItemWidget::rebuildTiles()
{
    qDeleteAll(m_tiles);
    m_tiles.clear();
    m_dragChildId.clear();
    m_dragChildIndex = -1;
    m_dropIndex = -1;
    m_visibleCount = 0;

    setFixedSize(computeSize());
    setToolTip(m_item.name);

    // 小文件夹只显示缩略图，点开才看全部（走 GroupPopup）
    if (m_item.compact) {
        update();
        return;
    }

    const int cap = qMax(1, m_item.cols * m_item.rows);
    const int total = m_item.children.size();
    const bool overflow = total > cap;
    const int visible = overflow ? qMax(0, cap - 1) : total;

    for (int i = 0; i < visible; ++i) {
        const LayoutItem &child = m_item.children.at(i);
        auto *tile = new IconTile(this);
        tile->setLabel(child.name);
        tile->setPath(child.path);
        loadIconAsync(tile, child.path, i * 6);

        const QString childId = child.id;
        const QString childPath = child.path;
        connect(tile, &IconTile::clicked, this, [this, childId, childPath] {
            emit childLaunchRequested(m_item.id, childId, childPath);
        });
        connect(tile, &IconTile::contextRequested, this,
                [this, childId, childPath](const QPoint &globalPos) {
                    emit childContextRequested(m_item.id, childId, childPath, globalPos);
                });
        // 拖动：不搬动整个框，只记下正在拖哪一格；
        // 松手时落在框内另一格上就换位，落在框外就变成画布上的独立图标。
        connect(tile, &IconTile::dragMoved, this,
                [this, childId, i](const QPoint &, const QPoint &currentGlobal) {
                    m_dragChildId = childId;
                    m_dragChildIndex = i;
                    setDropIndex(tileIndexAt(mapFromGlobal(currentGlobal)));
                });
        connect(tile, &IconTile::dragEnded, this, [this] {
            const QString childId = m_dragChildId;
            const int from = m_dragChildIndex;
            const QPoint global = QCursor::pos();
            clearDragState();

            if (childId.isEmpty())
                return;

            const QPoint here = mapFromGlobal(global);
            if (rect().contains(here)) {
                // 框内拖动：拖到哪一格就排到那一格
                const int to = tileIndexAt(here);
                if (to >= 0 && from >= 0 && to != from)
                    AppConfig::instance().moveChildToIndex(m_item.id, from, to);
                return;
            }

            QWidget *canvas = parentWidget();
            if (!canvas)
                return;
            emit tileDragOut(m_item.id, childId, canvas->mapFromGlobal(global));
        });

        tile->show();
        m_tiles.append(tile);
    }

    m_visibleCount = m_tiles.size();

    if (overflow) {
        auto *tile = new IconTile(this);
        tile->setOverflow(true, I18n::instance().t("group.open"));
        connect(tile, &IconTile::clicked, this, [this] {
            emit openRequested(m_item.id);
        });
        tile->show();
        m_tiles.append(tile);
    }

    relayoutTiles();
    update();
}

void GroupItemWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const Theme &th = Theme::instance();

    if (m_item.compact) {
        QColor accent(m_item.color);
        if (!accent.isValid())
            accent = th.color("accent");

        QColor fill = th.color("surface");
        fill.setAlpha(underMouse() ? 235 : 210);
        p.setPen(QPen(accent, 1.2));
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), Metrics::kGroupRadius,
                          Metrics::kGroupRadius);

        // 缩略图：1~2 个居中排一行，3~4 个排 2×2，看起来才平衡
        const int n = qMin(4, m_item.children.size());
        const int inner = qMin(width() - 14, height() - 26);
        const int cols = (n <= 2) ? qMax(1, n) : 2;
        const int rows = (n <= 2) ? 1 : 2;
        const int cellW = inner / cols;
        const int cellH = inner / rows;
        const int left = (width() - cols * cellW) / 2;
        const int top = 4 + (inner - rows * cellH) / 2;

        for (int i = 0; i < n; ++i) {
            const int r = i / cols;
            const int c = i % cols;
            const QRect box(left + c * cellW, top + r * cellH, cellW, cellH);
            const QIcon icon = IconProvider::instance().icon(m_item.children.at(i).path);
            const int side = qMin(box.width(), box.height()) - 4;
            if (side <= 6)
                continue;
            const QRect iconRect(box.center().x() - side / 2, box.center().y() - side / 2, side,
                                 side);
            const qreal dpr = devicePixelRatioF();
            QPixmap pm = icon.pixmap(QSize(side, side) * dpr);
            if (!pm.isNull()) {
                pm.setDevicePixelRatio(dpr);
                p.drawPixmap(iconRect, pm);
            }
        }

        if (n == 0) {
            QFont f = font();
            f.setPixelSize(18);
            p.setFont(f);
            p.setPen(accent);
            p.drawText(QRect(0, 0, width(), height() - 12), Qt::AlignCenter,
                       QStringLiteral("+"));
        }

        // 名字
        QFont f = font();
        f.setPixelSize(10);
        p.setFont(f);
        p.setPen(th.color("text"));
        const QRect textRect(1, height() - 15, width() - 2, 14);
        const QFontMetrics fm(f);
        p.drawText(textRect, Qt::AlignCenter,
                   fm.elidedText(m_item.name, Qt::ElideRight, textRect.width()));

        paintHighlight(p, Metrics::kGroupRadius);
        return;
    }

    QColor accent(m_item.color);
    if (!accent.isValid())
        accent = th.color("accent");

    QColor fill = accent;
    fill.setAlpha(underMouse() ? 32 : 22);

    p.setPen(QPen(accent, 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Metrics::kGroupRadius,
                      Metrics::kGroupRadius);

    // 顶部标题条：显示文件夹名字
    QFont f = font();
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    p.setPen(th.color("text"));

    const QRect titleRect(6, 2, width() - 12, Metrics::kGroupTitleH - 3);
    const QFontMetrics fm(f);
    p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(m_title, Qt::ElideRight, titleRect.width()));

    // 空框给一句提示，否则看起来像坏了
    if (m_item.children.isEmpty()) {
        QFont ef = font();
        ef.setPixelSize(11);
        p.setFont(ef);
        QColor muted = th.color("textMuted");
        muted.setAlpha(170);
        p.setPen(muted);
        p.drawText(rect().adjusted(Metrics::kGroupPad, Metrics::kGroupTitleH,
                                   -Metrics::kGroupPad, -Metrics::kGroupPad),
                   Qt::AlignCenter | Qt::TextWordWrap,
                   I18n::instance().t(m_item.isStartMenuFrame() ? "group.startmenu_empty_hint"
                                        : m_item.isDesktopFrame() ? "group.desktop_empty_hint"
                                                                  : "group.empty_hint"));
    }

    paintHighlight(p, Metrics::kGroupRadius);
}
