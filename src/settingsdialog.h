#pragma once

#include <QDialog>

class QComboBox;
class QLineEdit;
class QCheckBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

signals:
    void hotkeyChanged();

private slots:
    void applyAndClose();

private:
    QComboBox *m_theme = nullptr;
    QComboBox *m_language = nullptr;
    QLineEdit *m_hotkey = nullptr;
    QCheckBox *m_hideDesktop = nullptr;
};
