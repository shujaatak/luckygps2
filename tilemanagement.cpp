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

#include <QFile>
#include <QHttp>
#include <QIODevice>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>

#include "tilemanagement.h"

#include <stdio.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(a) (((a) < 0) ? -(a) : (a))


QString get_tilename(int x, int y, int z, QString path)
{
    return path.arg(z).arg(x).arg("/").arg(y).arg(".png");
}

QImage *get_map(Tile mytile)
{
    /* load tile or download it */
    QImage *img = new QImage();
    QString filename = get_tilename(mytile._x, mytile._y, mytile._z, mytile._path);

    if(img)
        img->load(filename);

    if(img && !img->isNull())
        return img;
    else
        return NULL;
}

/* This function is part of the MapWidget to be able to access plugins */
QImage *MapWidget::fill_tiles_pixel(TileList *requested_tiles, TileList *missing_tiles, TileList *cache, int nx, int ny)
{
	QImage *paintImg = NULL;
    QPainter painter;
    int i;

    /* init painting on a pixmap/image */
	paintImg = new QImage(nx * TILE_SIZE, ny * TILE_SIZE, QImage::Format_RGB32);
	if(!paintImg)
        return NULL;
	painter.begin(paintImg);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.fillRect(0,0, nx * TILE_SIZE, ny * TILE_SIZE, Qt::white);

    for (i = 0; i < requested_tiles->length(); i++)
    {
        int cache_index = -1;
        if((cache_index = cache->indexOf(requested_tiles->at(i))) < 0)
        {
			QImage *img = NULL;
			if(_tileManager)
				img = _tileManager->RequestTile(requested_tiles->at(i)._x, requested_tiles->at(i)._y, requested_tiles->at(i)._z);
			else
				img = get_map(requested_tiles->at(i));

            if(img)
            {
                cache->append(Tile(requested_tiles->at(i), img));
                painter.drawImage(requested_tiles->at(i)._posx, requested_tiles->at(i)._posy, *img);
            }
            else
            {
				missing_tiles->append(requested_tiles->at(i));
            }
        }
        else
        {
            painter.drawImage(requested_tiles->at(i)._posx, requested_tiles->at(i)._posy, *(cache->at(cache_index)._img));
        }

    }
    painter.end();

	return paintImg;
}

TileList *get_necessary_tiles(int pixel_x, int pixel_y, int zoom, int width, int height, QString path, TileInfo &info)
{
	int i, j;
	int start_tile_x, start_tile_y;
	int num_tiles_x, num_tiles_y;
	int pos_x, pos_y;
	TileList *mytilelist = new TileList();

	/* calc sub-tile offset to get smooth map scrolling */
	info.offset_tile_x = -(pixel_x % TILE_SIZE);
	info.offset_tile_y = -(pixel_y % TILE_SIZE);

	/* calc the number of tiles needed to draw */
	info.nx = num_tiles_x = ((width - info.offset_tile_x) / TILE_SIZE) + 1;
	info.ny = num_tiles_y = ((height - info.offset_tile_y) / TILE_SIZE) + 1;

	/* calc the start tile from the requested pixel values */
	start_tile_x = pixel_x / TILE_SIZE;
	start_tile_y = pixel_y / TILE_SIZE;

	/* use offsets as initial drawing position for the tiles */
	pos_x = 0; // *offset_tile_x;
	pos_y = 0; // *offset_tile_y;

	/* save requested tile rect for caching */
	info.min_tile_x = start_tile_x;
	info.min_tile_y = start_tile_y;
	info.max_tile_x = (start_tile_x + num_tiles_x) - 1;
	info.max_tile_y = (start_tile_y + num_tiles_y) - 1;

	for (i = start_tile_x; i < (start_tile_x + num_tiles_x); i++)
	{
		for (j = start_tile_y; j < (start_tile_y + num_tiles_y); j++)
		{
			if(j < 0 || i < 0 || i >= exp(zoom * M_LN2) || j >= exp(zoom * M_LN2))
			{
				// nothing to draw
			}
			else
			{
				mytilelist->append(Tile(i, j, zoom, path, pos_x, pos_y));
			}
			pos_y += TILE_SIZE;
		}
		pos_x += TILE_SIZE;
		pos_y = 0; // *offset_tile_y;
	}

	return mytilelist;
}

QString get_scale(double lat, double lon, double lon2, int *width, int units /* metrical or imperial */, int length)
{
	double scale_factor = 0;
	double distance = fast_distance_rad(lat, lon, lat, lon2);
	QString unit_string = distance_to_scale(distance, &scale_factor, units);

	*width = (length * scale_factor);

	return unit_string;
}
