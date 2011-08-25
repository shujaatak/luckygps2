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
#include <QDir>
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

	/* create path if it does not exist */
	QDir dir;

	dir.setPath(newtile->_path.arg(newtile->_z).arg(newtile->_x).arg("").arg("").arg(""));
	if(!dir.exists())
		dir.mkpath(newtile->_path.arg(newtile->_z).arg(newtile->_x).arg("").arg("").arg(""));

	if((_dlTilesLeft.length() + _dlTilesTodo.length() > MAX_TILES_GET) || !get_inet())
	{
		qDebug("append tile");
		_dlTilesTodo.append(newtile);
	}
	else
	{
		qDebug("queue tile");
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
			QImage img = QImageReader(reply).read();
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


