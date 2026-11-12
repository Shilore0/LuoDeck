#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QPainter;

// 可拖动图标的基类：短按=激活，拖超过阈值=移动自身并上报新坐标
class ItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ItemWidget(const QString &itemId, QWidget *parent = nullptr);

    QString itemId() const { return m_id; }
    void setItemId(const QString &id) { m_id = id; }

    // 供分组内部子图标把手势交给分组本身
    void beginExternalDrag(const QPoint &globalPos);
    void updateExternalDrag(const QPoint &globalPos);

    // 短暂高亮一次（新加入/拖入时用来确认位置）
    void flashHighlight();

signals:
    void activated(const QString &id);
    void dragFinished(const QString &id, const QPoint &pos);
    void contextRequested(const QString &id, const QPoint &globalPos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    // 子类 paintEvent 里调用，给当前项画一圈高亮
    void paintHighlight(QPainter &p, int radius) const;

private:
    QString m_id;
    QPoint m_pressGlobal;
    QPoint m_startPos;
    bool m_pressing = false;
    bool m_dragging = false;
    bool m_highlighted = false;
};
