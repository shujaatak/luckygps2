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

#include <QBuffer>
#include <QDebug>
#include <QImage>

#ifdef WITH_IMAGEMAGICK
#include <Magick++.h>
#endif

#include <cmath>

#include "sqliteTileManager.h"


SQLiteTileMgr::SQLiteTileMgr(QObject *parent) : DataSource(parent)
{
	_db = NULL;
}

SQLiteTileMgr::~SQLiteTileMgr()
{
	if(_db)
		sqlite3_close(_db);
}

bool SQLiteTileMgr::LoadDatabase(QString path)
{
	QString filename = path + "/map.sqlite";

	if(sqlite3_open(filename.toUtf8().constData(), &_db) != SQLITE_OK)
	{
		qDebug() << "Cannot open " << filename.toUtf8().constData();

		if(_db)
		{
			sqlite3_close(_db);
			_db = NULL;
		}

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
			// qDebug(sql.toUtf8().constData());
			sqlite3_free (sql_err);

			if(_db)
			{
				sqlite3_close(_db);
				_db = NULL;
			}

			return false;
		}
		return true;
	}
}

bool SQLiteTileMgr::CreateDatabase(QString path)
{
	QString filename = path + "/map.sqlite";

	if(sqlite3_open(filename.toUtf8().constData(), &_db) != SQLITE_OK)
	{
		qDebug() << "Cannot open " << filename.toUtf8().constData();

		return false;
	}
	else
	{
		char *sql_err = NULL;
		QString sql = "CREATE TABLE tiles (zoom_level INTEGER NOT NULL, tile_column INTEGER NOT NULL, tile_row INTEGER NOT NULL, tile_data BLOB, PRIMARY KEY(zoom_level, tile_column, tile_row));";
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

QImage *SQLiteTileMgr::loadMapTile(const Tile *mytile)
{
	QImage *img = NULL;
	sqlite3_stmt *stmt = NULL;

	QString path = mytile->_path;
	path.truncate(path.indexOf("%1"));

	if(!_db)
	{
		if(!LoadDatabase(path))
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

int SQLiteTileMgr::saveMapTile(QImage *img, const Tile *mytile)
{
	if(!img || img->isNull())
	{
		return false;
	}

	QString path = mytile->_path;
	path.truncate(path.indexOf("%1"));

	if(!_db && !LoadDatabase(path))
	{
		if(!CreateDatabase(path))
		{
			return false;
		}
	}

	sqlite3_stmt *stmt = NULL;
	char *sql_err = NULL;
	QString sql = "INSERT OR IGNORE INTO tiles VALUES(%1, %2, %3, ?);";
	sql = sql.arg(mytile->_z).arg(mytile->_x).arg(mytile->_y);

	if(sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL) != SQLITE_OK)
	{
		sql = "Could not prepare statement: " + sql;
		qDebug(sql.toUtf8().constData());
		sqlite3_finalize(stmt);
		return false;
	}

	QByteArray ba;
	QBuffer buffer(&ba);
	buffer.open(QIODevice::WriteOnly);
	img->save(&buffer, "PNG"); // writes image into ba in PNG format
	buffer.close();

#ifdef WITH_IMAGEMAGICK
	/* Load PNG into GraphicsMagick */
	Magick::Blob tmpPng(ba.constData(), ba.size());
	Magick::Image magImg(tmpPng);

	/* Adaptive color reducion, depending on how much colors are in the image */
	unsigned int numColor = magImg.totalColors();
	numColor = std::max(std::min(numColor, 255), 2);
	numColor = pow(2, ceil(log(numColor)/log(2))) / 2;
	// qDebug(QString::number(numColor).toAscii().constData());

	/* Choose compression algorithm */
	magImg.quality(90);

	qDebug(QString::number(numColor).toAscii().constData());
	qDebug(QString::number(magImg.totalColors()).toAscii().constData());

	/* Reduce colors */
	if(numColor > 0 && numColor < magImg.totalColors())
	{
		magImg.quantizeColors( numColor );
		magImg.quantizeDither(false);
		magImg.quantizeColorSpace(Magick::YUVColorspace);
		magImg.quantizeTreeDepth(9);
		magImg.depth(8);
		magImg.quantize();
	}

	/* Save image as indexed 8-bit png */
	magImg.magick("PNG8");
	magImg.write(&tmpPng);

	if(sqlite3_bind_blob(stmt, 1, (const char *)tmpPng.data(), tmpPng.length(), NULL) != SQLITE_OK)
#else
	if(sqlite3_bind_blob(stmt, 1, ba.constData(), ba.size(), NULL) != SQLITE_OK)
#endif
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
