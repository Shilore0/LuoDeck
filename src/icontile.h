#pragma once

#include <QIcon>
#include <QPoint>
#include <QString>
#include <QWidget>

// 分组框内的小图标（也用于展开弹窗），只负责显示与点击，拖动交给父级
// 图标提取可能很慢（.lnk 要走 shell 解析），先出文字，图标稍后补上
class IconTile;
void loadIconAsync(IconTile *tile, const QString &path, int delayMs);

class IconTile : public QWidget
{
    Q_OBJECT
public:
    explicit IconTile(QWidget *parent = nullptr);

    void setIcon(const QIcon &icon);
    void setLabel(const QString &label);
    void setPath(const QString &path);
    void setOverflow(bool overflow, const QString &label);
    // 框内重排时的落点提示：被拖的图标悬到这一格上，画一圈虚线
    void setDropTarget(bool on);

signals:
    void clicked();
    // 拖动过程中持续上报：起点 + 当前点，由父级（框）负责移动自身
    void dragMoved(const QPoint &startGlobal, const QPoint &currentGlobal);
    void dragEnded();
    void contextRequested(const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QIcon m_icon;
    QString m_label;
    QString m_path;
    bool m_hover = false;
    bool m_overflow = false;
    bool m_dropTarget = false;
    QPoint m_pressGlobal;
    bool m_pressing = false;
    bool m_moved = false;
};
