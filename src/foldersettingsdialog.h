#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

#include "appconfig.h"

class QLabel;
class QLineEdit;
class QPushButton;

// 文件夹设置：改名 + 选边框颜色。系统框只读。
// 颜色点击后立刻发出预览信号，让主面板实时反映；取消时回滚。
class FolderSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FolderSettingsDialog(const LayoutItem &item, QWidget *parent = nullptr);

    QString folderName() const;
    QString folderColor() const { return m_color; }

signals:
    void colorPreviewed(const QString &color);

protected:
    void showEvent(QShowEvent *event) override;
    void reject() override;

private:
    void selectColor(const QString &color);

    LayoutItem m_item;
    QLineEdit *m_name = nullptr;
    QString m_color;
    QString m_originalColor;
    QVector<QPushButton *> m_colorButtons;
};
