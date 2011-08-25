/*
 * This file is part of the luckyGPS project.
 *
 * Copyright (C) 2009 - 2011 Daniel Genrich <dg@luckygps.com>
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

#include <QDebug>

#include "sqliteTileManager.h"


SQLiteTileManager::SQLiteTileManager()
{
	_db = NULL;
}

SQLiteTileManager::~SQLiteTileManager()
{
	if(_db)
		sqlite3_close(_db);
}

bool SQLiteTileManager::LoadDatabase(QString path)
{
	QString filename = path + "/map.sqlite";
	qDebug(filename.toUtf8().constData());

	if(sqlite3_open(filename.toUtf8().constData(), &_db) != SQLITE_OK)
	{
		qDebug() << "Cannot open " << filename.toUtf8().constData();

		return false;
	}
	else
	{
		/* A second test if mbtiles "tiles" tables exists */
		char *sql_err = NULL;
		QString sql = "SELECT * FROM tiles LIMIT 1;";
		if(sqlite3_exec(_db, sql.toUtf8().constData(), 0, NULL, &sql_err) != SQLITE_OK)
		{
			sql = sql + " error: " + sql_err;
			qDebug(sql.toUtf8().constData());
			sqlite3_free (sql_err);
			return false;
		}
		return true;
	}
}

bool SQLiteTileManager::CreateDatabase(QString path)
{
	QString filename = path + "/map.sqlite";
	qDebug(filename.toUtf8().constData());

	if(sqlite3_open(filename.toUtf8().constData(), &_db) != SQLITE_OK)
	{
		qDebug() << "Cannot open " << filename.toUtf8().constData();

		return false;
	}
	else
	{
		char *sql_err = NULL;
		QString sql = "CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER, tile_data BLOB);";
		if(sqlite3_exec(_db, sql.toUtf8().constData(), 0, NULL, &sql_err) != SQLITE_OK)
		{
			sql = sql + " error: " + sql_err;
			qDebug(sql.toUtf8().constData());
			sqlite3_free (sql_err);

			return false;
		}

		return true;
	}
}

QImage *SQLiteTileManager::loadMapTile(const Tile *mytile)
{
	QImage *img = NULL;
	sqlite3_stmt *stmt = NULL;

	if(!_db)
	{
		if(!LoadDatabase(mytile->_path))
			return NULL;
	}

	if(sqlite3_prepare_v2(_db, QString("SELECT tile_data FROM tiles WHERE zoom_level=\"%1\" AND tile_column=\"%2\" AND tile_row=\"%3\" LIMIT 1;").arg(mytile->_z).arg(mytile->_x).arg(mytile->_y).toUtf8().constData(), -1, &stmt, NULL) == SQLITE_OK)
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

int SQLiteTileManager::saveMapTile(QImage *img, const Tile *mytile)
{
	if(!img || img->isNull())
	{
		return false;
	}

	if(!_db && !LoadDatabase(mytile->_path))
	{
		if(!CreateDatabase(mytile->_path))
		{
			return false;
		}
	}

	sqlite3_stmt *stmt = NULL;
	char *sql_err = NULL;
	QString sql = "INSERT INTO tiles VALUES(%1, %2, %3, ?);";
	sql.arg(mytile->_z).arg(mytile->_x).arg(mytile->_y);

	if(sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL) != SQLITE_OK)
	{
		sql = "Could not prepare statement: " + sql;
		qDebug(sql.toUtf8().constData());
		sqlite3_finalize(stmt);
		return false;
	}
	if(sqlite3_bind_blob(stmt, 1, img->bits(), img->byteCount(), NULL) != SQLITE_OK)
	{
		qDebug("Could not bind argument #1.");
		sqlite3_finalize(stmt);
		return false;
	}
	if(sqlite3_step(stmt) != SQLITE_DONE)
	{
		qDebug("Could not step (execute) stmt.");
		sqlite3_finalize(stmt);
		return false;
	}

	sqlite3_finalize(stmt);

	return false;
}
