#include "mainwindow.h"

#include "appconfig.h"
#include "canvaswidget.h"
#include "hotkey.h"
#include "i18n.h"
#include "iconprovider.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
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
    setWindowTitle("LuoDeck");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);

    m_canvas = new CanvasWidget(this);
    layout->addWidget(m_canvas);

    connect(m_canvas, &CanvasWidget::requestHide, this, &MainWindow::hidePanel);
    connect(&Theme::instance(), &Theme::themeChanged, this, &MainWindow::applyTheme);
    connect(&I18n::instance(), &I18n::languageChanged, this, &MainWindow::retranslateUi);

    applyTheme();
    m_canvas->rebuildLayout();
}

MainWindow::~MainWindow() = default;

void MainWindow::updateWindowSize()
{
    const QSize canvas = AppConfig::instance().canvasSize();
    m_canvas->setFixedSize(canvas);
    resize(canvas.width() + 28, canvas.height() + 28);
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(Theme::instance().styleSheet());
    updateWindowSize();
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

void MainWindow::showPanel()
{
    m_allowAutoHide = false;
    updateWindowSize();
    m_canvas->rebuildLayout();

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

    show();
    raise();
    activateWindow();
    setFocus();
    QTimer::singleShot(220, this, [this] { m_allowAutoHide = true; });
}

void MainWindow::hidePanel()
{
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
    m_canvas->rebuildLayout();
    update();
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
        if (!menuOpen && !m_canvas->isPopupOpen())
            hidePanel();
    }
    return QWidget::event(event);
}

void MainWindow::openSettings()
{
    m_allowAutoHide = false;

    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::hotkeyChanged, this, &MainWindow::reregisterHotkey);
    dlg.exec();

    m_allowAutoHide = true;
}

void MainWindow::setupHotkey()
{
    if (!m_hotkey) {
        m_hotkey = new Hotkey(this);
        connect(m_hotkey, &Hotkey::activated, this, &MainWindow::toggleVisible);
    }
    reregisterHotkey();
}

void MainWindow::reregisterHotkey()
{
    if (!m_hotkey)
        return;

    AppConfig &cfg = AppConfig::instance();
    const QString wanted = cfg.hotkey();

    if (m_hotkey->registerFromString(wanted))
        return;

    if (wanted.compare(QStringLiteral("ctrl+space"), Qt::CaseInsensitive) == 0
        && m_hotkey->registerFromString(QStringLiteral("ctrl+alt+space"))) {
        notify(QStringLiteral("LuoDeck"),
               QStringLiteral("Ctrl+Space 已被其他程序（通常是输入法）占用，临时改用 Ctrl+Alt+Space。\n"
                              "可在托盘 → 设置 中修改热键。"));
        return;
    }

    notify(QStringLiteral("LuoDeck"),
           I18n::instance().t(QStringLiteral("tray.hotkey_failed"), wanted));
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
    m_trayMenu->addAction(I18n::instance().t("tray.settings"), this, &MainWindow::openSettings);
    m_trayMenu->addSeparator();
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
        m_tray->showMessage(title, body, QSystemTrayIcon::Information, 7000);
}
