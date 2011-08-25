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

#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <QObject>

#include "tile.h"

class DataSource : public QObject
{
    Q_OBJECT

public:
	explicit DataSource(QObject *parent = 0);

	virtual QImage *loadMapTile(Tile mytile) = 0;
	virtual int saveMapTile(Tile mytile) = 0;

private:
	/* Flags to describe what operations are possible on this data source */
	enum dsAccessFlags { DS_ACCESS_READ = 0, DS_ACCESS_WRITE = 1};
	dsAccessFlags accessFlag;

	/* Flags to describe what kind of data source this is */
	// num dsTypeFlags { DS_ACCESS_READ = 0, DS_ACCESS_WRITE = 1};
	// dsTypeFlags accessFlag;

signals:

public slots:

};

#endif // DATASOURCE_H
