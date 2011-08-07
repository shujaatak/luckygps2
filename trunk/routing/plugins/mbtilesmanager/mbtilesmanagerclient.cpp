/*
 * This file is part of the luckyGPS project.
 *
 * Copyright (C) 2009 - 2010 Daniel Genrich <dg@luckygps.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QDir>
#include <QFileInfoList>
#include <QtDebug>

#include "mbtilesmanagerclient.h"

MBTilesManager::MBTilesManager()
{
	_db = NULL;
}

MBTilesManager::~MBTilesManager()
{
	if(_db)
		sqlite3_close(_db);
}

bool MBTilesManager::LoadData()
{
	QDir path;

	/* get all route files in folder */
	path.setPath(_directory);

	QFileInfoList file_list = path.entryInfoList(QStringList("*.sqlite"), QDir::Files);

	if(file_list.length() > 0)
	{
		QString file = file_list[0].filePath();

		if(sqlite3_open(file.toUtf8().constData(), &_db) != SQLITE_OK)
		{
			qDebug() << "Cannot open " << file.toUtf8().constData();

			return false;
		}
		else return true;
	}
	else
		return false;
}

QString MBTilesManager::GetName()
{
	return "MBTiles Tile Manager";
}

bool MBTilesManager::IsCompatible( int fileFormatVersion )
{
	if ( fileFormatVersion == 1 )
		return true;
	return false;
}

void MBTilesManager::SetInputDirectory( const QString& dir )
{
	_directory = dir;
}

QImage *MBTilesManager::RequestTile(int x, int y, int zoom)
{
	QImage *img = NULL;
	sqlite3_stmt *stmt = NULL;

	if(!_db || zoom > GetMaxZoom())
	{
		return NULL;
	}

	if(sqlite3_prepare_v2(_db, QString("SELECT tile_data FROM tiles WHERE zoom_level=\"%1\" AND tile_column=\"%2\" AND tile_row=\"%3\" LIMIT 1;").arg(zoom).arg(x).arg(y).toUtf8().constData(), -1, &stmt, NULL) == SQLITE_OK)
	{
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
			img = new QImage();
			img->loadFromData(reinterpret_cast<const uchar *>(sqlite3_column_blob(stmt, 0)), sqlite3_column_bytes(stmt, 0), "PNG");
		}
		sqlite3_finalize(stmt);

		if(img && img->isNull())
		{
			delete img;
			img = NULL;
		}
	}

	return img;
}

int MBTilesManager::GetMaxZoom()
{
	return 13;
}

Q_EXPORT_PLUGIN2( mbtilesmanagerclient, MBTilesManager )
