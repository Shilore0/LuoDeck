#pragma once

#include <QSize>
#include <QStyledItemDelegate>

class TileDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TileDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void setCellSize(const QSize &size);

private:
    QSize m_cell{88, 94};
};
