#pragma once

#include <QVector>

#include "appconfig.h"
#include "itemwidget.h"

class IconTile;

// "框"：把若干图标圈在一个可视方框里，图标缩小显示且可直接点击
class GroupItemWidget : public ItemWidget
{
    Q_OBJECT
public:
    explicit GroupItemWidget(const LayoutItem &item, QWidget *parent = nullptr);

    void setLayoutItem(const LayoutItem &item);
    void setColor(const QString &color);
    const LayoutItem &layoutItem() const { return m_item; }
    QString groupName() const { return m_item.name; }
    QSize computeSize() const;

    void rebuildTiles();

signals:
    void openRequested(const QString &groupId);
    void childLaunchRequested(const QString &groupId, const QString &childId, const QString &path);
    void childContextRequested(const QString &groupId, const QString &childId,
                               const QString &path, const QPoint &globalPos);
    // 框内图标被拖到框外（canvasPos 是画布坐标），交给画布把它变成独立图标
    void tileDragOut(const QString &groupId, const QString &childId, const QPoint &canvasPos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void relayoutTiles();
    // 本地坐标落在第几个格子上（-1 = 空白处 / 「打开」那格）
    int tileIndexAt(const QPoint &localPos) const;
    void setDropIndex(int index);
    void clearDragState();

    LayoutItem m_item;
    QString m_title;
    QString m_dragChildId;
    int m_dragChildIndex = -1; // 正在拖的图标在 children 里的下标
    int m_dropIndex = -1;      // 光标当前悬停的目标格
    int m_visibleCount = 0;    // 真正画出来的图标数（不含溢出的「打开」格）
    QVector<IconTile *> m_tiles;
};
