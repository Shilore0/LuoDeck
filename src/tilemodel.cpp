#include "tilemodel.h"

#include "iconprovider.h"

#include <QDir>
#include <QFileInfo>

namespace {
constexpr int kMaxChildren = 60;
}

TileModel::TileModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant TileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return r.entry.name;
    case Qt::DecorationRole:
        return IconProvider::instance().icon(r.entry.path);
    case Qt::ToolTipRole:
        return r.entry.path;
    case PathRole:
        return r.entry.path;
    case IsFolderRole:
        return r.entry.isFolder;
    case ExpandedRole:
        return m_expanded.value(r.entry.id, false);
    case DepthRole:
        return r.depth;
    case IdRole:
        return r.entry.id;
    case ParentIdRole:
        return r.parentId;
    default:
        return {};
    }
}

Qt::ItemFlags TileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void TileModel::appendChildren(const TileEntry &folder)
{
    QDir dir(folder.path);
    if (!dir.exists())
        return;

    const QFileInfoList list = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    int added = 0;
    for (const QFileInfo &fi : list) {
        if (added >= kMaxChildren)
            break;

        Row c;
        c.entry.id = folder.id + "::" + fi.fileName();
        c.entry.path = QDir::fromNativeSeparators(fi.absoluteFilePath());
        c.entry.isFolder = fi.isDir();
        c.entry.name = c.entry.isFolder ? fi.fileName() : AppConfig::displayNameFor(c.entry.path);
        c.entry.expanded = false;
        c.depth = 1;
        c.parentId = folder.id;
        c.isChild = true;

        m_rows.append(c);
        ++added;
    }
}

void TileModel::rebuild()
{
    beginResetModel();
    m_rows.clear();

    const QVector<TileEntry> &items = AppConfig::instance().items();
    for (const TileEntry &e : items) {
        Row r;
        r.entry = e;
        r.entry.isFolder = QFileInfo(e.path).isDir();
        r.depth = 0;
        m_rows.append(r);

        if (r.entry.isFolder && m_expanded.value(e.id, false))
            appendChildren(e);
    }

    endResetModel();
}

bool TileModel::toggleFolderAt(int row)
{
    if (row < 0 || row >= m_rows.size())
        return false;

    const Row r = m_rows.at(row);
    if (!r.entry.isFolder || r.isChild)
        return false;

    const bool now = !m_expanded.value(r.entry.id, false);
    m_expanded.insert(r.entry.id, now);
    rebuild();
    return now;
}

void TileModel::setFolderCollapsedAll()
{
    if (m_expanded.isEmpty())
        return;
    m_expanded.clear();
    rebuild();
}

int TileModel::rowOfId(const QString &id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).entry.id == id)
            return i;
    }
    return -1;
}

const TileModel::Row *TileModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return nullptr;
    return &m_rows.at(row);
}
