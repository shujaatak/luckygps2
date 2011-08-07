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

#ifndef TILE_H
#define TILE_H

#include <QImage>
#include <QNetworkReply>
#include <QString>
#include <QUrl>

class Tile
{
public:
    Tile(int x, int y, int z, QString path, QString url):_x(x), _y(y), _z(z), _path(path), _url(url)
    {
        _img = NULL;
    }
    Tile(int x, int y, int z, QString path, int posx, int posy):_x(x), _y(y), _z(z), _path(path), _posx(posx), _posy(posy)
    {
        _img = NULL;
    }
    Tile(int x, int y, int z, QString path, QString url, int posx, int posy):_x(x), _y(y), _z(z), _path(path), _url(url), _posx(posx), _posy(posy)
    {
        _img = NULL;
    }
    Tile(const Tile &other, QImage *img):_x(other._x), _y(other._y), _z(other._z), _path(other._path), _url(other._url), _posx(other._posx), _posy(other._posy)
    {
        _img = img;
    }

    bool operator==(const Tile &other) const
    {
        if( (_y != other._y) || (_x != other._x) || (_z != other._z) || (_path.compare(other._path) != 0))
            return false;
        else
            return true;
    }

    int _x;
    int _y;
    int _z;
    QString _path;
    QString _url;
	QNetworkReply *_reply; /* to identify the network reply for this tile */

    /* for MapWidget cache */
    int _posx, _posy;
    QImage *_img;
};
class TileList : public QList<Tile>{};
class TileListP : public QList<Tile*>{};

typedef struct TileInfo {
    int nx;
    int ny;
    int offset_tile_x;
    int offset_tile_y;
    int min_tile_x;
    int min_tile_y;
    int max_tile_x;
    int max_tile_y;
} TileInfo;

#endif // TILE_H
