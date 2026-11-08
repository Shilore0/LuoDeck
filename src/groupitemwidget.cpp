#include "groupitemwidget.h"

#include "iconprovider.h"
#include "icontile.h"
#include "i18n.h"
#include "metrics.h"
#include "theme.h"

#include <QPainter>

GroupItemWidget::GroupItemWidget(const LayoutItem &item, QWidget *parent)
    : ItemWidget(item.id, parent)
    , m_item(item)
{
    setFixedSize(computeSize());
    rebuildTiles();
}

QSize GroupItemWidget::computeSize() const
{
    const int w = m_item.cols * (Metrics::kGroupCellW + 2) + 2 * Metrics::kGroupPad;
    const int h = m_item.rows * (Metrics::kGroupCellH + 2) + 2 * Metrics::kGroupPad;
    return QSize(w, h);
}

void GroupItemWidget::setLayoutItem(const LayoutItem &item)
{
    m_item = item;
}

void GroupItemWidget::relayoutTiles()
{
    const int cols = qMax(1, m_item.cols);
    for (int i = 0; i < m_tiles.size(); ++i) {
        const int r = i / cols;
        const int c = i % cols;
        m_tiles.at(i)->move(Metrics::kGroupPad + c * (Metrics::kGroupCellW + 2),
                            Metrics::kGroupPad + r * (Metrics::kGroupCellH + 2));
    }
}

void GroupItemWidget::rebuildTiles()
{
    qDeleteAll(m_tiles);
    m_tiles.clear();

    setFixedSize(computeSize());

    const int cap = qMax(1, m_item.cols * m_item.rows);
    const int total = m_item.children.size();
    const bool overflow = total > cap;
    const int visible = overflow ? qMax(0, cap - 1) : total;

    for (int i = 0; i < visible; ++i) {
        const LayoutItem &child = m_item.children.at(i);
        auto *tile = new IconTile(this);
        tile->setIcon(IconProvider::instance().icon(child.path));
        tile->setLabel(child.name);
        tile->setPath(child.path);

        const QString childId = child.id;
        const QString childPath = child.path;
        connect(tile, &IconTile::clicked, this, [this, childId, childPath] {
            emit childLaunchRequested(m_item.id, childId, childPath);
        });
        connect(tile, &IconTile::dragRequested, this, [this](const QPoint &globalPos) {
            beginExternalDrag(globalPos);
        });

        tile->show();
        m_tiles.append(tile);
    }

    if (overflow) {
        auto *tile = new IconTile(this);
        tile->setOverflow(true, I18n::instance().t("group.open"));
        connect(tile, &IconTile::clicked, this, [this] {
            emit openRequested(m_item.id);
        });
        connect(tile, &IconTile::dragRequested, this, [this](const QPoint &globalPos) {
            beginExternalDrag(globalPos);
        });
        tile->show();
        m_tiles.append(tile);

        // 溢出按钮固定占最后一个格子，视觉上跟随网格位置
        const int lastRow = (cap - 1) / qMax(1, m_item.cols);
        const int lastCol = (cap - 1) % qMax(1, m_item.cols);
        tile->move(Metrics::kGroupPad + lastCol * (Metrics::kGroupCellW + 2),
                   Metrics::kGroupPad + lastRow * (Metrics::kGroupCellH + 2));
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
    QColor bg = th.color("surface");
    bg.setAlpha(150);
    QColor border = th.color("border");
    border.setAlpha(150);

    p.setPen(QPen(border, 1));
    p.setBrush(bg);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), Metrics::kGroupRadius, Metrics::kGroupRadius);
}
