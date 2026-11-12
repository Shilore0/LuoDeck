#include "settingsdialog.h"

#include "appconfig.h"
#include "i18n.h"
#include "shellintegration.h"
#include "theme.h"
#include "version.h"
#include "windowfx.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>

void SettingsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    applyFrameTheme(this);
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::instance().t("settings.title"));
    setWindowIcon(luoDeckIcon());
    setMinimumWidth(380);
    setModal(true);

    AppConfig &cfg = AppConfig::instance();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *form = new QFormLayout;
    form->setSpacing(9);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_theme = new QComboBox(this);
    QStringList themes = Theme::instance().available();
    if (themes.isEmpty())
        themes << cfg.theme();
    m_theme->addItems(themes);
    m_theme->setCurrentText(cfg.theme());
    form->addRow(I18n::instance().t("settings.theme"), m_theme);

    m_language = new QComboBox(this);
    m_language->addItems(I18n::instance().available());
    m_language->setCurrentText(I18n::instance().current());
    form->addRow(I18n::instance().t("settings.language"), m_language);

    m_hotkey = new QLineEdit(cfg.hotkey(), this);
    m_hotkey->setPlaceholderText(QStringLiteral("ctrl+space"));
    form->addRow(I18n::instance().t("settings.hotkey"), m_hotkey);

    m_hotkeyAdd = new QLineEdit(cfg.hotkeyAdd(), this);
    m_hotkeyAdd->setPlaceholderText(QStringLiteral("ctrl+alt+l"));
    form->addRow(I18n::instance().t("settings.add_hotkey"), m_hotkeyAdd);

    auto *pyHost = new QWidget(this);
    auto *pyRow = new QHBoxLayout(pyHost);
    pyRow->setContentsMargins(0, 0, 0, 0);
    pyRow->setSpacing(6);
    m_python = new QLineEdit(cfg.pythonPath(), pyHost);
    m_python->setPlaceholderText(I18n::instance().t("settings.python_auto"));
    pyRow->addWidget(m_python, 1);
    auto *browse = new QPushButton(I18n::instance().t("settings.browse"), pyHost);
    browse->setFixedWidth(64);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, I18n::instance().t("settings.python"), QString(),
            QStringLiteral("pythonw.exe (*.exe);;所有文件 (*.*)"));
        if (!file.isEmpty())
            m_python->setText(QDir::toNativeSeparators(file));
    });
    pyRow->addWidget(browse);
    form->addRow(I18n::instance().t("settings.python"), pyHost);

    root->addLayout(form);

    auto *pyNote = new QLabel(I18n::instance().t("settings.python_note"), this);
    pyNote->setWordWrap(true);
    pyNote->setStyleSheet(QString("color: %1; font-size: 12px;")
                              .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    root->addWidget(pyNote);

    auto *note = new QLabel(I18n::instance().t("settings.hotkey_note"), this);
    note->setWordWrap(true);
    note->setStyleSheet(QString("color: %1; font-size: 12px;")
                            .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    root->addWidget(note);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    auto *openDir = new QPushButton(I18n::instance().t("settings.open_dir"), this);
    connect(openDir, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppConfig::instance().dataDir()));
    });
    row->addWidget(openDir);

    auto *shellBtn = new QPushButton(this);
    auto refreshShell = [shellBtn] {
        shellBtn->setText(ShellIntegration::isInstalled()
                              ? I18n::instance().t("settings.shell_remove")
                              : I18n::instance().t("settings.shell_install"));
    };
    refreshShell();
    connect(shellBtn, &QPushButton::clicked, this, [this, refreshShell] {
        QString err;
        const bool ok = ShellIntegration::isInstalled()
            ? ShellIntegration::uninstall(&err)
            : ShellIntegration::install(&err);
        // 注册表写入失败别静默吞掉，用户得知道右键菜单其实没装上
        if (!ok && !err.isEmpty())
            QMessageBox::warning(this, I18n::instance().t("settings.title"), err);
        refreshShell();
    });
    row->addWidget(shellBtn);

    row->addStretch(1);
    root->addLayout(row);

    auto *shellNote = new QLabel(I18n::instance().t("settings.shell_note"), this);
    shellNote->setWordWrap(true);
    shellNote->setStyleSheet(QString("color: %1; font-size: 12px;")
                                 .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    root->addWidget(shellNote);

    auto *hideBox = new QVBoxLayout;
    hideBox->setContentsMargins(0, 2, 0, 0);
    hideBox->setSpacing(2);

    m_hideDesktop = new QCheckBox(I18n::instance().t("settings.hide_desktop"), this);
    m_hideDesktop->setChecked(cfg.hideDesktopFrame());
    hideBox->addWidget(m_hideDesktop);

    auto *hideNote = new QLabel(I18n::instance().t("settings.hide_desktop_note"), this);
    hideNote->setWordWrap(true);
    hideNote->setStyleSheet(QString("color: %1; font-size: 12px;")
                                .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    hideBox->addWidget(hideNote);

    root->addLayout(hideBox);

    auto *bootBox = new QVBoxLayout;
    bootBox->setContentsMargins(0, 2, 0, 0);
    bootBox->setSpacing(2);

    m_autoStart = new QCheckBox(I18n::instance().t("settings.autostart"), this);
    m_autoStart->setChecked(ShellIntegration::isAutoStartEnabled());
    bootBox->addWidget(m_autoStart);

    auto *bootNote = new QLabel(I18n::instance().t("settings.autostart_note"), this);
    bootNote->setWordWrap(true);
    bootNote->setStyleSheet(QString("color: %1; font-size: 12px;")
                                .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    bootBox->addWidget(bootNote);

    root->addLayout(bootBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(I18n::instance().t("settings.ok"));
    buttons->button(QDialogButtonBox::Cancel)->setText(I18n::instance().t("settings.cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::applyAndClose);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // 版本号放右下角，一眼能看到当前用的是哪一版
    auto *version = new QLabel(QStringLiteral("LuoDeck v" LUODECK_VERSION), this);
    QFont vf = version->font();
    vf.setPixelSize(11);
    version->setFont(vf);
    version->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    version->setStyleSheet(QString("color: %1; background: transparent;")
                               .arg(Theme::instance().color("textMuted").name(QColor::HexRgb)));
    root->addWidget(version);
}

void SettingsDialog::applyAndClose()
{
    AppConfig &cfg = AppConfig::instance();

    const QString theme = m_theme->currentText();
    const QString lang = m_language->currentText();
    const QString hotkey = m_hotkey->text().trimmed();

    bool hotkeyDirty = false;

    if (!theme.isEmpty() && theme != cfg.theme()) {
        cfg.setTheme(theme);
        Theme::instance().load(theme);
    }

    if (!lang.isEmpty() && lang != cfg.language()) {
        cfg.setLanguage(lang);
        I18n::instance().load(lang);
    }

    if (!hotkey.isEmpty() && hotkey.compare(cfg.hotkey(), Qt::CaseInsensitive) != 0) {
        cfg.setHotkey(hotkey);
        hotkeyDirty = true;
    }

    if (m_hotkeyAdd) {
        const QString addKey = m_hotkeyAdd->text().trimmed();
        if (!addKey.isEmpty() && addKey.compare(cfg.hotkeyAdd(), Qt::CaseInsensitive) != 0) {
            cfg.setHotkeyAdd(addKey);
            hotkeyDirty = true;
        }
    }

    if (m_python && m_python->text().trimmed() != cfg.pythonPath())
        cfg.setPythonPath(m_python->text().trimmed());

    if (hotkeyDirty)
        emit hotkeyChanged();

    if (m_hideDesktop && m_hideDesktop->isChecked() != cfg.hideDesktopFrame())
        cfg.setHideDesktopFrame(m_hideDesktop->isChecked());

    if (m_autoStart)
        ShellIntegration::setAutoStart(m_autoStart->isChecked());

    accept();
}
