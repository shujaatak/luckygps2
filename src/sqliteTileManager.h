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

#ifndef SQLITETILEMANAGER_H
#define SQLITETILEMANAGER_H

#include <QImage>
#include <QString>

#include "sqlite3.h"

#include "tile.h"

#include "datasource.h"


class SQLiteTileManager : public DataSource
{
	Q_OBJECT

public:
    SQLiteTileManager();
	~SQLiteTileManager();

	virtual QImage *loadMapTile(const Tile *mytile);
	virtual int saveMapTile(QImage *img, const Tile *mytile);

private:
	bool CreateDatabase(QString path);
	bool LoadDatabase(QString path);

	sqlite3 *_db;
};

#endif // SQLITETILEMANAGER_H
