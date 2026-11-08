#pragma once

#include <QIcon>
#include <QPoint>
#include <QString>
#include <QWidget>

// 分组框内的小图标（也用于展开弹窗），只负责显示与点击，拖动交给父级
class IconTile : public QWidget
{
    Q_OBJECT
public:
    explicit IconTile(QWidget *parent = nullptr);

    void setIcon(const QIcon &icon);
    void setLabel(const QString &label);
    void setPath(const QString &path);
    void setOverflow(bool overflow, const QString &label);

signals:
    void clicked();
    void dragRequested(const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QIcon m_icon;
    QString m_label;
    QString m_path;
    bool m_hover = false;
    bool m_overflow = false;
    QPoint m_pressGlobal;
    bool m_pressing = false;
    bool m_moved = false;
};
