#include "canvaswidget.h"

#include "appitemwidget.h"
#include "grouppopup.h"
#include "groupitemwidget.h"
#include "i18n.h"
#include "launcher.h"
#include "metrics.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QTimer>
#include <QUrl>

CanvasWidget::CanvasWidget(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    connect(&AppConfig::instance(), &AppConfig::layoutChanged,
            this, &CanvasWidget::scheduleRebuild);
}

bool CanvasWidget::isPopupOpen() const
{
    return m_popup != nullptr && m_popup->isVisible();
}

void CanvasWidget::scheduleRebuild()
{
    if (m_rebuildPending)
        return;
    m_rebuildPending = true;
    QTimer::singleShot(0, this, [this] {
        m_rebuildPending = false;
        rebuildLayout();
    });
}

void CanvasWidget::clearWidgets()
{
    const auto kids = findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *w : kids) {
        w->hide();
        w->deleteLater();
    }
    m_groupWidgets.clear();
}

void CanvasWidget::rebuildLayout()
{
    clearWidgets();

    AppConfig &cfg = AppConfig::instance();
    for (const LayoutItem &item : cfg.items()) {
        if (item.isGroup())
            addGroupWidget(item);
        else
            addAppWidget(item);
    }
}

void CanvasWidget::addAppWidget(const LayoutItem &item)
{
    auto *w = new AppItemWidget(item.id, item.name, item.path, this);
    w->move(item.x, item.y);
    w->show();

    connect(w, &ItemWidget::activated, this, &CanvasWidget::handleActivated);
    connect(w, &ItemWidget::dragFinished, this, &CanvasWidget::handleDragFinished);
    connect(w, &ItemWidget::contextRequested, this, &CanvasWidget::handleContext);
}

void CanvasWidget::addGroupWidget(const LayoutItem &item)
{
    auto *w = new GroupItemWidget(item, this);
    w->move(item.x, item.y);
    w->show();
    m_groupWidgets.insert(item.id, w);

    connect(w, &ItemWidget::activated, this, &CanvasWidget::handleActivated);
    connect(w, &ItemWidget::dragFinished, this, &CanvasWidget::handleDragFinished);
    connect(w, &ItemWidget::contextRequested, this, &CanvasWidget::handleContext);

    connect(w, &GroupItemWidget::openRequested, this, [this](const QString &id) {
        openGroupPopup(id, QCursor::pos());
    });

    connect(w, &GroupItemWidget::childLaunchRequested, this,
            [this](const QString &, const QString &, const QString &path) {
                emit requestHide();
                Launcher::run(path);
            });
}

void CanvasWidget::handleActivated(const QString &id)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item)
        return;

    if (item->isGroup()) {
        const auto *w = m_groupWidgets.value(id);
        const QPoint anchor = w ? w->mapToGlobal(w->rect().center()) : QCursor::pos();
        openGroupPopup(id, anchor);
        return;
    }

    emit requestHide();
    Launcher::run(item->path);
}

void CanvasWidget::handleDragFinished(const QString &id, const QPoint &pos)
{
    AppConfig::instance().moveItem(id, pos);
}

void CanvasWidget::handleContext(const QString &id, const QPoint &globalPos)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item)
        return;

    if (item->isGroup())
        showGroupMenu(id, globalPos);
    else
        showAppMenu(id, globalPos);
}

void CanvasWidget::showGroupMenu(const QString &id, const QPoint &globalPos)
{
    QMenu menu(this);
    menu.addAction(I18n::instance().t("group.open"), this, [this, id] {
        const auto *w = m_groupWidgets.value(id);
        const QPoint anchor = w ? w->mapToGlobal(w->rect().center()) : QCursor::pos();
        openGroupPopup(id, anchor);
    });

    menu.addAction(I18n::instance().t("group.rename"), this, [this, id] {
        const LayoutItem *item = AppConfig::instance().findById(id);
        if (!item)
            return;
        bool ok = false;
        const QString name = QInputDialog::getText(this,
                                                   I18n::instance().t("group.rename"),
                                                   I18n::instance().t("group.rename_prompt"),
                                                   QLineEdit::Normal, item->name, &ok);
        if (ok && !name.trimmed().isEmpty())
            AppConfig::instance().renameGroup(id, name.trimmed());
    });

    QMenu *sizeMenu = menu.addMenu(I18n::instance().t("group.capacity"));
    struct Preset { int cols; int rows; };
    const QVector<Preset> presets = {{2, 2}, {3, 3}, {3, 4}, {4, 3}, {4, 4}};
    for (const Preset &p : presets) {
        const QString label = QStringLiteral("%1 × %2").arg(p.cols).arg(p.rows);
        sizeMenu->addAction(label, this, [this, id, p] {
            AppConfig::instance().setGroupGrid(id, p.cols, p.rows);
        });
    }

    menu.addSeparator();
    menu.addAction(I18n::instance().t("group.delete"), this, [this, id] {
        AppConfig::instance().removeItem(id);
    });

    menu.exec(globalPos);
}

void CanvasWidget::showAppMenu(const QString &id, const QPoint &globalPos)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item)
        return;

    const QString path = item->path;

    QMenu menu(this);
    menu.addAction(I18n::instance().t("item.reveal"), this, [path] {
        Launcher::revealInExplorer(path);
    });
    menu.addSeparator();
    menu.addAction(I18n::instance().t("item.remove"), this, [this, id] {
        AppConfig::instance().removeItem(id);
    });
    menu.exec(globalPos);
}

void CanvasWidget::showBlankMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.addAction(I18n::instance().t("group.new"), this, [this] {
        AppConfig::instance().addGroup();
    });
    menu.addAction(I18n::instance().t("canvas.refresh"), this, [this] {
        rebuildLayout();
    });
    menu.exec(globalPos);
}

void CanvasWidget::openGroupPopup(const QString &id, const QPoint &globalPos)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item || !item->isGroup())
        return;

    if (m_popup) {
        m_popup->close();
        m_popup = nullptr;
    }

    auto *popup = new GroupPopup(*item, this);
    m_popup = popup;

    connect(popup, &GroupPopup::childLaunchRequested, this, [this](const QString &path) {
        emit requestHide();
        Launcher::run(path);
    });
    connect(popup, &QObject::destroyed, this, [this] { m_popup = nullptr; });

    popup->popupAt(globalPos);
}

QString CanvasWidget::groupIdAt(const QPoint &pos) const
{
    for (auto it = m_groupWidgets.constBegin(); it != m_groupWidgets.constEnd(); ++it) {
        if (it.value()->geometry().contains(pos))
            return it.key();
    }
    return QString();
}

void CanvasWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void CanvasWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void CanvasWidget::dropEvent(QDropEvent *event)
{
    const QString targetGroup = groupIdAt(event->position().toPoint());
    const QList<QUrl> urls = event->mimeData()->urls();

    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        AppConfig::instance().addApp(url.toLocalFile(), targetGroup);
    }

    event->acceptProposedAction();
}

void CanvasWidget::contextMenuEvent(QContextMenuEvent *event)
{
    showBlankMenu(event->globalPos());
    event->accept();
}
