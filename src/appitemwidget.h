#pragma once

#include <QIcon>

#include "itemwidget.h"

class AppItemWidget : public ItemWidget
{
    Q_OBJECT
public:
    AppItemWidget(const QString &id, const QString &name, const QString &path,
                  QWidget *parent = nullptr);

    void setDisplayName(const QString &name);
    QString path() const { return m_path; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_name;
    QString m_path;
    QIcon m_icon;
};
