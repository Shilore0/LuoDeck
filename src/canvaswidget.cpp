#include "canvaswidget.h"

#include "appitemwidget.h"
#include "foldersettingsdialog.h"
#include "grouppopup.h"
#include "groupitemwidget.h"
#include "i18n.h"
#include "launcher.h"
#include "layoutengine.h"

#include "metrics.h"
#include "theme.h"
#include "winappdialog.h"
#include "windowfx.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>

namespace {

// 面板本身是置顶工具窗，文件对话框要挂到它上面才不会跑到后面去
QString pickStartDir()
{
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    return desktop.isEmpty() ? QDir::homePath() : desktop;
}

} // namespace

CanvasWidget::CanvasWidget(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    // 每帧都会填满脏区，告诉 Qt 不必再做一次背景擦除
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setContextMenuPolicy(Qt::DefaultContextMenu);

    connect(&AppConfig::instance(), &AppConfig::layoutChanged,
            this, &CanvasWidget::scheduleRebuild);

    // 只改颜色时直接更新对应控件，不重建整块画布
    connect(&AppConfig::instance(), &AppConfig::itemVisualChanged, this,
            [this](const QString &id) {
                GroupItemWidget *w = m_groupWidgets.value(id);
                if (!w)
                    return;
                const LayoutItem *item = AppConfig::instance().findById(id);
                if (item)
                    w->setColor(item->color);
            });
}

bool CanvasWidget::isPopupOpen() const
{
    return m_popup != nullptr && m_popup->isVisible();
}

void CanvasWidget::closePopup()
{
    if (!m_popup)
        return;
    m_popup->close();
    m_popup = nullptr;
}

void CanvasWidget::scheduleRebuild()
{
    if (m_rebuildPending)
        return;

    // 鼠标还按着（可能正在拖东西）时先缓一缓：桌面监听会在任意时刻触发重建，
    // 正在拖的控件被删掉就会丢手感
    if (QGuiApplication::mouseButtons() & Qt::LeftButton) {
        QTimer::singleShot(160, this, [this] { scheduleRebuild(); });
        return;
    }

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
    m_itemWidgets.clear();
}

void CanvasWidget::flashItem(const QString &id)
{
    if (id.isEmpty())
        return;
    m_pendingFlash = id;
    scheduleRebuild();
}

void CanvasWidget::ensureVisible(const QString &id)
{
    QWidget *w = m_itemWidgets.value(id);
    if (!w)
        return;
    emit requestReveal(w->geometry());
}

// 画布自己铺一层实心底色，否则子控件移动/删除后会留下半透明残影
void CanvasWidget::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    QColor bg = Theme::instance().color("window");
    bg.setAlpha(255);
    p.fillRect(event->rect(), bg);

    // 拖动文件经过某个框时，把那个框圈出来
    if (!m_dropTarget.isEmpty()) {
        if (GroupItemWidget *w = m_groupWidgets.value(m_dropTarget)) {
            QColor accent = Theme::instance().color("accent");
            accent.setAlpha(230);
            QPen pen(accent, 2.0, Qt::DashLine);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(w->geometry()).adjusted(-3, -3, 3, 3),
                              Metrics::kGroupRadius, Metrics::kGroupRadius);
        }
    }

    // 空状态：什么都还没有的时候给一句提示，而不是一块空白
    bool hasContent = false;
    for (const LayoutItem &it : AppConfig::instance().items()) {
        if (it.isGroup() && it.system && AppConfig::instance().hideDesktopFrame())
            continue;
        hasContent = true;
        break;
    }
    if (!hasContent) {
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont f = font();
        f.setPixelSize(13);
        p.setFont(f);
        QColor muted = Theme::instance().color("textMuted");
        muted.setAlpha(190);
        p.setPen(muted);
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
                   I18n::instance().t("canvas.empty"));
    }
}

