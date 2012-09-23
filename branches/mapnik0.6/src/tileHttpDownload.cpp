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

#include "tileHttpDownload.h"
#include "filetilemanager.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QImageReader>


#ifdef Q_CC_MSVC
#define snprintf _snprintf
#endif

/* Get a maximum of 10 tiles parallel */
#define MAX_TILES_GET 10

TileHttpDownload::TileHttpDownload(DataSource *ds, QObject *parent)
	: DataSource(parent)
{
	_ds = ds;

	set_autodownload(true);
	set_inet(true);

	_dlManager = new QNetworkAccessManager(this);
	connect(_dlManager, SIGNAL(finished(QNetworkReply*)), SLOT(dlDownloadFinished(QNetworkReply*)));
}

TileHttpDownload::~TileHttpDownload()
{
	delete _dlManager;
}

QImage *TileHttpDownload::loadMapTile(const Tile *mytile)
{
	Tile *newtile = new Tile(*mytile, NULL);
	delete mytile;

	if((_dlTilesLeft.length() + _dlTilesTodo.length() > MAX_TILES_GET) || !get_inet())
	{
		_dlTilesTodo.append(newtile);
	}
	else
	{
		dlQueueTile(newtile);
	}

	return NULL;
}

#if 0
/* If internet connection is up again, put missing tiles into download queue again */
void TileHttpDownload::dlGetTiles()
{
	for(int i = 0; i < _dlTilesTodo.length(); i++)
	{
		Tile *tmpTile = _dlTilesTodo.takeLast();

		if(!(_dlTilesLeft.length() > MAX_TILES_GET))
			dlQueueTile(tmpTile);
	}
}
#endif

void TileHttpDownload::dlQueueTile(Tile *tile)
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


void TileHttpDownload::dlDownloadFinished(QNetworkReply *reply)
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
		/* Save downloaded file */
		if(downloadedTile)
		{
			QImageReader *iread = new QImageReader();
			iread->setAutoDetectImageFormat(false);
			iread->setFormat("PNG");
			iread->setDevice(reply);
			QImage img = iread->read();
			delete iread;

			_ds->saveMapTile(&img, downloadedTile);
		}
	}

	if(downloadedTile)
		delete downloadedTile;

	if(_dlTilesTodo.length() > 0 && get_inet())
		dlQueueTile(_dlTilesTodo.takeLast());

	_dlCurrentDownloads.removeAll(reply);
	reply->deleteLater();
}


