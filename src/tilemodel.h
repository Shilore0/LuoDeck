#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "appconfig.h"

class TileModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        IsFolderRole,
        ExpandedRole,
        DepthRole,
        IdRole,
        ParentIdRole,
    };

    explicit TileModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void rebuild();
    bool toggleFolderAt(int row);
    void setFolderCollapsedAll();

    int rowOfId(const QString &id) const;

    struct Row {
        TileEntry entry;
        int depth = 0;
        QString parentId;
        bool isChild = false;
    };

    const Row *rowAt(int row) const;

private:
    void appendChildren(const TileEntry &folder);

    QVector<Row> m_rows;
    QHash<QString, bool> m_expanded;
};
