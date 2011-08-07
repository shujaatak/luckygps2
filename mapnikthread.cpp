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

#include <QFile>

#include "mapnikthread.h"
#include "tilemanagement.h"


MapnikThread::MapnikThread()
{
}

#ifdef WITH_MAPNIK
MapnikThread::MapnikThread(Map *map)
{
    _map = new Map(*map);
    _tile = NULL;
}

MapnikThread::~MapnikThread()
{
    delete _map;
}

void MapnikThread::run()
{
    int x, y, zoom;
    QString path;
    QString filename;

    if(!_tile)
        return;

    x = _tile->_x;
    y = _tile->_y;
    zoom = _tile->_z;
    path = _tile->_path;

    delete _tile;
    _tile = NULL;

    filename = get_tilename(x, y, zoom, path);

    if(!QFile::exists(filename))
    {
        QFile file(filename);
        file.open(QIODevice::WriteOnly);
        file.close();

		qDebug("%s\n", filename.toAscii());

        /* Mapnik direct rendering using postgres/postgis db */
        projection proj(_map->srs());

        /* Calculate pixel positions of bottom-left & top-right */
        int p0_x = x * TILE_SIZE;
        int p0_y = (y + 1) * TILE_SIZE;
        int p1_x = (x + 1) * TILE_SIZE;
        int p1_y = y * TILE_SIZE;

        /* convert pixel to lat/lon degree */
        double lon_0 = rad_to_deg(pixel_to_lon(zoom, p0_x));
        double lat_0 = rad_to_deg(pixel_to_lat(zoom, p0_y));
        double lon_1 = rad_to_deg(pixel_to_lon(zoom, p1_x));
        double lat_1 = rad_to_deg(pixel_to_lat(zoom, p1_y));

        proj.forward(lon_0, lat_0);
        proj.forward(lon_1, lat_1);

        /* calculate bounding box for the tile */
        Envelope<double> box = Envelope<double>::Envelope(lon_0, lat_0, lon_1, lat_1);
        _map->zoomToBox(box);

        /* Render image with default Agg renderer */
        Image32 img = Image32::Image32(TILE_SIZE, TILE_SIZE);
        agg_renderer<Image32> render(*_map, img);

        render.apply();

        file.remove();
        save_to_file(img.data(), filename.toStdString(), "png");
    }
}

#endif
