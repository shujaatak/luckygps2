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

#include "tile.h"

/* Mapnik include */
#ifdef WITH_MAPNIK
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
#endif


class MapnikThread : public QThread
{
public:
    MapnikThread();

#ifdef WITH_MAPNIK
public:
    MapnikThread(Map *map);
    ~MapnikThread();

    void run();

    Tile *_tile;
private:
    Map *_map;
#endif
};

#endif // MAPNIKTHREAD_H
