#include "mainwindow.h"

#include "appconfig.h"
#include "canvaswidget.h"
#include "hotkey.h"
#include "i18n.h"
#include "iconprovider.h"
#include "settingsdialog.h"
#include "shellselection.h"
#include "theme.h"
#include "version.h"
#include "windowfx.h"

#include <QApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowTitle("LuoDeck");

    // 画布直接作为子控件铺在面板里：内容比窗口高时靠自己滚动，
    // 用 QScrollArea 会在这个"置顶 + 透明"的窗口上布局错位。
    m_canvas = new CanvasWidget(this);
    m_canvas->move(14, 14);

    connect(m_canvas, &CanvasWidget::requestHide, this, &MainWindow::hidePanel);
    connect(m_canvas, &CanvasWidget::launchFailed, this, [this](const QString &path) {
        notify(QStringLiteral("LuoDeck"),
               I18n::instance().t(QStringLiteral("launch.failed"),
                                  QFileInfo(path).fileName()));
    });
    connect(m_canvas, &CanvasWidget::scrollRequested, this, &MainWindow::scrollBy);
    connect(m_canvas, &CanvasWidget::hScrollRequested, this, &MainWindow::scrollXBy);
    connect(m_canvas, &CanvasWidget::requestReveal, this, &MainWindow::revealCanvasRect);
    connect(m_canvas, &CanvasWidget::requestResize, this, [this] { updateWindowSize(); });
    connect(&Theme::instance(), &Theme::themeChanged, this, &MainWindow::applyTheme);
    connect(&I18n::instance(), &I18n::languageChanged, this, &MainWindow::retranslateUi);
    // 结构变了（增删/换框）之后画布尺寸可能要跟着变
    connect(&AppConfig::instance(), &AppConfig::layoutChanged, this,
            [this] { QTimer::singleShot(0, this, &MainWindow::updateWindowSize); });

    // F5 = 整理布局。挂在窗口级快捷键上，焦点在画布/图标上时也能收到
    auto *arrangeShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    arrangeShortcut->setContext(Qt::WindowShortcut);
    connect(arrangeShortcut, &QShortcut::activated, this, &MainWindow::arrangeLayout);

    applyTheme();
}

MainWindow::~MainWindow() = default;

void MainWindow::updateWindowSize()
{
    const QSize canvasSize = AppConfig::instance().requiredCanvasSize();

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    const int maxW = qMax(360, avail.width() - 80);
    const int maxH = qMax(260, avail.height() - 80);

    const int w = qMin(canvasSize.width() + 28, maxW);
    const int h = qMin(canvasSize.height() + 28, maxH);

    if (size() != QSize(w, h))
        resize(w, h);

    // 画布铺满可视宽度；高度按内容给，超出窗口的部分靠滚动看
    const int viewW = qMax(200, w - 28);
    const int viewH = qMax(120, h - 28);
    m_canvas->setFixedSize(qMax(canvasSize.width(), viewW), canvasSize.height());

    // 屏幕放不下画布宽度时，横向也能滚（Shift+滚轮），否则右侧内容永久够不着
    m_maxScroll = qMax(0, m_canvas->height() - viewH);
    m_maxScrollX = qMax(0, m_canvas->width() - viewW);
    if (m_scrollY > m_maxScroll)
        m_scrollY = m_maxScroll;
    if (m_scrollX > m_maxScrollX)
        m_scrollX = m_maxScrollX;
    applyScroll();
}

void MainWindow::applyScroll()
{
    m_canvas->move(14 - m_scrollX, 14 - m_scrollY);
    update();
}

void MainWindow::scrollTo(int y)
{
    const int next = qBound(0, y, m_maxScroll);
    if (next == m_scrollY)
        return;
    m_scrollY = next;
    applyScroll();
}

void MainWindow::scrollBy(int delta)
{
    scrollTo(m_scrollY - delta);
}

void MainWindow::scrollXTo(int x)
{
    const int next = qBound(0, x, m_maxScrollX);
    if (next == m_scrollX)
        return;
    m_scrollX = next;
    applyScroll();
}

void MainWindow::scrollXBy(int delta)
{
    scrollXTo(m_scrollX - delta);
}

