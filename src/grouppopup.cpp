#include "grouppopup.h"

#include "iconprovider.h"
#include "icontile.h"
#include "i18n.h"
#include "metrics.h"
#include "theme.h"

#include <QEvent>
#include <QCursor>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kMaxVisibleRows = 8; // 超过这么多行就出滚动条

QString popupTitle(const LayoutItem &group)
{
    if (group.isDesktopFrame())
        return I18n::instance().t("group.desktop");
    if (group.isStartMenuFrame())
        return I18n::instance().t("group.startmenu");
    return group.name;
}

} // namespace

GroupPopup::GroupPopup(const LayoutItem &group, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                         | Qt::NoDropShadowWindowHint)
    , m_group(group)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, true);

    m_allowAutoClose = false;
    QTimer::singleShot(400, this, [this] { m_allowAutoClose = true; });
}

void GroupPopup::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Metrics::kGroupPad + 3, Metrics::kGroupPad + 2,
                              Metrics::kGroupPad + 3, Metrics::kGroupPad + 4);
    outer->setSpacing(4);

    auto *title = new QLabel(popupTitle(m_group), this);
    QFont tf = title->font();
    tf.setPixelSize(12);
    tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet(QString("color: %1; padding-left: 2px;")
                             .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    outer->addWidget(title);

    // 项多了给个搜索框：开始菜单一进来就是上百项，没搜索基本没法用
    if (m_group.children.size() > 8) {
        m_search = new QLineEdit(this);
        m_search->setPlaceholderText(I18n::instance().t("popup.search"));
        m_search->setClearButtonEnabled(true);
        connect(m_search, &QLineEdit::textChanged, this,
                [this](const QString &text) { populate(text); });
        outer->addWidget(m_search);
    }

    m_gridHost = new QWidget(this);
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(0, 2, 0, 0);
    m_grid->setHorizontalSpacing(Metrics::kGroupGap);
    m_grid->setVerticalSpacing(Metrics::kGroupGap);

    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setWidget(m_gridHost);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget"
                            " { background: transparent; }");
    outer->addWidget(m_scroll);
}

void GroupPopup::populate(const QString &filter)
{
    // 清掉旧图标
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_tiles.clear();
    m_tileChildIds.clear();
    m_dragTileIndex = -1;
    m_dropTileIndex = -1;

    const QString needle = filter.trimmed();
    const int cols = qBound(1, m_group.cols, 6);

    int shown = 0;
    for (const LayoutItem &child : m_group.children) {
        if (!needle.isEmpty() && !child.name.contains(needle, Qt::CaseInsensitive)
            && !child.path.contains(needle, Qt::CaseInsensitive))
            continue;

        auto *tile = new IconTile(m_gridHost);
        tile->setLabel(child.name);
        tile->setPath(child.path);
        loadIconAsync(tile, child.path, shown * 6);

        const QString path = child.path;
        const QString childArgs = child.args;
        const QString childId = child.id;
        connect(tile, &IconTile::clicked, this, [this, path, childArgs] {
            emit childLaunchRequested(path, childArgs);
            close();
        });

        // 弹层里也能整理顺序；只改显示顺序，不动磁盘上的文件
        {
            const int tileIndex = shown;
            connect(tile, &IconTile::dragMoved, this,
                    [this, tileIndex](const QPoint &, const QPoint &currentGlobal) {
                        m_dragTileIndex = tileIndex;
                        setDropIndex(tileIndexAt(mapFromGlobal(currentGlobal)));
                    });
            connect(tile, &IconTile::dragEnded, this, [this] { finishTileDrag(); });
        }

        m_grid->addWidget(tile, shown / cols, shown % cols);
        m_tiles.append(tile);
        m_tileChildIds.append(childId);
        ++shown;
    }

    if (shown == 0) {
        auto *empty = new QLabel(needle.isEmpty() ? I18n::instance().t("popup.empty")
                                                  : I18n::instance().t("popup.no_match"),
                                 m_gridHost);
        empty->setStyleSheet(QString("color: %1; padding: 10px;")
                                 .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
        m_grid->addWidget(empty, 0, 0);
        ++shown;
    }

    // 高度：内容少就贴着内容，多了固定高度出滚动条
    const int rows = (shown + cols - 1) / cols;
    const int rowPx = Metrics::kGroupCellH + Metrics::kGroupGap;
    const int contentH = rows * rowPx + 6;

    QScreen *screen = QGuiApplication::screenAt(m_anchor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    const int maxH = qMax(200, qMin(avail.height() - 180, kMaxVisibleRows * rowPx + 6));

    m_scroll->setFixedHeight(qBound(rowPx, contentH, maxH));
    m_scroll->setFixedWidth(cols * (Metrics::kGroupCellW + Metrics::kGroupGap) + 2);

    relayoutToContent(m_anchor);
}

int GroupPopup::tileIndexAt(const QPoint &localPos) const
{
    for (int i = 0; i < m_tiles.size(); ++i) {
        if (m_tiles.at(i)->geometry().contains(localPos))
            return i;
    }
    return -1;
}

void GroupPopup::setDropIndex(int index)
{
    if (index == m_dragTileIndex)
        index = -1;
    if (index == m_dropTileIndex)
        return;

    if (m_dropTileIndex >= 0 && m_dropTileIndex < m_tiles.size())
        m_tiles.at(m_dropTileIndex)->setDropTarget(false);
    m_dropTileIndex = index;
    if (m_dropTileIndex >= 0 && m_dropTileIndex < m_tiles.size())
        m_tiles.at(m_dropTileIndex)->setDropTarget(true);
}

void GroupPopup::finishTileDrag()
{
    const int from = m_dragTileIndex;
    m_dragTileIndex = -1;
    setDropIndex(-1);
    if (from < 0 || from >= m_tileChildIds.size())
        return;

    const int to = tileIndexAt(mapFromGlobal(QCursor::pos()));
    if (to < 0 || to == from || to >= m_tileChildIds.size())
        return;

    const QString fromId = m_tileChildIds.at(from);
    const QString toId = m_tileChildIds.at(to);

    const LayoutItem *group = AppConfig::instance().findById(m_group.id);
    if (!group)
        return;

    int fromChild = -1;
    int toChild = -1;
    for (int i = 0; i < group->children.size(); ++i) {
        if (group->children.at(i).id == fromId)
            fromChild = i;
        if (group->children.at(i).id == toId)
            toChild = i;
    }
    if (fromChild < 0 || toChild < 0 || fromChild == toChild)
        return;

    if (!AppConfig::instance().moveChildToIndex(m_group.id, fromChild, toChild))
        return;

    if (const LayoutItem *updated = AppConfig::instance().findById(m_group.id))
        m_group = *updated;

    // 顺序变了就地重排弹层（保持打开），不用关掉重开
    populate(m_search ? m_search->text() : QString());
}

void GroupPopup::relayoutToContent(const QPoint &globalPos)
{
    adjustSize();

    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int x = globalPos.x() - width() / 2;
    int y = globalPos.y() - height() / 2;
    x = qBound(avail.left() + 8, x, qMax(avail.left() + 8, avail.right() - width() - 8));
    y = qBound(avail.top() + 8, y, qMax(avail.top() + 8, avail.bottom() - height() - 8));

    move(x, y);
}

void GroupPopup::popupAt(const QPoint &globalPos)
{
    m_anchor = globalPos;
    buildUi();
    populate(QString());

    show();
    raise();
    activateWindow();
    if (m_search)
        m_search->setFocus();
    else
        setFocus();
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
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);
}

void GroupPopup::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    if (event->key() == Qt::Key_F5) {
        emit arrangeRequested();
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
