#pragma once

#include <QHash>
#include <QPointer>
#include <QString>
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
    void closePopup();

    // 加入新内容后把它滚进视野并闪一下，让用户确认"加进去了"
    void flashItem(const QString &id);

signals:
    void requestHide();
    void launchFailed(const QString &path);
    // 内容比面板高/宽时，把滚轮手势交给主窗口处理（Shift+滚轮走横向）
    void scrollRequested(int delta);
    void hScrollRequested(int delta);
    void requestReveal(const QRect &canvasRect);
    // 内容尺寸变了（比如图标被拖到最右边），窗口要跟着调
    void requestResize();

protected:
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
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

    void addFilesTo(const QString &groupId);
    void addFolderTo(const QString &groupId);
    // 商店应用（UWP）没有文件路径，走这个选择框添加
    void addWindowsAppsTo(const QString &groupId);
    void renameItemDialog(const QString &id);
    void editArgsDialog(const QString &id);
    void ensureVisible(const QString &id);

    QHash<QString, class ItemWidget *> m_itemWidgets;
    QHash<QString, GroupItemWidget *> m_groupWidgets;
    // QPointer：弹层是 WA_DeleteOnClose 的独立窗口，销毁时这里自动变空，
    // 不会再出现"旧弹层的 destroyed 把新弹层的指针清掉"的问题
    QPointer<GroupPopup> m_popup;
    QString m_pendingFlash;
    QString m_dropTarget;
    bool m_rebuildPending = false;
};
