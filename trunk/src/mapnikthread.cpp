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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "mapnikthread.h"
#include "tilemanagement.h"


#ifdef WITH_MAPNIK
MapnikThread::MapnikThread()
{
    QString mapnikDir("/home/daniel/mapnik_files");

    /* Register fonts */
    QDir fontDir = mapnikDir + "/dejavu-fonts-ttf-2.33/ttf/";
    QFileInfoList fileList = fontDir.entryInfoList(QStringList("*.ttf"), QDir::Files);
    if(fileList.length() > 0)
        for(int i = 0; i < fileList.length(); i++)
        {
            freetype_engine::register_font(fileList[i].filePath().toStdString());
            qDebug() << "Register font: " << fileList[i].filePath();
        }

    /* Register plugins */
    datasource_cache::instance()->register_datasources("/usr/lib/mapnik/0.7/input/");


    /* ------------------------------- */
    /*    Adjust xml style template    */
    /* ------------------------------- */

    /* Load xml style template */
    QFile file("/home/daniel/mapnik_files/luckygps-default.template");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    in.setAutoDetectUnicode(true);
    QString xml = in.readAll();
    file.close();

    /* Replace path variables in template */
    // later use %1 with .arg()?

    QString symbols_path("/home/daniel/mapnik_files/symbols/");
    QString symbolsString("%(symbols)s");
    size_t index = xml.indexOf(symbolsString);
    xml.replace(index, symbolsString.length(), symbols_path);

    QString srs("&srs4326");
    QString srsString("%(osm2pgsql_projection)s");
    index = xml.indexOf(srsString);
    xml.replace(index, srsString.length(), srs);

    QString wb_path("/home/daniel/mapnik_files/world_boundaries/");
    QString wbString("%(world_boundaries)s");
    index = xml.indexOf(wbString);
    xml.replace(index, wbString.length(), wb_path);

    QString prefix("world");
    QString prefixString("%(prefix)s");
    index = xml.indexOf(prefixString);
    xml.replace(index, prefixString.length(), prefix);
    xml.replace(prefixString, prefix);


    QString dbSettings("<Parameter name=\"type\">sqlite</Parameter>\n\
                       <Parameter name=\"file\">/home/daniel/alberta.sqlite</Parameter>\n\
                       <Parameter name=\"key_field\">rowid</Parameter>\n\
                       <Parameter name=\"geometry_field\">way</Parameter>\n\
                       <Parameter name=\"wkb_format\">spatialite</Parameter>\n\
                       <Parameter name=\"estimate_extent\">false</Parameter>\n\
                       <Parameter name=\"extent\">180.0,89.0,-180.0,-89.0</Parameter>\n\
                       <Parameter name=\"use_spatial_index\">true</Parameter>");
    QString dbSettingsString("%(datasource_settings)s");
    xml.replace(dbSettingsString, dbSettings);

    /* ------------------------------- */


    /* Create Mapnik map */
    _map = new Map(256,256);

    /* load map settings */
    load_map_string(*_map, xml.toStdString());

    /* Copied from osm2pgsql for better label placement */
    _map->set_buffer_size(128);

    /* Google srid=900913 projection used for rendering */
    _proj = projection("+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +no_defs +over");
}

MapnikThread::~MapnikThread()
{
    if(_map)
        delete _map;
}

void MapnikThread::createTile(int x, int y, int zoom)
{
    /* Calculate pixel positions of bottom-left & top-right */
    int p0_x = x * TILE_SIZE;
    int p0_y = (y + 1) * TILE_SIZE;
    int p1_x = (x + 1) * TILE_SIZE;
    int p1_y = y * TILE_SIZE;

    /* convert pixel to lat/lon degree */
    double lon_0 = rad_to_deg(pixel_to_longitude(zoom, p0_x));
    double lat_0 = rad_to_deg(pixel_to_latitude(zoom, p0_y));
    double lon_1 = rad_to_deg(pixel_to_longitude(zoom, p1_x));
    double lat_1 = rad_to_deg(pixel_to_latitude(zoom, p1_y));

    _proj.forward(lon_0, lat_0);
    _proj.forward(lon_1, lat_1);

    /* calculate bounding box for the tile */
    Envelope<double> box = Envelope<double>(lon_0, lat_0, lon_1, lat_1);
    _map->zoomToBox(box);

    /* Render image with default Agg renderer */
    Image32 img = Image32(TILE_SIZE, TILE_SIZE);
    agg_renderer<Image32> render(*_map, img);
    render.apply();

    save_to_file(img.data(), "/home/daniel/test.png", "png");
}

#else
MapnikThread::MapnikThread()
{
}
#endif
