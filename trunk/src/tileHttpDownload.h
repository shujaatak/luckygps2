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

#ifndef TILEHTTPDOWNLOAD_H
#define TILEHTTPDOWNLOAD_H

#include <QFile>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QThread>

#include "customwidgets.h"
#include "datasource.h"

#include <cmath>


class TileHttpDownload : public DataSource
{

	Q_OBJECT

public:
	TileHttpDownload(DataSource *ds, QObject *parent = 0);
	~TileHttpDownload();

	virtual QImage *loadMapTile(const Tile *mytile);

	// void dlGetTiles(TileListP tileList);
	// TODO void dlGetTiles();

#if 0
	// not used
	int dlTilesMissing()
	{
		return _dlTilesLeft.length() + _dlTilesTodo.length();
	}
#endif

 private:
	/* Queue tile for retreiving */
	void dlQueueTile(Tile *tile);

	/* Download Manager (used to download all the tiles) */
	QNetworkAccessManager *_dlManager;
	QList<QNetworkReply *> _dlCurrentDownloads;
	TileListP _dlTilesLeft;

	/* Tiles which couldn't go into the queue because it was full */
	TileListP _dlTilesTodo;

	/* data sink: Where to write the downloaded images */
	DataSource *_ds;

private slots:
	/* Download Manager callback function */
	void dlDownloadFinished(QNetworkReply *reply);

};

#endif // TILEHTTPDOWNLOAD_H