void CanvasWidget::rebuildLayout()
{
    clearWidgets();

    AppConfig &cfg = AppConfig::instance();
    for (const LayoutItem &item : cfg.items()) {
        if (item.isGroup() && item.system && cfg.hideDesktopFrame())
            continue;
        if (item.isGroup())
            addGroupWidget(item);
        else
            addAppWidget(item);
    }

    if (!m_pendingFlash.isEmpty()) {
        const QString id = m_pendingFlash;
        m_pendingFlash.clear();
        if (auto *w = m_itemWidgets.value(id)) {
            w->raise();
            w->flashHighlight();
            ensureVisible(id);
        }
    }
}

void CanvasWidget::addAppWidget(const LayoutItem &item)
{
    auto *w = new AppItemWidget(item.id, item.name, item.path, this);
    w->move(item.x, item.y);
    w->show();
    m_itemWidgets.insert(item.id, w);

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
    m_itemWidgets.insert(item.id, w);

    connect(w, &ItemWidget::activated, this, &CanvasWidget::handleActivated);
    connect(w, &ItemWidget::dragFinished, this, &CanvasWidget::handleDragFinished);
    connect(w, &ItemWidget::contextRequested, this, &CanvasWidget::handleContext);

    connect(w, &GroupItemWidget::openRequested, this, [this](const QString &id) {
        openGroupPopup(id, QCursor::pos());
    });

    connect(w, &GroupItemWidget::childLaunchRequested, this,
            [this](const QString &, const QString &childId, const QString &path) {
                const LayoutItem *child = AppConfig::instance().findById(childId);
                const QString args = child ? child->args : QString();
                emit requestHide();
                if (!Launcher::run(path, args))
                    emit launchFailed(path);
            });

    connect(w, &GroupItemWidget::tileDragOut, this,
            [this](const QString &, const QString &childId, const QPoint &canvasPos) {
                const QString newId = AppConfig::instance().detachChild(childId, canvasPos);
                if (!newId.isEmpty())
                    flashItem(newId);
            });

    connect(w, &GroupItemWidget::childContextRequested, this,
            [this](const QString &groupId, const QString &childId, const QString &path,
                   const QPoint &globalPos) {
                const LayoutItem *group = AppConfig::instance().findById(groupId);
                const bool systemFrame = group && group->system;
                const LayoutItem *child = AppConfig::instance().findById(childId);
                const QString args = child ? child->args : QString();

                QMenu menu(this);
                menu.addAction(I18n::instance().t("item.open"), this, [this, path, args] {
                    emit requestHide();
                    if (!Launcher::run(path, args))
                        emit launchFailed(path);
                });
                menu.addAction(I18n::instance().t("item.reveal"), this, [path] {
                    Launcher::revealInExplorer(path);
                });
                menu.addAction(I18n::instance().t("item.args"), this, [this, childId] {
                    editArgsDialog(childId);
                });
                menu.addAction(I18n::instance().t("item.rename"), this, [this, childId] {
                    renameItemDialog(childId);
                });
                menu.addSeparator();
                menu.addAction(systemFrame ? I18n::instance().t("item.hide_desktop")
                                           : I18n::instance().t("item.remove"),
                               this, [this, systemFrame, childId, path] {
                                   if (systemFrame) {
                                       AppConfig::instance().hidePath(path);
                                   } else {
                                       AppConfig::instance().removeItem(childId);
                                   }
                               });
                menu.exec(globalPos);
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
    if (!Launcher::run(item->path, item->args))
        emit launchFailed(item->path);
}

void CanvasWidget::handleDragFinished(const QString &id, const QPoint &pos)
{
    AppConfig &cfg = AppConfig::instance();
    const LayoutItem *dragged = cfg.findById(id);
    if (!dragged)
        return;

    // 全部按格子走：先吸附到最近的格点，再判断这一格上有没有别的东西
    const QPoint snapped = LayoutEngine::snap(pos);
    const QRect cell = LayoutEngine::cellRect(snapped);
    const QPoint center = cell.center();

    // 1) 落在某个框上 → 收进框里（系统框不收）
    if (!dragged->isGroup()) {
        const QString groupId = groupIdAt(center);
        if (!groupId.isEmpty() && groupId != id) {
            if (cfg.moveItemIntoGroup(id, groupId))
                return;
            cfg.moveItemToFreeSpot(id);
            scheduleRebuild();
            return;
        }
    }

    // 2) 落在另一个图标上 → 两个合成一个文件夹
    if (!dragged->isGroup()) {
        QString hitId;
        for (const LayoutItem &other : cfg.items()) {
            if (other.isGroup() || other.id == id)
                continue;
            if (LayoutEngine::itemRect(other).contains(center)) {
                hitId = other.id;
                break;
            }
        }
        if (!hitId.isEmpty()) {
            const QString folderId = cfg.mergeIntoFolder(id, hitId);
            if (!folderId.isEmpty()) {
                flashItem(folderId);
                return;
            }
        }
    }

    cfg.moveItem(id, snapped);

    // 关键：光写配置不够，控件还停在鼠标松手的地方，得当场挪到格点上，
    // 否则要等下次重建（重启）才归位
    if (QWidget *w = m_itemWidgets.value(id)) {
        w->move(snapped);
        w->raise();
    }
    update();

    // 拖动的是框：既要把被压住的图标挪开，也要避免压在别的框上
    if (dragged->isGroup()) {
        cfg.resolveGroupOverlaps();
        cfg.pushOverlaps(id);
        scheduleRebuild(); // 框自己或邻居可能被挪了位置
    }

    emit requestResize();
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
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item)
        return;

    const bool isSystem = item->system;

    QMenu menu(this);
    menu.addAction(I18n::instance().t("group.open"), this, [this, id] {
        const auto *w = m_groupWidgets.value(id);
        const QPoint anchor = w ? w->mapToGlobal(w->rect().center()) : QCursor::pos();
        openGroupPopup(id, anchor);
    });

    // 「桌面」框的内容跟桌面目录走，不提供手动添加
    if (!isSystem) {
        menu.addAction(I18n::instance().t("group.add_file"), this, [this, id] {
            addFilesTo(id);
        });
        menu.addAction(I18n::instance().t("canvas.add_winapp"), this, [this, id] {
            addWindowsAppsTo(id);
        });
    }

    menu.addAction(I18n::instance().t("group.settings"), this, [this, id] {
        const LayoutItem *target = AppConfig::instance().findById(id);
        if (!target)
            return;

        FolderSettingsDialog dlg(*target, this);

        // 颜色点一下就生效，主面板能立刻看到效果
        connect(&dlg, &FolderSettingsDialog::colorPreviewed, this, [id](const QString &color) {
            AppConfig::instance().setGroupColor(id, color);
        });

        // 摆到主面板旁边，别把面板挡住
        if (QWidget *top = window())
            placeBeside(&dlg, top->frameGeometry());

        const int result = dlg.exec();

        // ⚠️ 不能再用 target：它指向 m_items 内部，dlg.exec() 的嵌套事件循环里
        // 桌面监听随时可能 refreshDesktop() 增删 m_items，指针早已失效。
        // 界面状态一律按 id 重新查。
        if (result == QDialog::Accepted) {
            const LayoutItem *fresh = AppConfig::instance().findById(id);
            if (fresh && !fresh->system) {
                const QString name = dlg.folderName();
                if (!name.isEmpty())
                    AppConfig::instance().renameGroup(id, name);
            }
        }
    });

    // 容量：系统「桌面」框和用户自己建的框都能设。
    // 小文件夹本来就是"一格"，给它选容量等于要展开，所以选了就直接展开成大框。
    QMenu *sizeMenu = menu.addMenu(I18n::instance().t("group.capacity"));
    {
        QAction *autoAct =
            sizeMenu->addAction(I18n::instance().t("group.capacity_auto"), this, [id] {
                AppConfig::instance().setGroupAutoGrid(id);
            });
        autoAct->setCheckable(true);
        autoAct->setChecked(!item->manualGrid);
        sizeMenu->addSeparator();

        struct Preset { int cols; int rows; };
        const QVector<Preset> presets = {{2, 1}, {2, 2}, {3, 2}, {3, 3},
                                         {3, 4}, {4, 3}, {4, 4}, {5, 4}};
        for (const Preset &p : presets) {
            const QString label = QStringLiteral("%1 × %2").arg(p.cols).arg(p.rows);
            QAction *act = sizeMenu->addAction(label, this, [id, p] {
                AppConfig::instance().setGroupGrid(id, p.cols, p.rows);
            });
            act->setCheckable(true);
            act->setChecked(item->manualGrid && item->cols == p.cols && item->rows == p.rows);
        }
    }

    if (!isSystem) {
        menu.addAction(item->compact ? I18n::instance().t("group.expand")
                                     : I18n::instance().t("group.compact"),
                       this, [id, wantCompact = !item->compact] {
                           AppConfig::instance().setGroupCompact(id, wantCompact);
                       });
    }

    menu.addSeparator();
    menu.addAction(I18n::instance().t("group.sort"), this, [id] {
        AppConfig::instance().sortGroup(id);
    });
    QAction *clear = menu.addAction(I18n::instance().t("group.clear"), this, [id] {
        AppConfig::instance().clearGroup(id);
    });
    clear->setEnabled(!isSystem);

    menu.addSeparator();
    QAction *dissolve = menu.addAction(I18n::instance().t("group.dissolve"), this, [id] {
        AppConfig::instance().dissolveGroup(id);
    });
    dissolve->setEnabled(!isSystem);

    QAction *del = menu.addAction(I18n::instance().t("group.delete"), this, [id] {
        AppConfig::instance().removeItem(id);
    });
    del->setEnabled(!isSystem);

    menu.exec(globalPos);
}

void CanvasWidget::showAppMenu(const QString &id, const QPoint &globalPos)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item)
        return;

    const QString path = item->path;
    const QString args = item->args;

    QMenu menu(this);
    menu.addAction(I18n::instance().t("item.open"), this, [this, path, args] {
        emit requestHide();
        if (!Launcher::run(path, args))
            emit launchFailed(path);
    });
    menu.addAction(I18n::instance().t("item.reveal"), this, [path] {
        Launcher::revealInExplorer(path);
    });
    menu.addAction(I18n::instance().t("item.args"), this, [this, id] {
        editArgsDialog(id);
    });
    menu.addAction(I18n::instance().t("item.rename"), this, [this, id] {
        renameItemDialog(id);
    });
    menu.addSeparator();
    menu.addAction(I18n::instance().t("item.remove"), this, [id] {
        AppConfig::instance().removeItem(id);
    });
    menu.exec(globalPos);
}

