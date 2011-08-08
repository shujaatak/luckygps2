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

#include "tiledownload.h"
#include "tilemanagement.h"

#include <QApplication>
#include <QDir>
#include <QFile>


#ifdef Q_CC_MSVC
#define snprintf _snprintf
#endif

/* Get a maximum of 10 tiles parallel */
#define MAX_TILES_GET 10

TileDownload::TileDownload(QObject *parent)
	: QObject(parent)
{
	_autodownload = true;

	_dlManager = new QNetworkAccessManager(this);
	connect(_dlManager, SIGNAL(finished(QNetworkReply*)), SLOT(dlDownloadFinished(QNetworkReply*)));
}

TileDownload::~TileDownload()
{
	delete _dlManager;
}

void TileDownload::dlGetTiles(TileListP tileList)
{
	/* create path if it does not exist */
	QDir dir;
	for(int i = 0; i < tileList.length(); i++)
	{
		dir.setPath(tileList[i]->_path.arg(tileList[i]->_z).arg(tileList[i]->_x).arg("").arg("").arg(""));
		if(!dir.exists())
			dir.mkpath(tileList[i]->_path.arg(tileList[i]->_z).arg(tileList[i]->_x).arg("").arg("").arg(""));
	}

	for(int i = 0; i < tileList.length(); i++)
	{
		Tile *tmpTile = tileList.takeLast();

		if((_dlTilesLeft.length() + _dlTilesTodo.length() > MAX_TILES_GET) || !_inet)
			_dlTilesTodo.append(tmpTile);
		else
			dlQueueTile(tmpTile);
	}
}

void TileDownload::dlGetTiles()
{
	for(int i = 0; i < _dlTilesTodo.length(); i++)
	{
		Tile *tmpTile = _dlTilesTodo.takeLast();

		if(!(_dlTilesLeft.length() > MAX_TILES_GET))
			dlQueueTile(tmpTile);
	}
}

void TileDownload::dlQueueTile(Tile *tile)
{
	char buffer[1024];

	memset(buffer, 0, sizeof(buffer));

	::snprintf(buffer, sizeof(buffer), tile->_url.toAscii(), tile->_z, tile->_x, tile->_y);
	QString urlstring = QString::fromAscii(buffer);
	QUrl url = QUrl::fromEncoded(urlstring.toAscii());

	QNetworkRequest request(url);
	QNetworkReply *reply = tile->_reply = _dlManager->get(request);

	_dlCurrentDownloads.append(reply);

	_dlTilesLeft.append(tile);
}


void TileDownload::dlDownloadFinished(QNetworkReply *reply)
{
	QUrl url = reply->url();
	Tile *downloadedTile = NULL;

	for(int i = 0; i < _dlTilesLeft.count(); i++)
	{
		if(_dlTilesLeft[i]->_reply == reply)
		{
			downloadedTile = _dlTilesLeft.takeAt(i);
			break;
		}
	}

	if (reply->error())
	{
		qDebug("Download of %s failed: %s", url.toEncoded().constData(), qPrintable(reply->errorString()));
	}
	else
	{
		if(downloadedTile)
		{
			QString filename = get_tilename(downloadedTile->_x, downloadedTile->_y, downloadedTile->_z, downloadedTile->_path);
			QFile file(filename);

			if(!file.exists() && file.open(QIODevice::WriteOnly))
			{
				file.write(reply->readAll());
				file.close();
			}
		}
	}

	if(downloadedTile)
		delete downloadedTile;

	if(_dlTilesTodo.length() > 0 && _inet)
		dlQueueTile(_dlTilesTodo.takeLast());

	_dlCurrentDownloads.removeAll(reply);
	reply->deleteLater();
}


