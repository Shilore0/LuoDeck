#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

#include "appconfig.h"

class QLineEdit;
class QPushButton;
class QLabel;

// 文件夹设置：改名 + 选边框颜色。系统框只读。
class FolderSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FolderSettingsDialog(const LayoutItem &item, QWidget *parent = nullptr);

    QString folderName() const;
    QString folderColor() const { return m_color; }

private:
    void selectColor(const QString &color);

    LayoutItem m_item;
    QLineEdit *m_name = nullptr;
    QString m_color;
    QVector<QPushButton *> m_colorButtons;
};
