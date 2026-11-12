#include "winappdialog.h"

#include "i18n.h"
#include "windowfx.h"

#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

WinAppDialog::WinAppDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::instance().t("winapp.title"));
    setMinimumSize(460, 460);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 14, 16, 12);
    outer->setSpacing(8);

    auto *hint = new QLabel(I18n::instance().t("winapp.hint"), this);
    hint->setWordWrap(true);
    QFont hf = hint->font();
    hf.setPixelSize(11);
    hint->setFont(hf);
    outer->addWidget(hint);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(I18n::instance().t("winapp.search"));
    m_search->setClearButtonEnabled(true);
    outer->addWidget(m_search);

    // 用列表模式（图标在左、名字在右）：商店应用名字普遍很长，
    // 网格模式必然被截断成"…"，列表里能完整显示
    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::ListMode);
    m_list->setIconSize(QSize(32, 32));
    m_list->setSpacing(1);
    m_list->setMovement(QListView::Static);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setWordWrap(false);
    m_list->setTextElideMode(Qt::ElideRight);
    m_list->setUniformItemSizes(true);
    QFont listFont = m_list->font();
    listFont.setPixelSize(12);
    m_list->setFont(listFont);
    outer->addWidget(m_list, 1);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *addBtn =
        buttons->addButton(I18n::instance().t("winapp.add"), QDialogButtonBox::AcceptRole);
    buttons->addButton(I18n::instance().t("settings.cancel"), QDialogButtonBox::RejectRole);
    addBtn->setDefault(true);
    outer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_search, &QLineEdit::textChanged, this, &WinAppDialog::filter);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { accept(); });

    // 取应用列表 + 图标要一会儿（每个图标都要问一次 shell），先把窗口亮出来
    QTimer::singleShot(0, this, [this] { fill(); });
}

void WinAppDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // 标题栏跟着主题色，别用系统给的黑/白
    applyFrameTheme(this);
}

void WinAppDialog::fill()
{
    if (m_filled)
        return;
    m_filled = true;

    m_entries = WinApps::list();

    for (const WinApps::Entry &e : m_entries) {
        auto *item = new QListWidgetItem(QIcon(WinApps::image(e.aumid, 32)), e.name, m_list);
        item->setData(Qt::UserRole, e.aumid);
        item->setToolTip(QStringLiteral("%1\n%2").arg(e.name, e.aumid));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setSizeHint(QSize(0, 40));
    }
}

void WinAppDialog::filter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *it = m_list->item(i);
        const bool hit = needle.isEmpty()
            || it->text().contains(needle, Qt::CaseInsensitive)
            || it->data(Qt::UserRole).toString().contains(needle, Qt::CaseInsensitive);
        it->setHidden(!hit);
    }
}

QStringList WinAppDialog::selectedAumids() const
{
    QStringList out;
    const auto items = m_list->selectedItems();
    for (const QListWidgetItem *it : items)
        out << it->data(Qt::UserRole).toString();
    return out;
}

QString WinAppDialog::nameFor(const QString &aumid) const
{
    for (const WinApps::Entry &e : m_entries) {
        if (e.aumid == aumid)
            return e.name;
    }
    return QString();
}
