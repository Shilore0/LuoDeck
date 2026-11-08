#include "grouppopup.h"

#include "iconprovider.h"
#include "icontile.h"
#include "i18n.h"
#include "metrics.h"
#include "theme.h"

#include <QEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

GroupPopup::GroupPopup(const LayoutItem &group, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                         | Qt::NoDropShadowWindowHint)
    , m_group(group)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, true);

    buildTiles();

    m_allowAutoClose = false;
    QTimer::singleShot(220, this, [this] { m_allowAutoClose = true; });
}

void GroupPopup::buildTiles()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Metrics::kGroupPad + 3, Metrics::kGroupPad + 2,
                              Metrics::kGroupPad + 3, Metrics::kGroupPad + 4);
    outer->setSpacing(2);

    const QString titleText = m_group.system ? I18n::instance().t("group.desktop")
                                            : m_group.name;
    auto *title = new QLabel(titleText, this);
    QFont tf = title->font();
    tf.setPixelSize(12);
    tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet(QString("color: %1; padding-left: 2px;")
                             .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    outer->addWidget(title);

    auto *gridHost = new QWidget(this);
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 2, 0, 0);
    grid->setHorizontalSpacing(2);
    grid->setVerticalSpacing(2);

    const int cols = qBound(1, m_group.cols, 5);
    for (int i = 0; i < m_group.children.size(); ++i) {
        const LayoutItem &child = m_group.children.at(i);
        auto *tile = new IconTile(gridHost);
        tile->setIcon(IconProvider::instance().icon(child.path));
        tile->setLabel(child.name);
        tile->setPath(child.path);

        const QString path = child.path;
        connect(tile, &IconTile::clicked, this, [this, path] {
            emit childLaunchRequested(path);
            close();
        });

        grid->addWidget(tile, i / cols, i % cols);
    }

    if (m_group.children.isEmpty()) {
        auto *empty = new QLabel(tr("空"), gridHost);
        empty->setStyleSheet(QString("color: %1;")
                                 .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
        grid->addWidget(empty, 0, 0);
    }

    outer->addWidget(gridHost);
}

void GroupPopup::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const Theme &th = Theme::instance();
    QColor bg = th.color("surface");
    bg.setAlpha(248);
    QColor border = th.color("border");
    border.setAlpha(170);

    p.setPen(QPen(border, 1));
    p.setBrush(bg);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 14, 14);
}

void GroupPopup::popupAt(const QPoint &globalPos)
{
    adjustSize();

    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int x = globalPos.x() - width() / 2;
    int y = globalPos.y() - height() / 2;
    x = qBound(avail.left() + 8, x, avail.right() - width() - 8);
    y = qBound(avail.top() + 8, y, avail.bottom() - height() - 8);

    move(x, y);
    show();
    raise();
    activateWindow();
    setFocus();
}

void GroupPopup::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool GroupPopup::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate && m_allowAutoClose) {
        close();
        return true;
    }
    return QWidget::event(event);
}
