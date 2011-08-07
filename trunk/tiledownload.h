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

#ifndef TILEDOWNLOAD_H
#define TILEDOWNLOAD_H

#include <QFile>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QThread>

#include "customwidgets.h"
#include "tile.h"

#include <cmath>

QString get_tilename(int x, int y, int z, QString path);

class TileDownload : public QObject
{

	Q_OBJECT

public:
	TileDownload(QObject *parent = 0);
    ~TileDownload();

	void dlGetTiles(TileListP tileList);
	void dlGetTiles();

	bool get_inet(){ return _inet; }

	void set_inet(bool value)
	{
		if(!_autodownload)
		{
			_inet = 0;
			return;
		}

		if((value != _inet) && value)
			dlGetTiles();

		_inet = value;
	}

	bool get_autodownload() { return _autodownload; }
	void set_autodownload(bool value) { _autodownload = value; if(_inet && !_autodownload) _inet = 0; }

	int dlTilesMissing()
	{
		return _dlTilesLeft.length() + _dlTilesTodo.length();
	}


 private:
	/* Queue tile for retreiving */
	void dlQueueTile(Tile *tile);

	/* Download Manager (used to download all the tiles) */
	QNetworkAccessManager *_dlManager;
	QList<QNetworkReply *> _dlCurrentDownloads;
	TileListP _dlTilesLeft;

	/* Tiles which couldn't go into the queue because it was full */
	TileListP _dlTilesTodo;

	/* True if internet connection available */
	bool _inet;

	/* false if using internet is not allowed */
	bool _autodownload;

private slots:
	/* Download Manager callback function */
	void dlDownloadFinished(QNetworkReply *reply);

};

#endif // TILEDOWNLOAD_H
