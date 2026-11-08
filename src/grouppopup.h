#pragma once

#include <QWidget>

#include "appconfig.h"

// 展开的框：显示名称 + 全部图标
class GroupPopup : public QWidget
{
    Q_OBJECT
public:
    explicit GroupPopup(const LayoutItem &group, QWidget *parent = nullptr);

    void popupAt(const QPoint &globalPos);

signals:
    void childLaunchRequested(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    void buildTiles();

    LayoutItem m_group;
    bool m_allowAutoClose = false;
};
