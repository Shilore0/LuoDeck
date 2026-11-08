#include "tiledelegate.h"

#include "theme.h"
#include "tilemodel.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>

TileDelegate::TileDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void TileDelegate::setCellSize(const QSize &size)
{
    m_cell = size;
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return m_cell;
}

void TileDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const int depth = index.data(TileModel::DepthRole).toInt();
    const bool isFolder = index.data(TileModel::IsFolderRole).toBool();
    const bool expanded = index.data(TileModel::ExpandedRole).toBool();
    const QString name = index.data(Qt::DisplayRole).toString();
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect cell = option.rect.adjusted(3, 3, -3, -3);
    const Theme &th = Theme::instance();
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool selected = option.state & QStyle::State_Selected;

    if (hovered || selected) {
        QColor bg = th.color("surfaceHover");
        bg.setAlpha(selected ? 235 : 190);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRoundedRect(cell, 8, 8);
    } else if (isFolder && expanded) {
        QColor bg = th.color("accent");
        bg.setAlpha(28);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRoundedRect(cell, 8, 8);
    }

    const int baseSide = (depth == 0) ? 48 : 30;
    const QSize iconSize(baseSide, baseSide);
    const int iconTop = cell.top() + 8 + (48 - baseSide) / 2;
    const QRect iconRect(cell.center().x() - baseSide / 2, iconTop, baseSide, baseSide);

    if (!icon.isNull()) {
        const qreal dpr = qApp->devicePixelRatio();
        QPixmap pm = icon.pixmap(QSize(baseSide, baseSide) * dpr);
        if (!pm.isNull()) {
            pm.setDevicePixelRatio(dpr);
            painter->drawPixmap(iconRect, pm);
        }
    }

    QFont f = option.font;
    f.setPixelSize(depth == 0 ? 12 : 11);
    painter->setFont(f);
    painter->setPen(th.color(hovered ? "text" : "text"));

    QRect textRect(cell.left() + 2, iconRect.bottom() + 4,
                   cell.width() - 4, cell.bottom() - iconRect.bottom() - 4);
    const QFontMetrics fm(f);
    const QString elided = fm.elidedText(name, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignTop | Qt::AlignHCenter, elided);

    painter->restore();
}
