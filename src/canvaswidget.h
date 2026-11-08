#pragma once

#include <QHash>
#include <QWidget>

#include "appconfig.h"

class GroupItemWidget;
class GroupPopup;

// 自由布局画布：承载所有图标与"框"，负责拖放、右键菜单与位置持久化
class CanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    void rebuildLayout();
    void scheduleRebuild();
    bool isPopupOpen() const;

signals:
    void requestHide();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void clearWidgets();
    void addAppWidget(const LayoutItem &item);
    void addGroupWidget(const LayoutItem &item);
    void handleActivated(const QString &id);
    void handleDragFinished(const QString &id, const QPoint &pos);
    void handleContext(const QString &id, const QPoint &globalPos);
    void showGroupMenu(const QString &id, const QPoint &globalPos);
    void showAppMenu(const QString &id, const QPoint &globalPos);
    void showBlankMenu(const QPoint &globalPos);
    void openGroupPopup(const QString &id, const QPoint &globalPos);
    QString groupIdAt(const QPoint &pos) const;

    QHash<QString, GroupItemWidget *> m_groupWidgets;
    GroupPopup *m_popup = nullptr;
    bool m_rebuildPending = false;
};
