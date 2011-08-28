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

#ifndef MAPNIKTHREAD_H
#define MAPNIKTHREAD_H

#include <QThread>

#include "datasource.h"


#ifdef WITH_MAPNIK

/* Mapnik include */
#include <mapnik/agg_renderer.hpp>
#include <mapnik/coord.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/graphics.hpp>
#include <mapnik/image_data.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/map.hpp>
#include <mapnik/projection.hpp>

using namespace mapnik;

#define NUM_THREADS 2

class MapnikThread : public QObject
{
	Q_OBJECT

public:
	MapnikThread(int numThread, QObject *parent = 0);
	~MapnikThread();

private:
	Map *_map;
	projection *_proj;
	int _numThread;

private slots:
	void renderTile(void *mytile);
signals:
	void finished(int numThread, QImage *img, Tile *tile);
};

class MapnikSource : public DataSource
{
    Q_OBJECT

public:
	MapnikSource(DataSource *ds, QObject *parent = 0);
	~MapnikSource();

	virtual QImage *loadMapTile(const Tile *mytile);

	/* Tiles for render */
	TileListP _dlTilesTodo;

private:
	QThread thread[NUM_THREADS];
	MapnikThread *mapnikThread[NUM_THREADS];

	/* data sink: Where to write the downloaded images */
	DataSource *_ds;

	bool _isRendering[NUM_THREADS];


private slots:
	void save(int numThread, QImage *img, Tile *tile);
signals:
	void renderTile(Tile *);
};

#else
class MapnikSource : public DataSource
{
	Q_OBJECT

public:
	MapnikThread() {};
	virtual QImage *loadMapTile(const Tile *mytile) { return NULL; }
};
#endif // #ifdef WITH_MAPNIK

#endif // MAPNIKTHREAD_H
