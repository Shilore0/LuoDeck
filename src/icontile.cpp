#include "icontile.h"

#include "metrics.h"
#include "theme.h"

#include "iconprovider.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

IconTile::IconTile(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(Metrics::kGroupCellW, Metrics::kGroupCellH);
}

void IconTile::setIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

void IconTile::setLabel(const QString &label)
{
    m_label = label;
    update();
}

void IconTile::setPath(const QString &path)
{
    m_path = path;
    setToolTip(path);
}

void IconTile::setOverflow(bool overflow, const QString &label)
{
    m_overflow = overflow;
    if (overflow)
        m_label = label;
    update();
}

void IconTile::setDropTarget(bool on)
{
    if (m_dropTarget == on)
        return;
    m_dropTarget = on;
    update();
}

void IconTile::enterEvent(QEnterEvent *event)
{
    m_hover = true;
    update();
    QWidget::enterEvent(event);
}

void IconTile::leaveEvent(QEvent *event)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(event);
}

void IconTile::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const Theme &th = Theme::instance();
    const QRect cell = rect().adjusted(1, 1, -1, -1);

    // 标签固定在底部，图标在剩余空间里居中；第一行格子矮 16px（让出标题条）也照样居中
    constexpr int labelH = 15;
    const int iconTop = qMax(2, (cell.height() - labelH - Metrics::kGroupIcon) / 2);

    if (m_hover) {
        QColor bg = th.color("surfaceHover");
        bg.setAlpha(200);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(cell, 4, 4);
    }

    // 重排时的落点提示
    if (m_dropTarget) {
        QColor accent = th.color("accent");
        QPen pen(accent, 1.4, Qt::DashLine);
        p.setPen(pen);
        QColor fill = accent;
        fill.setAlpha(30);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(cell).adjusted(1.5, 1.5, -1.5, -1.5), 7, 7);
    }

    if (m_overflow) {
        QColor accent = th.color("accent");
        QPen pen(accent, 1.4);
        pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        QColor fill = accent;
        fill.setAlpha(24);
        p.setBrush(fill);
        const QRect box(cell.center().x() - 14, cell.top() + iconTop, 28, 28);
        p.drawRoundedRect(box, 7, 7);

        p.setPen(accent);
        QFont f = font();
        f.setPixelSize(15);
        f.setBold(true);
        p.setFont(f);
        p.drawText(box, Qt::AlignCenter, QStringLiteral("+"));
    } else if (!m_icon.isNull()) {
        const int side = Metrics::kGroupIcon;
        const QRect iconRect(cell.center().x() - side / 2, cell.top() + iconTop, side, side);
        const qreal dpr = devicePixelRatioF();
        QPixmap pm = m_icon.pixmap(QSize(side, side) * dpr);
        if (!pm.isNull()) {
            pm.setDevicePixelRatio(dpr);
            p.drawPixmap(iconRect, pm);
        }
    }

    QFont f = font();
    f.setPixelSize(10);
    p.setFont(f);
    p.setPen(th.color(m_hover ? "text" : "textMuted"));
    const QRect textRect(cell.left() + 1, cell.bottom() - 15, cell.width() - 2, 15);
    const QFontMetrics fm(f);
    p.drawText(textRect, Qt::AlignCenter,
               fm.elidedText(m_label, Qt::ElideRight, textRect.width()));
}

void IconTile::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressGlobal = event->globalPosition().toPoint();
    m_pressing = true;
    m_moved = false;
    event->accept();
}

void IconTile::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressing) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint current = event->globalPosition().toPoint();
    if (!m_moved && (current - m_pressGlobal).manhattanLength() < Metrics::kDragThreshold) {
        event->accept();
        return;
    }
    m_moved = true;
    emit dragMoved(m_pressGlobal, current);
    event->accept();
}

void IconTile::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const bool moved = m_moved;
    m_pressing = false;
    m_moved = false;

    if (moved)
        emit dragEnded();
    else
        emit clicked();
    event->accept();
}

void IconTile::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextRequested(event->globalPos());
    event->accept();
}

void loadIconAsync(IconTile *tile, const QString &path, int delayMs)
{
    if (!tile)
        return;
    // 以 tile 作为 context object：弹层关掉时定时器自动作废，不会野指针
    QTimer::singleShot(delayMs, tile, [tile, path] {
        tile->setIcon(IconProvider::instance().icon(path));
    });
}
