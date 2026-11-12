#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

#include "winapps.h"

class QLineEdit;
class QListWidget;

// 「添加 Windows 应用…」：把 shell:AppsFolder 里的商店应用列出来勾选，
// 这些应用没有可拖的文件路径，只能这样加。
class WinAppDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WinAppDialog(QWidget *parent = nullptr);

    // 选中的 AUMID
    QStringList selectedAumids() const;
    QString nameFor(const QString &aumid) const;

protected:
    void showEvent(QShowEvent *event) override;

private:
    void fill();
    void filter(const QString &text);

    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QVector<WinApps::Entry> m_entries;
    bool m_filled = false;
};
