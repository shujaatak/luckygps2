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
#include "filetilemanager.h"
#include "system_helpers.h" /* TODO: support a local instalation here */


#ifdef WITH_MAPNIK

MapnikThread::MapnikThread(int numThread, QObject *parent) : QObject(parent)
{
	_init = 1;

	/* Unique thread number */
	_numThread = numThread;

	QString mapnikDir(getDataHome() + "/mapnik_files");

	/* Register fonts */
	QDir fontDir = mapnikDir + "/dejavu-fonts-ttf-2.33/ttf/";
	QFileInfoList fileList = fontDir.entryInfoList(QStringList("*.ttf"), QDir::Files);
	if(fileList.length() > 0)
	{
		for(int i = 0; i < fileList.length(); i++)
		{
			freetype_engine::register_font(fileList[i].filePath().toStdString());
			qDebug() << "Register font: " << fileList[i].filePath();
		}
	}
	else
	{
		qDebug() << "Cannot find any mapnik fonts.";
		_init = 0;
	}

	/* Register plugins */
	datasource_cache::instance()->register_datasources(mapnikDir.toStdString());

	for(unsigned i = 0; i < datasource_cache::instance()->plugin_names().size(); i++)
		qDebug() << "Found Mapnik plugin: " << QString::fromStdString(datasource_cache::instance()->plugin_names().at(i));

	if(datasource_cache::instance()->plugin_names().size() <= 0)
		_init = 0;

	/* ------------------------------- */
	/*    Adjust xml style template    */
	/* ------------------------------- */

	/* Load xml style template */
	QFile file(mapnikDir + "/luckygps-default.template");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		_init = 0;
		return;
	}
	QTextStream in(&file);
	in.setCodec("UTF-8");
	in.setAutoDetectUnicode(true);
	QString xml = in.readAll();
	file.close();

	/* Replace path variables in template */
	// later use %1 with .arg()?

	QString symbols_path(mapnikDir + "/symbols/");
	QString symbolsString("%(symbols)s");
	xml.replace(symbolsString, symbols_path);

	QString srs("&srs4326;");
	QString srsString("%(osm2pgsql_projection)s");
	xml.replace(srsString, srs);

	QString wb_path(mapnikDir + "/world_boundaries/");
	QString wbString("%(world_boundaries)s");
	xml.replace(wbString, wb_path);

	QString prefix("world");
	QString prefixString("%(prefix)s");
	int index = xml.indexOf(prefixString);
	xml.replace(index, prefixString.length(), prefix);
	xml.replace(prefixString, prefix);


	QString dbSettings("<Parameter name=\"type\">spatialite</Parameter>\n\
					   <Parameter name=\"file\">/home/daniel/of.sqlite</Parameter>\n\
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
	_proj = new projection("+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +no_defs +over");

	if(!_proj)
	{
		/* Shouldn't happen */
		qDebug() << "No valid projection given.";
		_init = 0;
	}
}

MapnikThread::~MapnikThread()
{
	if(_map)
		delete _map;
	if(_proj)
		delete _proj;
}

void MapnikThread::renderTile(void *mytile)
{
	if(!_init)
		return;

	Tile *newtile = new Tile(*((Tile *)mytile), NULL);
	delete (Tile *)mytile;

	/* Calculate pixel positions of bottom-left & top-right */
	int p0_x = newtile->_x * TILE_SIZE;
	int p0_y = (newtile->_y + 1) * TILE_SIZE;
	int p1_x = (newtile->_x + 1) * TILE_SIZE;
	int p1_y = newtile->_y * TILE_SIZE;

	/* convert pixel to lat/lon degree */
	double lon_0 = rad_to_deg(pixel_to_longitude(newtile->_z, p0_x));
	double lat_0 = rad_to_deg(pixel_to_latitude(newtile->_z, p0_y));
	double lon_1 = rad_to_deg(pixel_to_longitude(newtile->_z, p1_x));
	double lat_1 = rad_to_deg(pixel_to_latitude(newtile->_z, p1_y));

	_proj->forward(lon_0, lat_0);
	_proj->forward(lon_1, lat_1);

	/* calculate bounding box for the tile */
	box2d<double> box = box2d<double>(lon_0, lat_0, lon_1, lat_1);
	_map->zoom_to_box(box);

	/* Render image with default Agg renderer */
	image_32 img = image_32(TILE_SIZE, TILE_SIZE);
	agg_renderer<image_32> render(*_map, img);
	render.apply();

	/* TODO: speed this up! */
	QImage tmpImg = QImage((uchar*)img.raw_data(), _map->width(), _map->height(), QImage::Format_ARGB32);
	// Need to switch r and b channel
	QImage *image = new QImage(tmpImg.rgbSwapped());

	emit finished(_numThread, image, newtile);
}

MapnikSource::MapnikSource(DataSource *ds, QObject *parent) : DataSource(parent)
{
	_ds = ds;

	for(int i = 0; i < NUM_THREADS; i++)
	{
		_isRendering[i] = false;

		mapnikThread[i] = new MapnikThread(i);
		mapnikThread[i]->moveToThread(&thread[i]);

		// connect thread signals
		connect(mapnikThread[i], SIGNAL(finished(int, QImage *, Tile *)), this, SLOT(save(int, QImage *, Tile *)));
		connect(&thread[i], SIGNAL(terminated()), mapnikThread[i], SLOT(deleteLater()));
		thread[i].start();
	}
}

MapnikSource::~MapnikSource()
{
	for(int i = 0; i < NUM_THREADS; i++)
	{
		if(thread[i].isRunning())
		{
			thread[i].quit();
			thread[i].wait();
		}
	}
}

QImage *MapnikSource::loadMapTile(const Tile *mytile)
{
	Tile *newtile = new Tile(*mytile, NULL);
	delete mytile;

	bool assigned = false;

	for(int i = 0; i < NUM_THREADS; i++)
		if(!_isRendering[i])
		{
			_isRendering[i] = true;
			QMetaObject::invokeMethod( mapnikThread[i], "renderTile", Q_ARG( void *, newtile ) );

			assigned = true;

			break;
		}

	if(!assigned)
		_dlTilesTodo.append(newtile);

	return NULL;
}

void MapnikSource::save(int numThread, QImage *img, Tile *tile)
{
	_ds->saveMapTile(img, tile);
	delete img;
	delete tile;

	_isRendering[numThread] = false;

	if(!_dlTilesTodo.empty())
		loadMapTile(_dlTilesTodo.takeLast());
}

#endif
