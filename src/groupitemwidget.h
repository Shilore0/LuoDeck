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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void relayoutTiles();

    LayoutItem m_item;
    QString m_title;
    QVector<IconTile *> m_tiles;
    bool m_externalDrag = false;
};
