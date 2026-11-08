#include "appitemwidget.h"

#include "iconprovider.h"
#include "metrics.h"
#include "theme.h"

#include <QPainter>

AppItemWidget::AppItemWidget(const QString &id, const QString &name, const QString &path,
                             QWidget *parent)
    : ItemWidget(id, parent)
    , m_name(name)
    , m_path(path)
    , m_icon(IconProvider::instance().icon(path))
{
    setFixedSize(Metrics::kCellW, Metrics::kCellH);
    setToolTip(path);
}

void AppItemWidget::setDisplayName(const QString &name)
{
    m_name = name;
    update();
}

void AppItemWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const Theme &th = Theme::instance();
    const QRect cell = rect().adjusted(1, 1, -1, -1);

    if (underMouse()) {
        QColor bg = th.color("surfaceHover");
        bg.setAlpha(190);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(cell, 8, 8);
    }

    if (!m_icon.isNull()) {
        const int side = Metrics::kIconSize;
        const QRect iconRect(cell.center().x() - side / 2, cell.top() + Metrics::kIconTop,
                             side, side);
        const qreal dpr = devicePixelRatioF();
        QPixmap pm = m_icon.pixmap(QSize(side, side) * dpr);
        if (!pm.isNull()) {
            pm.setDevicePixelRatio(dpr);
            p.drawPixmap(iconRect, pm);
        }
    }

    QFont f = font();
    f.setPixelSize(11);
    p.setFont(f);
    p.setPen(th.color("text"));
    const QRect textRect(cell.left() + 2, cell.bottom() - 20, cell.width() - 4, 18);
    const QFontMetrics fm(f);
    p.drawText(textRect, Qt::AlignCenter,
               fm.elidedText(m_name, Qt::ElideRight, textRect.width()));
}
