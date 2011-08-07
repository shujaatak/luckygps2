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

#ifndef MBTILESMANAGERCLIENT_H
#define MBTILESMANAGERCLIENT_H

#include "sqlite3.h"
#include "interfaces/itilemanager.h"

#include <QImage>
#include <QObject>
#include <QString>

class MBTilesManager : public QObject, public ITilemanager
{
	Q_OBJECT
	Q_INTERFACES( ITilemanager )

public:
	MBTilesManager();
	virtual ~MBTilesManager();

	virtual QImage *RequestTile(int x, int y, int zoom);

	virtual QString GetName();
	virtual void SetInputDirectory( const QString& dir );
	virtual bool IsCompatible( int fileFormatVersion );
	virtual bool LoadData();
	virtual int GetMaxZoom();

signals:

public slots:

protected:

	sqlite3 *_db;
	QString _directory;
};

#endif // MBTILESMANAGERCLIENT_H