void MainWindow::revealCanvasRect(const QRect &rect)
{
    const int viewH = qMax(1, height() - 28);
    if (rect.top() < m_scrollY)
        scrollTo(rect.top() - 8);
    else if (rect.bottom() > m_scrollY + viewH)
        scrollTo(rect.bottom() - viewH + 8);

    // 横向同样要保证目标可见
    const int viewW = qMax(1, width() - 28);
    if (rect.left() < m_scrollX)
        scrollXTo(rect.left() - 8);
    else if (rect.right() > m_scrollX + viewW)
        scrollXTo(rect.right() - viewW + 8);
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(Theme::instance().styleSheet());
    updateWindowSize();
    if (m_tray)
        m_tray->setIcon(luoDeckIcon());
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

    // 内容超出时右侧给一条细滚动指示
    if (m_maxScroll > 0) {
        const int trackTop = 20;
        const int trackH = height() - 40;
        const int viewH = qMax(1, height() - 28);
        const int barH = qMax(28, trackH * viewH / qMax(1, m_canvas->height()));
        const int barTop = trackTop + (trackH - barH) * m_scrollY / qMax(1, m_maxScroll);

        QColor bar = th.color("textMuted");
        bar.setAlpha(90);
        p.setPen(Qt::NoPen);
        p.setBrush(bar);
        p.drawRoundedRect(QRectF(width() - 8, barTop, 3, barH), 1.5, 1.5);
    }
}

void MainWindow::showPanel()
{
    m_allowAutoHide = false;

    // 桌面 / 开始菜单目录可能在程序运行期间变过，每次弹出时对一次账
    AppConfig::instance().refreshDesktop();
    AppConfig::instance().refreshPinned();
    updateWindowSize();

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect avail = screen->availableGeometry();
        const QPoint cursor = QCursor::pos();
        int x = qBound(avail.left() + 8, cursor.x() - width() / 2, avail.right() - width() - 8);
        int y = qBound(avail.top() + 8, cursor.y() - height() / 2, avail.bottom() - height() - 8);
        move(x, y);
    }

    // 先把窗口亮出来再填图标：图标提取要时间，先出个空面板比卡住几百毫秒舒服
    show();
    raise();
    activateWindow();
    setFocus();
    m_canvas->rebuildLayout();

    QTimer::singleShot(220, this, [this] { m_allowAutoHide = true; });
}

void MainWindow::hidePanel()
{
    if (m_canvas)
        m_canvas->closePopup();
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
    AppConfig::instance().refreshDesktop();
    AppConfig::instance().refreshPinned();
    updateWindowSize();
    m_canvas->rebuildLayout();
    update();
}

void MainWindow::arrangeLayout()
{
    if (!isVisible())
        return;

    // 展开的框是独立窗口，整理时先收起来，免得跟着旧坐标晃
    if (m_canvas)
        m_canvas->closePopup();

    scrollTo(0);
    scrollXTo(0);
    AppConfig::instance().autoArrange();
    updateWindowSize();
}

void MainWindow::retranslateUi()
{
    rebuildTrayMenu();
    if (m_canvas)
        m_canvas->rebuildLayout();
    update();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hidePanel();
        return;
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
        const bool menuOpen = m_trayMenu && m_trayMenu->isVisible();

        // 自己弹出的模态对话框（设置、选文件）不算失去焦点，否则一开框面板就没了
        QWidget *modal = QApplication::activeModalWidget();
        const bool modalOpen = modal && (modal == this || isAncestorOf(modal));

        // 右键菜单 / 展开的框 也是独立弹窗，同样不能因此收起面板
        const bool popupOpen = QApplication::activePopupWidget() != nullptr;

        if (!menuOpen && !modalOpen && !popupOpen && !m_canvas->isPopupOpen())
            hidePanel();
    }
    return QWidget::event(event);
}

void MainWindow::openSettings()
{
    if (!isVisible())
        showPanel();

    m_allowAutoHide = false;

    SettingsDialog dlg(this);
    placeBeside(&dlg, geometry());
    connect(&dlg, &SettingsDialog::hotkeyChanged, this, &MainWindow::reregisterHotkey);
    dlg.exec();

    m_allowAutoHide = true;
}

void MainWindow::setupHotkey()
{
    if (!m_hotkey) {
        m_hotkey = new Hotkey(0xB00D, this);
        connect(m_hotkey, &Hotkey::activated, this, &MainWindow::toggleVisible);
    }
    if (!m_hotkeyAdd) {
        m_hotkeyAdd = new Hotkey(0xB00E, this);
        connect(m_hotkeyAdd, &Hotkey::activated, this, &MainWindow::addCurrentSelection);
    }
    reregisterHotkey();
}

