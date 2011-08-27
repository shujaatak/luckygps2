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

#include <QHttp>
#include <QPainter>

#include "datasourcemanager.h"
#include "filetilemanager.h"
#include "sqliteTileManager.h"


DataSourceManager::DataSourceManager(QObject *parent)
	: QObject(parent)
{
	_gotMissingTiles = false;

	_cacheSize = 0;
	_outlineCacheSize = 0;

	_mapImg = NULL;
	_outlineMapImg = NULL;

	memset(&_tileInfo, 0, sizeof(TileInfo));

	/* timer to check for internet connection once in a while */
	connect(&_inetTimer, SIGNAL(timeout()), this, SLOT(callback_inet_connection_update()));
	_inetTimer.start();

	_networkManager = new QNetworkAccessManager(this);
	connect(_networkManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(slotRequestFinished(QNetworkReply *)));

	_dsFile = new SQLiteTileMgr(this); // new FileTileMgr(this);
	_dsHttp = new TileHttpDownload(_dsFile, this);
}

DataSourceManager::~DataSourceManager()
{
	if(_mapImg)
		delete _mapImg;
	if(_outlineMapImg)
		delete _outlineMapImg;

	while(_outlineCache.length() > 0)
	{
		Tile mytile = _outlineCache.takeFirst();
		if(mytile._img)
			delete mytile._img;
	}

	while(_cache.length() > 0)
	{
		Tile mytile = _cache.takeFirst();
		if(mytile._img)
			delete mytile._img;
	}

	delete _networkManager;

	if(_dsHttp)
		delete _dsHttp;

	if(_dsFile)
		delete _dsFile;
}

void DataSourceManager::callback_inet_connection_update()
{
	if(_dsHttp->get_autodownload())
	{
		QUrl url = QUrl::fromEncoded("http://www.google.de");
		QNetworkRequest request(url);
		QNetworkReply *reply = _networkManager->get(request);

		_inetTimer.start(10000);
	}
	else
		_inetTimer.start(1000);
}

void DataSourceManager::callback_http_finished()
{
	_dsHttp->set_inet(true);
}

void DataSourceManager::callback_no_inet()
{
	_dsHttp->set_inet(false);
}

void DataSourceManager::slotRequestFinished(QNetworkReply *reply)
{
	if (reply->error() > 0)
	{
		QString message = "Error number = " + reply->errorString();
		qDebug("%s\n", message.toAscii().constData());

		callback_no_inet();
	}
	else
	{
		callback_http_finished();
	}

	reply->abort();
	reply->deleteLater();
}


/* This function is part of the MapWidget to be able to access plugins */
QImage *DataSourceManager::fill_tiles_pixel(TileList *requested_tiles, TileList *missing_tiles, TileList *cache, int nx, int ny)
{
	QImage *paintImg = NULL;
	QPainter painter;
	int i;

	/* init painting on a pixmap/image */
	paintImg = new QImage(nx * TILE_SIZE, ny * TILE_SIZE, QImage::Format_RGB32);
	if(!paintImg)
		return NULL;
	painter.begin(paintImg);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.fillRect(0,0, nx * TILE_SIZE, ny * TILE_SIZE, Qt::white);

	for (i = 0; i < requested_tiles->length(); i++)
	{
		int cache_index = -1;
		if((cache_index = cache->indexOf(requested_tiles->at(i))) < 0)
		{
			QImage *img = NULL;
			if(NULL)
				img = NULL; //_tilesManager->RequestTile(requested_tiles->at(i)._x, requested_tiles->at(i)._y, requested_tiles->at(i)._z);
			else
				img = _dsFile->loadMapTile(&(requested_tiles->at(i)));

			if(img)
			{
				cache->append(Tile(requested_tiles->at(i), img));
				painter.drawImage(requested_tiles->at(i)._posx, requested_tiles->at(i)._posy, *img);
			}
			else
			{
				missing_tiles->append(requested_tiles->at(i));
			}
		}
		else
		{
			painter.drawImage(requested_tiles->at(i)._posx, requested_tiles->at(i)._posy, *(cache->at(cache_index)._img));
		}

	}
	painter.end();

	return paintImg;
}

QImage *DataSourceManager::getImage(int x, int y, int zoom, int width, int height, TileInfo &tile_info)
{
	TileList missingTiles;
	QImage *mapImg = NULL;

	/* generate list of tiles which are needed for the current x, y and zoom level */
	TileList *requested_tiles = get_necessary_tiles(x, y, zoom, width, height, _mapPath, tile_info);

	/* check if the same tile rect is requested as the last time */
	if(_mapImg && !_gotMissingTiles &&
	   _tileInfo.min_tile_x== tile_info.min_tile_x &&
	   _tileInfo.min_tile_y== tile_info.min_tile_y &&
	   _tileInfo.max_tile_x== tile_info.max_tile_x &&
	   _tileInfo.max_tile_y== tile_info.max_tile_y)
	{
		mapImg = _mapImg;
	}
	else
	{

		/* Let's center the coordinates here using width + height of map widget */
		mapImg = fill_tiles_pixel(requested_tiles, &missingTiles, &_cache, tile_info.nx, tile_info.ny);
		delete _mapImg;
		_mapImg = mapImg;
	}

	memcpy(&_tileInfo, &tile_info, sizeof(TileInfo));
	delete requested_tiles;

	if(missingTiles.length() > 0)
		_gotMissingTiles = true;
	else
		_gotMissingTiles = false;

	/* check tile cache for MAX items */
	while(_cache.length() > _cacheSize)
	{
		Tile mytile = _cache.takeFirst();
		if(mytile._img)
			delete mytile._img;
	}

	/* -------------------------------------------------- */
	/* The following stuff got nothing to do with drawing */
	/* -------------------------------------------------- */

	/* request missing tiles from internet/renderer */
	{
		/* put tile into missing tiles (thread) list */
		TileListP tileList;

		for(int i = 0; i < missingTiles.length(); i++)
			_dsHttp->loadMapTile(new Tile(missingTiles[i]._x, missingTiles[i]._y, missingTiles[i]._z, _mapPath, _mapUrl));
	}

	return  mapImg;
}
