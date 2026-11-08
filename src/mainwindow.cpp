#include "mainwindow.h"

#include "appconfig.h"
#include "i18n.h"
#include "iconprovider.h"
#include "launcher.h"
#include "theme.h"
#include "tiledelegate.h"
#include "tilemodel.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAcceptDrops(true);
    setWindowTitle("LuoDeck");

    buildUi();

    connect(&AppConfig::instance(), &AppConfig::changed, this, &MainWindow::reload);
    connect(&I18n::instance(), &I18n::languageChanged, this, &MainWindow::retranslateUi);
    connect(&Theme::instance(), &Theme::themeChanged, this, &MainWindow::applyTheme);

    applyTheme();
    m_model->rebuild();
    updateEmptyState();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);

    m_view = new QListView(this);
    m_view->setObjectName("TileView");
    m_view->setViewMode(QListView::IconMode);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setUniformItemSizes(true);
    m_view->setSpacing(0);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setTextElideMode(Qt::ElideRight);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setMouseTracking(true);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setFrameShape(QFrame::NoFrame);

    m_model = new TileModel(this);
    m_delegate = new TileDelegate(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(m_delegate);

    layout->addWidget(m_view);

    m_empty = new QLabel(m_view->viewport());
    m_empty->setObjectName("EmptyHint");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_empty->hide();

    connect(m_view, &QListView::clicked, this, &MainWindow::onItemActivated);
    connect(m_view, &QListView::customContextMenuRequested, this, &MainWindow::onContextMenu);

    retranslateUi();
}

void MainWindow::retranslateUi()
{
    if (m_empty)
        m_empty->setText(I18n::instance().t("ui.empty"));
    rebuildTrayMenu();
    updateEmptyState();
}

void MainWindow::applyStyle()
{
    qApp->setStyleSheet(Theme::instance().styleSheet());
}

void MainWindow::applyTheme()
{
    applyStyle();

    const int iconSize = AppConfig::instance().iconSize();
    const int cols = qMax(3, AppConfig::instance().columns());
    const QSize cell(iconSize + 40, iconSize + 46);

    if (m_delegate)
        m_delegate->setCellSize(cell);
    if (m_view)
        m_view->setGridSize(cell);

    const int width = cols * cell.width() + 28 + 12;
    const int height = qBound(320, cell.height() * 4 + 60, 640);
    resize(width, height);

    if (m_tray)
        m_tray->setIcon(makeTrayIcon());

    update();
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const Theme &th = Theme::instance();
    QColor bg = th.color("window");
    QColor border = th.color("border");
    border.setAlpha(120);

    p.setPen(QPen(border, 1));
    p.setBrush(bg);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), th.radius(), th.radius());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_empty && m_view)
        m_empty->setGeometry(m_view->viewport()->rect());
}

void MainWindow::positionNearCursor()
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect avail = screen->availableGeometry();
    const QPoint cursor = QCursor::pos();

    int x = cursor.x() - width() / 2;
    int y = cursor.y() - height() / 2;

    x = qBound(avail.left() + 8, x, avail.right() - width() - 8);
    y = qBound(avail.top() + 8, y, avail.bottom() - height() - 8);
    move(x, y);
}

void MainWindow::showPanel()
{
    m_allowAutoHide = false;
    m_model->rebuild();
    updateEmptyState();
    positionNearCursor();
    show();
    raise();
    activateWindow();
    m_view->setFocus();
    QTimer::singleShot(220, this, [this] { m_allowAutoHide = true; });
}

void MainWindow::hidePanel()
{
    m_model->setFolderCollapsedAll();
    hide();
}

void MainWindow::toggleVisible()
{
    if (isVisible())
        hidePanel();
    else
        showPanel();
}

void MainWindow::reload()
{
    IconProvider::instance().clear();
    m_model->rebuild();
    updateEmptyState();
}

void MainWindow::updateEmptyState()
{
    if (!m_empty || !m_model)
        return;
    const bool empty = m_model->rowCount() == 0;
    m_empty->setVisible(empty);
    if (empty && m_view)
        m_empty->setGeometry(m_view->viewport()->rect());
}

void MainWindow::onItemActivated(const QModelIndex &index)
{
    const TileModel::Row *row = m_model->rowAt(index.row());
    if (!row)
        return;

    if (row->entry.isFolder && !row->isChild) {
        m_model->toggleFolderAt(index.row());
        updateEmptyState();
        return;
    }

    hidePanel();
    Launcher::run(row->entry.path);
}

void MainWindow::onContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    QMenu menu(this);

    if (index.isValid()) {
        const TileModel::Row *row = m_model->rowAt(index.row());
        if (row) {
            if (!row->isChild) {
                const QString id = row->entry.id;
                menu.addAction(I18n::instance().t("ctx.remove"), this, [this, id] {
                    AppConfig::instance().removeById(id);
                });
            }
            const QString path = row->entry.path;
            menu.addAction(I18n::instance().t("ctx.reveal"), this, [path] {
                Launcher::revealInExplorer(path);
            });
            if (row->entry.isFolder) {
                menu.addAction(I18n::instance().t("ctx.open_folder"), this, [path] {
                    Launcher::openContainingFolder(path);
                });
            }
            menu.addSeparator();
        }
    }

    menu.addAction(I18n::instance().t("tray.reload"), this, &MainWindow::reload);
    m_allowAutoHide = false;
    menu.exec(m_view->viewport()->mapToGlobal(pos));
    QTimer::singleShot(220, this, [this] { m_allowAutoHide = true; });
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        AppConfig::instance().addPath(url.toLocalFile());
    }
    event->acceptProposedAction();
    showPanel();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        hidePanel();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        const QModelIndex idx = m_view->currentIndex();
        if (idx.isValid())
            onItemActivated(idx);
        return;
    }
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hidePanel();
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate && m_allowAutoHide && isVisible()) {
        const bool menuOpen = (m_trayMenu && m_trayMenu->isVisible());
        if (!menuOpen)
            hidePanel();
    }
    return QWidget::event(event);
}

QIcon MainWindow::makeTrayIcon() const
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::instance().color("accent"));
    p.drawRoundedRect(QRectF(5, 5, 54, 54), 15, 15);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(34);
    f.setBold(true);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, "L");
    p.end();

    return QIcon(pm);
}

void MainWindow::rebuildTrayMenu()
{
    if (!m_trayMenu)
        return;

    m_trayMenu->clear();
    m_trayMenu->addAction(I18n::instance().t("tray.show"), this, &MainWindow::toggleVisible);
    m_trayMenu->addAction(I18n::instance().t("tray.reload"), this, &MainWindow::reload);
    m_trayMenu->addAction(I18n::instance().t("tray.config_dir"), this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppConfig::instance().dataDir()));
    });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(I18n::instance().t("tray.quit"), qApp, &QApplication::quit);
}

void MainWindow::installTray()
{
    if (m_tray)
        return;

    m_tray = new QSystemTrayIcon(makeTrayIcon(), this);
    m_tray->setToolTip("LuoDeck");

    m_trayMenu = new QMenu(this);
    rebuildTrayMenu();
    m_tray->setContextMenu(m_trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                    toggleVisible();
            });

    m_tray->show();
}

void MainWindow::notify(const QString &title, const QString &body)
{
    if (m_tray)
        m_tray->showMessage(title, body, QSystemTrayIcon::Information, 6000);
}