void MainWindow::reregisterHotkey()
{
    if (!m_hotkey)
        return;

    AppConfig &cfg = AppConfig::instance();

    // 面板热键
    const QString wanted = cfg.hotkey();
    const QString previous = m_hotkey->current(); // registerFromString 会先注销旧的，先记下来
    if (!m_hotkey->registerFromString(wanted)) {
        if (wanted.compare(QStringLiteral("ctrl+space"), Qt::CaseInsensitive) == 0
            && m_hotkey->registerFromString(QStringLiteral("ctrl+alt+space"))) {
            notify(QStringLiteral("LuoDeck"),
                   QStringLiteral("Ctrl+Space 已被其他程序（通常是输入法）占用，"
                                  "临时改用 Ctrl+Alt+Space。可在托盘 → 设置 中修改。"));
        } else {
            // 注册失败不能让坏键位留在配置里：能滚回上一个键位就滚回去
            if (!previous.isEmpty() && m_hotkey->registerFromString(previous)) {
                cfg.setHotkey(previous);
                notify(QStringLiteral("LuoDeck"),
                       I18n::instance().t(QStringLiteral("tray.hotkey_failed"), wanted)
                           + QStringLiteral(" ")
                           + I18n::instance().t(QStringLiteral("hotkey.rolled_back"),
                                                previous));
            } else {
                notify(QStringLiteral("LuoDeck"),
                       I18n::instance().t(QStringLiteral("tray.hotkey_failed"), wanted));
            }
        }
    }

    // 「把选中项加入面板」热键
    if (m_hotkeyAdd) {
        const QString addKey = cfg.hotkeyAdd();
        if (!addKey.isEmpty()) {
            const QString previousAdd = m_hotkeyAdd->current();
            if (!m_hotkeyAdd->registerFromString(addKey)) {
                if (!previousAdd.isEmpty() && m_hotkeyAdd->registerFromString(previousAdd))
                    cfg.setHotkeyAdd(previousAdd);
                notify(QStringLiteral("LuoDeck"),
                       I18n::instance().t(QStringLiteral("hotkey.add_failed"), addKey));
            }
        }
    }
}

void MainWindow::addCurrentSelection()
{
    const QStringList paths = ShellSelection::currentSelection();
    if (paths.isEmpty()) {
        notify(QStringLiteral("LuoDeck"), I18n::instance().t(QStringLiteral("hotkey.no_selection")));
        return;
    }

    QString last;
    for (const QString &path : paths) {
        const QString id = AppConfig::instance().addApp(path);
        if (!id.isEmpty())
            last = id;
    }

    const int added = paths.size();
    notify(QStringLiteral("LuoDeck"), I18n::instance().t(QStringLiteral("hotkey.added")).arg(added));
    if (!isVisible())
        showPanel();
    if (!last.isEmpty())
        m_canvas->flashItem(last);
}

void MainWindow::rebuildTrayMenu()
{
    if (!m_trayMenu)
        return;

    m_trayMenu->clear();
    m_trayMenu->addAction(I18n::instance().t("tray.show"), this, &MainWindow::toggleVisible);
    m_trayMenu->addAction(I18n::instance().t("tray.settings"), this, &MainWindow::openSettings);
    m_trayMenu->addSeparator();
    auto *menuImport = new QAction(I18n::instance().t("tray.startmenu"), m_trayMenu);
    menuImport->setCheckable(true);
    menuImport->setChecked(AppConfig::instance().showStartMenu());
    connect(menuImport, &QAction::toggled, this, [](bool on) {
        AppConfig::instance().setShowStartMenu(on);
    });
    m_trayMenu->addAction(menuImport);

    m_trayMenu->addAction(I18n::instance().t("tray.reload"), this, &MainWindow::reload);
    m_trayMenu->addAction(I18n::instance().t("tray.config_dir"), this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppConfig::instance().dataDir()));
    });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(I18n::instance().t("tray.quit"), qApp, &QApplication::quit);
}

void MainWindow::askFirstRunImport()
{
    AppConfig &cfg = AppConfig::instance();
    if (!cfg.needsStartMenuPrompt())
        return;

    QMessageBox box(QMessageBox::Question, QStringLiteral("LuoDeck"),
                    I18n::instance().t("firstrun.startmenu"), QMessageBox::NoButton, this);
    box.setInformativeText(I18n::instance().t("firstrun.startmenu_note"));
    QPushButton *yes =
        box.addButton(I18n::instance().t("firstrun.yes"), QMessageBox::AcceptRole);
    box.addButton(I18n::instance().t("firstrun.no"), QMessageBox::RejectRole);
    box.setDefaultButton(yes);
    box.winId();
    applyFrameTheme(&box);
    box.exec();

    cfg.setShowStartMenu(box.clickedButton() == yes);
    cfg.markStartMenuAsked();
}

void MainWindow::installTray()
{
    if (m_tray)
        return;

    m_tray = new QSystemTrayIcon(luoDeckIcon(), this);
    m_tray->setToolTip(QStringLiteral("LuoDeck v" LUODECK_VERSION));

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
        m_tray->showMessage(title, body, QSystemTrayIcon::Information, 7000);
}
