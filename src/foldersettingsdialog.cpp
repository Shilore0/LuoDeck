#include "foldersettingsdialog.h"

#include "i18n.h"
#include "theme.h"
#include "windowfx.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

FolderSettingsDialog::FolderSettingsDialog(const LayoutItem &item, QWidget *parent)
    : QDialog(parent)
    , m_item(item)
    , m_color(item.color.isEmpty() ? AppConfig::defaultDesktopColor() : item.color)
    , m_originalColor(m_color)
{
    setWindowTitle(I18n::instance().t("folder.settings"));
    setWindowIcon(luoDeckIcon());
    setMinimumWidth(360);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(12);

    auto *form = new QFormLayout;
    form->setSpacing(9);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_name = new QLineEdit(m_item.name, this);
    m_name->setEnabled(!m_item.system);
    form->addRow(I18n::instance().t("folder.name"), m_name);

    auto *colorsHost = new QWidget(this);
    auto *colorsRow = new QHBoxLayout(colorsHost);
    colorsRow->setContentsMargins(0, 0, 0, 0);
    colorsRow->setSpacing(8);

    const QStringList colors = AppConfig::palette();
    const QString textColor = Theme::instance().color("text").name(QColor::HexRgb);

    for (const QString &c : colors) {
        auto *btn = new QPushButton(colorsHost);
        btn->setFixedSize(26, 26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);
        btn->setChecked(c.compare(m_color, Qt::CaseInsensitive) == 0);
        btn->setEnabled(!m_item.system);
        btn->setStyleSheet(
            QString("QPushButton { background: %1; border: 2px solid transparent;"
                    " border-radius: 13px; padding: 0px; }"
                    "QPushButton:checked { border: 2px solid %2; }"
                    "QPushButton:disabled { border: 2px solid transparent; }")
                .arg(c, textColor));
        btn->setProperty("colorValue", c);
        connect(btn, &QPushButton::clicked, this, [this, c] { selectColor(c); });
        m_colorButtons.append(btn);
        colorsRow->addWidget(btn);
    }
    colorsRow->addStretch(1);
    form->addRow(I18n::instance().t("folder.color"), colorsHost);

    root->addLayout(form);

    if (m_item.system) {
        auto *note = new QLabel(I18n::instance().t("folder.system_note"), this);
        note->setWordWrap(true);
        note->setStyleSheet(QString("color: %1; font-size: 12px;")
                                .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
        root->addWidget(note);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(I18n::instance().t("settings.ok"));
    buttons->button(QDialogButtonBox::Cancel)->setText(I18n::instance().t("settings.cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void FolderSettingsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    applyFrameTheme(this);
}

void FolderSettingsDialog::selectColor(const QString &color)
{
    if (m_item.system)
        return;

    m_color = color;
    for (QPushButton *btn : m_colorButtons) {
        btn->setChecked(btn->property("colorValue").toString().compare(color, Qt::CaseInsensitive)
                        == 0);
    }

    // 实时预览：主面板立刻换色，不用点确定
    emit colorPreviewed(color);
}

void FolderSettingsDialog::reject()
{
    // 取消时把预览过的颜色还原
    if (!m_item.system && m_color.compare(m_originalColor, Qt::CaseInsensitive) != 0) {
        m_color = m_originalColor;
        emit colorPreviewed(m_originalColor);
    }
    QDialog::reject();
}

QString FolderSettingsDialog::folderName() const
{
    return m_name ? m_name->text().trimmed() : m_item.name;
}
