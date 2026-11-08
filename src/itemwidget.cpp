#include "itemwidget.h"

#include "metrics.h"

#include <QContextMenuEvent>
#include <QMouseEvent>

ItemWidget::ItemWidget(const QString &itemId, QWidget *parent)
    : QWidget(parent)
    , m_id(itemId)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void ItemWidget::beginExternalDrag(const QPoint &globalPos)
{
    m_pressGlobal = globalPos;
    m_startPos = pos();
    m_pressing = true;
    m_dragging = true;
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
    event->accept();
}

void ItemWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressing) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
    if (!m_dragging && delta.manhattanLength() < Metrics::kDragThreshold) {
        event->accept();
        return;
    }

    m_dragging = true;

    QPoint target = m_startPos + delta;
    if (QWidget *p = parentWidget()) {
        target.setX(qBound(0, target.x(), qMax(0, p->width() - width())));
        target.setY(qBound(0, target.y(), qMax(0, p->height() - height())));
    }
    move(target);
    raise();
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