void CanvasWidget::showBlankMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.addAction(I18n::instance().t("canvas.add_file"), this, [this] {
        addFilesTo(QString());
    });
    menu.addAction(I18n::instance().t("canvas.add_folder"), this, [this] {
        addFolderTo(QString());
    });
    menu.addAction(I18n::instance().t("canvas.add_winapp"), this, [this] {
        addWindowsAppsTo(QString());
    });
    menu.addSeparator();
    menu.addAction(I18n::instance().t("group.new"), this, [] {
        AppConfig::instance().addGroup();
    });
    menu.addAction(I18n::instance().t("canvas.arrange_f5"), this, [] {
        AppConfig::instance().autoArrange();
    });
    menu.addAction(I18n::instance().t("canvas.compact"), this, [] {
        AppConfig::instance().reflowLayout();
    });
    menu.addAction(I18n::instance().t("canvas.refresh"), this, [this] {
        rebuildLayout();
    });
    menu.exec(globalPos);
}

void CanvasWidget::addFilesTo(const QString &groupId)
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, I18n::instance().t("canvas.add_file"), pickStartDir(),
        I18n::instance().t("canvas.file_filter"));

    QString last;
    for (const QString &f : files) {
        const QString id = AppConfig::instance().addApp(f, groupId);
        if (!id.isEmpty())
            last = id;
    }
    if (!last.isEmpty())
        flashItem(groupId.isEmpty() ? last : groupId);
}

