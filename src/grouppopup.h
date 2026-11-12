#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include "appconfig.h"

class IconTile;
class QGridLayout;
class QLineEdit;
class QScrollArea;
class QWidget;

// 展开的框：显示名称 + 全部图标。项多时自动带搜索框与滚动条（开始菜单框就是这个场景）
class GroupPopup : public QWidget
{
    Q_OBJECT
public:
    explicit GroupPopup(const LayoutItem &group, QWidget *parent = nullptr);

    void popupAt(const QPoint &globalPos);

signals:
    void childLaunchRequested(const QString &path, const QString &args);
    // 弹层里按 F5：交给画布整理整个面板
    void arrangeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    void buildUi();
    void populate(const QString &filter);
    void relayoutToContent(const QPoint &globalPos);
    // 弹层里也能拖动换位：拖到另一格上就换到那个位置
    int tileIndexAt(const QPoint &localPos) const;
    void setDropIndex(int index);
    void finishTileDrag();

    LayoutItem m_group;
    QGridLayout *m_grid = nullptr;
    QWidget *m_gridHost = nullptr;
    QScrollArea *m_scroll = nullptr;
    QLineEdit *m_search = nullptr;
    QVector<IconTile *> m_tiles;      // 与 m_tileChildIds 一一对应
    QVector<QString> m_tileChildIds;  // 每格对应的 child id（搜索过滤后也能对上号）
    int m_dragTileIndex = -1;
    int m_dropTileIndex = -1;
    QPoint m_anchor;
    bool m_allowAutoClose = false;
};
