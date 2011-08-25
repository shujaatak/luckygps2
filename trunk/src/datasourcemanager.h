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

#ifndef DATASOURCEMANAGER_H
#define DATASOURCEMANAGER_H

#include <QImage>
#include <QObject>
#include <QTimer>

#include "datasource.h"
#include "tileHttpDownload.h"

class DataSourceManager : public QObject
{
    Q_OBJECT

public:
	explicit DataSourceManager(QObject *parent = 0);
	~DataSourceManager();

	QImage *getImage(int x, int y, int zoom, int width, int height, TileInfo &tile_info);

	void setCacheSize(int cacheSize)
	{
		_cacheSize = cacheSize;
		_outlineCacheSize = cacheSize / 4;
		if(_outlineCacheSize < 4) _outlineCacheSize = 4;
	}

	/* Map settings */
	void setPath(QString path) { _mapPath = path; }
	QString getPath() { return _mapPath; }
	void setUrl(QString url) { _mapUrl = url; }
	QString getUrl() { return _mapUrl; }

	void callback_no_inet();
	void callback_http_finished();

	/* Tiles Manager */
	// TileDownload *_tilesManager;

private:

	QImage *fill_tiles_pixel(TileList *requested_tiles, TileList *missing_tiles, TileList *cache, int nx, int ny);

	/* TODO Array of data sources like cache, database, file, internet, mapnik, also support read/write access flags */
	DataSource *_dsCache;
	DataSource *_dsFile; /* can also be a database file, needs read/write access */
	DataSource *_dsHttp;
	DataSource *_dsMapnik;
	// loadMapTile() -> try to read from cache -> try to read from db/file -> try to get from internet -> try to get from mapnik
	// if(img)  if (img NOT from cache)write to cache if(img not from db) write to db/(file?)

	/* MapWidget tile cache */
	TileList _cache;
	int _cacheSize;
	TileList _outlineCache;
	int _outlineCacheSize;
	TileInfo _tileInfo;
	bool _gotMissingTiles;
	QImage *_mapImg;

	/* images served for the overview map widget */
	QImage *_outlineMapImg;

	/* map data management */
	QString _mapPath;
	QString _mapUrl;

	/* Internet connection detection handling */
	QTimer _inetTimer;
	QNetworkAccessManager *_networkManager;

signals:

public slots:

private slots:
	/* Internet connection detection handling */
	void callback_inet_connection_update();
	void slotRequestFinished(QNetworkReply *reply);

};

#endif // DATASOURCEMANAGER_H