void CanvasWidget::addFolderTo(const QString &groupId)
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, I18n::instance().t("canvas.add_folder"), pickStartDir());
    if (dir.isEmpty())
        return;

    const QString id = AppConfig::instance().addApp(dir, groupId);
    if (!id.isEmpty())
        flashItem(groupId.isEmpty() ? id : groupId);
}

void CanvasWidget::addWindowsAppsTo(const QString &groupId)
{
    WinAppDialog dlg(this);
    if (QWidget *top = window())
        placeBeside(&dlg, top->frameGeometry());

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString last;
    const QStringList aumids = dlg.selectedAumids();
    for (const QString &aumid : aumids) {
        const QString id = AppConfig::instance().addShellApp(aumid, dlg.nameFor(aumid), groupId);
        if (!id.isEmpty())
            last = id;
    }

    if (!last.isEmpty())
        flashItem(groupId.isEmpty() ? last : groupId);
}

void CanvasWidget::renameItemDialog(const QString &id)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item || item->system)
        return;

    // 自己搭 QInputDialog 而不是用静态 getText：这样按钮文字能跟着语言走（否则永远是 OK / Cancel）
    QInputDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("%1 - %2").arg(I18n::instance().t("item.rename"), item->name));
    dlg.setLabelText(I18n::instance().t("item.rename_prompt"));
    dlg.setTextValue(item->name);
    dlg.setOkButtonText(I18n::instance().t("settings.ok"));
    dlg.setCancelButtonText(I18n::instance().t("settings.cancel"));
    if (QWidget *top = window())
        placeBeside(&dlg, top->frameGeometry());

    dlg.winId();
    applyFrameTheme(&dlg);

    if (dlg.exec() != QDialog::Accepted)
        return;

    AppConfig::instance().renameItem(id, dlg.textValue());
}

