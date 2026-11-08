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
    , m_title(item.system ? I18n::instance().t("group.desktop") : item.name)
{
    setFixedSize(computeSize());
    rebuildTiles();
}

QSize GroupItemWidget::computeSize() const
{
    const int w = m_item.cols * (Metrics::kGroupCellW + 2) + 2 * Metrics::kGroupPad;
    const int h = m_item.rows * (Metrics::kGroupCellH + 2) + 2 * Metrics::kGroupPad
        + Metrics::kGroupTitleH;
    return QSize(w, h);
}

void GroupItemWidget::setLayoutItem(const LayoutItem &item)
{
    m_item = item;
    m_title = item.system ? I18n::instance().t("group.desktop") : item.name;
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
        m_tiles.at(i)->move(Metrics::kGroupPad + c * (Metrics::kGroupCellW + 2),
                            Metrics::kGroupPad + Metrics::kGroupTitleH
                                + r * (Metrics::kGroupCellH + 2));
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
        connect(tile, &IconTile::dragMoved, this,
                [this](const QPoint &start, const QPoint &current) {
                    if (!m_externalDrag) {
                        m_externalDrag = true;
                        beginExternalDrag(start);
                    }
                    updateExternalDrag(current);
                });
        connect(tile, &IconTile::dragEnded, this, [this] {
            if (!m_externalDrag)
                return;
            m_externalDrag = false;
            emit dragFinished(m_item.id, pos());
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
        connect(tile, &IconTile::dragMoved, this,
                [this](const QPoint &start, const QPoint &current) {
                    if (!m_externalDrag) {
                        m_externalDrag = true;
                        beginExternalDrag(start);
                    }
                    updateExternalDrag(current);
                });
        connect(tile, &IconTile::dragEnded, this, [this] {
            if (!m_externalDrag)
                return;
            m_externalDrag = false;
            emit dragFinished(m_item.id, pos());
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

    QColor accent(m_item.color);
    if (!accent.isValid())
        accent = th.color("accent");

    QColor fill = accent;
    fill.setAlpha(22);

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

    const QRect titleRect(Metrics::kGroupPad + 1, 2,
                          width() - 2 * Metrics::kGroupPad - 2,
                          Metrics::kGroupTitleH - 3);
    const QFontMetrics fm(f);
    p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(m_title, Qt::ElideRight, titleRect.width()));
}
