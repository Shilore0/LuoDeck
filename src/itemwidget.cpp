#include "itemwidget.h"

#include "metrics.h"
#include "theme.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

ItemWidget::ItemWidget(const QString &itemId, QWidget *parent)
    : QWidget(parent)
    , m_id(itemId)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void ItemWidget::flashHighlight()
{
    m_highlighted = true;
    update();
    QTimer::singleShot(900, this, [this] {
        m_highlighted = false;
        update();
    });
}

void ItemWidget::paintHighlight(QPainter &p, int radius) const
{
    if (!m_highlighted)
        return;

    QColor glow = Theme::instance().color("accent");
    glow.setAlpha(210);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(glow, 2.0));
    p.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), radius, radius);
}

void ItemWidget::beginExternalDrag(const QPoint &globalPos)
{
    m_pressGlobal = globalPos;
    m_startPos = pos();
    m_pressing = true;
    m_dragging = true;
    raise();
}

void ItemWidget::updateExternalDrag(const QPoint &globalPos)
{
    if (!m_pressing)
        return;

    QPoint target = m_startPos + (globalPos - m_pressGlobal);
    QWidget *p = parentWidget();
    if (p) {
        target.setX(qBound(0, target.x(), qMax(0, p->width() - width())));
        target.setY(qBound(0, target.y(), qMax(0, p->height() - height())));
    }

    // 只重绘旧位置与新位置的并集，避免整块画布每帧重画
    const QRect before = geometry();
    move(target);
    if (p && before != geometry())
        p->update(before.united(geometry()));
}

void ItemWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressGlobal = event->globalPosition().toPoint();
    m_startPos = pos();
    m_pressing = true;
    m_dragging = false;
    raise(); // 只在这里抬升一次，拖动过程中不要再调
    event->accept();
}

void ItemWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressing) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint current = event->globalPosition().toPoint();
    if (!m_dragging && (current - m_pressGlobal).manhattanLength() < Metrics::kDragThreshold) {
        event->accept();
        return;
    }

    m_dragging = true;
    updateExternalDrag(current);
    event->accept();
}

void ItemWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const bool wasDragging = m_dragging;
    m_pressing = false;
    m_dragging = false;

    if (wasDragging)
        emit dragFinished(m_id, pos());
    else
        emit activated(m_id);

    event->accept();
}

void ItemWidget::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextRequested(m_id, event->globalPos());
    event->accept();
}
