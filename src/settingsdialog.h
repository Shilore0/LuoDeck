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

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void applyAndClose();

private:
    QComboBox *m_theme = nullptr;
    QComboBox *m_language = nullptr;
    QLineEdit *m_hotkey = nullptr;
    QLineEdit *m_hotkeyAdd = nullptr;
    QLineEdit *m_python = nullptr;
    QCheckBox *m_hideDesktop = nullptr;
    QCheckBox *m_autoStart = nullptr;
};
