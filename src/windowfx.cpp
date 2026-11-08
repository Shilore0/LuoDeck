#include "windowfx.h"

#include "theme.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWidget>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <dwmapi.h>
#  include <windows.h>
#endif

QIcon luoDeckIcon()
{
    QIcon icon;
    const QColor accent = Theme::instance().color("accent");

    const QList<int> sizes = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    for (int size : sizes) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);

        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal radius = size * 0.22;
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(0.5, 0.5, size - 1.0, size - 1.0), radius, radius);

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPixelSize(qMax(8, int(size * 0.6)));
        f.setBold(true);
        p.setFont(f);
        p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("L"));
        p.end();

        icon.addPixmap(pm);
    }
    return icon;
}

void applyFrameTheme(QWidget *window)
{
    if (!window)
        return;

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = Theme::instance().color("surface").lightness() < 128 ? TRUE : FALSE;

    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE（Win10 20H1+），19 为早期版本号
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
#else
    Q_UNUSED(window)
#endif
}

void placeBeside(QWidget *dialog, const QRect &panelGlobalRect)
{
    if (!dialog)
        return;

    dialog->adjustSize();

    QScreen *screen = QGuiApplication::screenAt(panelGlobalRect.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int x = panelGlobalRect.right() + 12;
    int y = panelGlobalRect.top();

    // 右侧放不下就放左侧
    if (x + dialog->width() > avail.right() - 8)
        x = panelGlobalRect.left() - dialog->width() - 12;

    // 左右都放不下就放下面
    if (x < avail.left() + 8) {
        x = panelGlobalRect.center().x() - dialog->width() / 2;
        y = panelGlobalRect.bottom() + 12;
    }

    x = qBound(avail.left() + 8, x, qMax(avail.left() + 8, avail.right() - dialog->width() - 8));
    y = qBound(avail.top() + 8, y, qMax(avail.top() + 8, avail.bottom() - dialog->height() - 8));

    dialog->move(x, y);
}
