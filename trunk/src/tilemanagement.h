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

#ifndef TILEMANAGEMENT_H
#define TILEMANAGEMENT_H

#include <QImage>
#include <QPixmap>
#include <QString>

#include "convertunits.h"
#include "tile.h"

#include "datasource.h"

#include <cmath>

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif /* !M_LN2 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif /* !M_PI */

class fileTileMgr : public DataSource
{

	Q_OBJECT

public:
	fileTileMgr(QObject *parent = 0) : DataSource(parent) {};

	virtual QImage *loadMapTile(const Tile *mytile);
	virtual int saveMapTile(QImage *img, const Tile *mytile);

private:
	QString get_tilename(int x, int y, int zoom, QString path);

signals:

public slots:

};

QImage *fill_tiles_pixel(TileList *requested_tiles, TileList *missing_tiles, TileList *cache, int nx, int ny);
TileList *get_necessary_tiles(int pixel_x, int pixel_y, int zoom, int width, int height, QString path, TileInfo &info);
QString get_scale(double lat, double lon, double lon2, int *width, int units /* metrical or imperial */, int length);

#endif // TILEMANAGEMENT_H
