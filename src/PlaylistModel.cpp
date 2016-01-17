#include "PlaylistModel.h"

#include <fileref.h>

#include <QDataStream>
#include <QDebug>
#include <QMimeData>

namespace splay
{

PlaylistModel::PlaylistModel(QObject* parent)
	: QAbstractTableModel{ parent }
	, mData{}
{
	mData.setPlaybackMode(QMediaPlaylist::Loop);
}

void PlaylistModel::Add(const QStringList& pathList)
{
	QList<QMediaContent> mediaList;

	for (const auto& path : pathList) {
		mediaList.push_back(QMediaContent{ path });
	}

	Q_EMIT layoutAboutToBeChanged();

	auto oldModelIndex = QModelIndex();
	auto res = mData.addMedia(mediaList);

	if (!res) {
		throw std::runtime_error{ mData.errorString().toStdString() };
	}

	changePersistentIndex(oldModelIndex, QModelIndex());

	Q_EMIT layoutChanged();
}

int PlaylistModel::columnCount(const QModelIndex& parent) const
{
	// Note! When implementing a table based model,
	// columnCount() should return 0 when the parent is valid.
	return parent.isValid() ? 0 : 4;
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid()) {
		return QVariant();
	}

	const auto row(index.row());
	const auto column(index.column());

	if (row >= static_cast<int>(mData.mediaCount())) {
		return QVariant();
	}

	TagLib::FileRef f{ mData.media(row).canonicalUrl().toString().toStdString().c_str() };

	if (role == Qt::DisplayRole) {
		switch (column) {
		case 1: {
			if (!f.isNull() && f.tag()) {
				return QVariant(f.tag()->artist().toCString());
			}
		}
		case 2: {
			if (!f.isNull() && f.tag()) {
				return QVariant(f.tag()->title().toCString());
			}
		}
		case 3: {
			if (!f.isNull() && f.audioProperties()) {
				return QVariant(f.audioProperties()->length());
			}
		}
		}
	}

	return QVariant();
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const
{
	auto defFlags = QAbstractTableModel::flags(index);

	if (index.isValid()) {
		return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defFlags;
	}
	else {
		return Qt::ItemIsDropEnabled | defFlags;
	}
}

QVariant PlaylistModel::headerData(int section,
	Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole) {
		if (orientation == Qt::Horizontal) {
			switch (section) {
			case 0:
				return QLatin1String("Playing");
			case 1:
				return QLatin1String("Author");
			case 2:
				return QLatin1String("Title");
			case 3:
				return QLatin1String("Duration");
			}
		}
	}

	return QVariant();
}

QStringList PlaylistModel::mimeTypes() const
{
	QStringList types;
	types << "application/splay-track";
	return types;
}

void PlaylistModel::OnInsert(QStringList list)
{
	const QModelIndex parent = QModelIndex();
	const auto row = mData.mediaCount();
	const auto cnt = list.size();

	beginInsertRows(parent, row, row + cnt - 1);

	for (const auto& path : list) {
		mData.addMedia(QMediaContent{ path });
	}

	endInsertRows();
}

void PlaylistModel::OnMove(RowList selectedRows, int dest)
{
	qDebug() << "PlaylistModel::OnMove: number = " << selectedRows.size() << " dest = " << dest;

	const QModelIndex parent = QModelIndex();
	// Number of rows for the moving.
	const auto num(static_cast<int>(selectedRows.size()));

	auto res = beginMoveRows(parent, selectedRows[0], selectedRows[num - 1], parent, dest);

	if (!res) {
		qDebug() << "PlaylistModel::OnMove: Result is " << res;
		return;
	}

	// Copy rows whose have been moved into the temporary vector.
	Playlist tmp;
	for (int i = 0; i < num; ++i) {
		tmp.addMedia(mData.media(selectedRows[i]));
	}
	// Delete moved rows from current playlist.
	for (const auto& row : selectedRows) {
		mData.removeMedia(row);
	}

	if (dest > static_cast<int>(mData.mediaCount()))
		dest = mData.mediaCount();

	int coef = 0;
	if (selectedRows[0] < dest) {
		coef = -1;
	}

	// Move selected rows to the destination.
	//mData.insert(std::begin(mData) + dest + coef,
	//	std::begin(tmp), std::end(tmp));

	endMoveRows();
}

void PlaylistModel::OnRemove(RowList selectedRows)
{
	int coef = 0;

	// coef var is used for the correction. We should update
	// values in the list of selected rows after erasing,
	// since size of list on every iteration decreasing.

	Q_EMIT layoutAboutToBeChanged();

	auto oldModelIndex = QModelIndex();

	for (const auto& row : selectedRows) {
		mData.removeMedia(row - coef);
		++coef;
	}

	changePersistentIndex(oldModelIndex, QModelIndex());

	Q_EMIT layoutChanged();
}

Playlist* PlaylistModel::Open(const QStringList& pathList)
{
	if (mData.mediaCount() > 0) {
		_Clear();
	}
	Add(pathList);
	mData.setCurrentIndex(0);

	return &mData;
}

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
	// Note! When implementing a table based model,
	// rowCount() should return 0 when the parent is valid.
	return parent.isValid() ? 0 : mData.mediaCount();
}

Qt::DropActions PlaylistModel::supportedDropActions() const
{
	return Qt::CopyAction | Qt::MoveAction;
}

void PlaylistModel::_Clear()
{
	Q_EMIT layoutAboutToBeChanged();

	auto oldModelIndex = QModelIndex();

	auto res = mData.clear();

	if (!res) {
		throw std::runtime_error{ mData.errorString().toStdString() };
	}

	changePersistentIndex(oldModelIndex, QModelIndex());

	Q_EMIT layoutChanged();
}

} // namespace splay