void CanvasWidget::editArgsDialog(const QString &id)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item || item->isGroup())
        return;

    const I18n &tr = I18n::instance();
    const Theme &th = Theme::instance();
    const QColor textColor = th.color("text");
    const QColor muted = th.color("textMuted");

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("%1 - %2").arg(tr.t("item.args"), item->name));
    dlg.setMinimumWidth(480);

    auto *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(18, 16, 18, 14);
    outer->setSpacing(7);

    // 到底在改哪一个：名字 + 真实路径（快捷方式再补一行它指向谁）
    auto *nameLab = new QLabel(item->name, &dlg);
    QFont nf = nameLab->font();
    nf.setPixelSize(15);
    nf.setBold(true);
    nameLab->setFont(nf);
    nameLab->setStyleSheet(QStringLiteral("color:%1;").arg(textColor.name(QColor::HexRgb)));
    outer->addWidget(nameLab);

    QFileInfo fi(item->path);
    auto *pathLab = new QLabel(QDir::toNativeSeparators(fi.absoluteFilePath()), &dlg);
    pathLab->setWordWrap(true);
    pathLab->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLab->setStyleSheet(QStringLiteral("color:%1;").arg(muted.name(QColor::HexRgb)));
    QFont pf = pathLab->font();
    pf.setPixelSize(11);
    pathLab->setFont(pf);
    outer->addWidget(pathLab);

    if (fi.suffix().compare(QStringLiteral("lnk"), Qt::CaseInsensitive) == 0) {
        const QString target = fi.symLinkTarget();
        const QString shown = target.isEmpty() ? tr.t("args.no_target")
                                               : tr.t("args.target").arg(QDir::toNativeSeparators(target));
        auto *targetLab = new QLabel(shown, &dlg);
        targetLab->setWordWrap(true);
        targetLab->setTextInteractionFlags(Qt::TextSelectableByMouse);
        targetLab->setStyleSheet(QStringLiteral("color:%1;").arg(muted.name(QColor::HexRgb)));
        targetLab->setFont(pf);
        outer->addWidget(targetLab);
    }

    auto *hint = new QLabel(tr.t("args.hint"), &dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1;").arg(muted.name(QColor::HexRgb)));
    QFont hf = hint->font();
    hf.setPixelSize(11);
    hint->setFont(hf);
    outer->addWidget(hint);

    auto *argsLab = new QLabel(tr.t("args.label"), &dlg);
    argsLab->setStyleSheet(QStringLiteral("color:%1;").arg(textColor.name(QColor::HexRgb)));
    outer->addSpacing(2);
    outer->addWidget(argsLab);

    auto *edit = new QPlainTextEdit(&dlg);
    edit->setPlainText(item->args);
    edit->setPlaceholderText(QStringLiteral("-server 127.0.0.1:8080"));
    edit->setFixedHeight(92);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    outer->addWidget(edit);

    auto *buttons = new QDialogButtonBox(&dlg);
    QPushButton *okBtn = buttons->addButton(tr.t("settings.ok"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn =
        buttons->addButton(tr.t("settings.cancel"), QDialogButtonBox::RejectRole);
    okBtn->setDefault(true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    Q_UNUSED(cancelBtn)
    outer->addWidget(buttons);

    if (QWidget *top = window())
        placeBeside(&dlg, top->frameGeometry());

    dlg.winId(); // 先建出窗口，标题栏才能跟着主题上色
    applyFrameTheme(&dlg);

    edit->setFocus();
    edit->moveCursor(QTextCursor::End);

    if (dlg.exec() == QDialog::Accepted)
        AppConfig::instance().setItemArgs(id, edit->toPlainText());
}

void CanvasWidget::openGroupPopup(const QString &id, const QPoint &globalPos)
{
    const LayoutItem *item = AppConfig::instance().findById(id);
    if (!item || !item->isGroup())
        return;

    if (m_popup) {
        m_popup->close(); // WA_DeleteOnClose → deleteLater，事件循环里销毁
        m_popup = nullptr;
    }

    auto *popup = new GroupPopup(*item, nullptr); // 独立顶层窗口，主面板隐藏时不受牵连
    m_popup = popup;

    connect(popup, &GroupPopup::childLaunchRequested, this,
            [this](const QString &path, const QString &args) {
                emit requestHide();
                if (!Launcher::run(path, args))
                    emit launchFailed(path);
            });
    connect(popup, &GroupPopup::arrangeRequested, this, [] {
        AppConfig::instance().autoArrange();
    });

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
    if (!event->mimeData()->hasUrls())
        return;

    QString target = groupIdAt(event->position().toPoint());
    // 「桌面」框不收外部拖入，别给它画落点提示
    if (const LayoutItem *g = AppConfig::instance().findById(target)) {
        if (g->system)
            target.clear();
    }

    if (target != m_dropTarget) {
        m_dropTarget = target;
        update();
    }
    event->acceptProposedAction();
}

void CanvasWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (!m_dropTarget.isEmpty()) {
        m_dropTarget.clear();
        update();
    }
    QWidget::dragLeaveEvent(event);
}

void CanvasWidget::dropEvent(QDropEvent *event)
{
    const QString targetGroup = groupIdAt(event->position().toPoint());
    const QList<QUrl> urls = event->mimeData()->urls();

    QString last;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QString id = AppConfig::instance().addApp(url.toLocalFile(), targetGroup);
        if (!id.isEmpty())
            last = id;
    }

    if (!m_dropTarget.isEmpty()) {
        m_dropTarget.clear();
        update();
    }

    if (!last.isEmpty())
        flashItem(targetGroup.isEmpty() ? last : targetGroup);

    event->acceptProposedAction();
}

void CanvasWidget::wheelEvent(QWheelEvent *event)
{
    const QPoint delta = event->angleDelta();
    // Shift+滚轮（或触控板横向手势）滚横向，其余滚纵向
    if (qAbs(delta.x()) > qAbs(delta.y()))
        emit hScrollRequested(delta.x());
    else
        emit scrollRequested(delta.y());
    event->accept();
}

void CanvasWidget::contextMenuEvent(QContextMenuEvent *event)
{
    showBlankMenu(event->globalPos());
    event->accept();
}
