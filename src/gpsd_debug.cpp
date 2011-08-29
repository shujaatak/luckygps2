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

#include "gpsd.h"

#include <QIODevice>
#include <QList>

#define MAX_SECONDS 20

gpsd::gpsd(QObject *parent, gps_settings_t &settings) : QObject(parent)
{
	_route = NULL;
	_routeIndex = 0;
	_deltaLat = 0;
	_deltaLon = 0;
	_count = 0;
	connect(&_readTimer, SIGNAL(timeout()), this, SLOT(callback_gpsd_read()));
}

bool gpsd::gps_connect()
{
	_gpsData.nice_connection = true;

	_readTimer.start(1000);

	return true;
}

void gpsd::gpsd_close()
{
	_gpsData.valid = false;
	_gpsData.seen_valid = false;
	_gpsData.nice_connection = false;
}

gpsd::~gpsd()
{
	gpsd_close();
}

bool gpsd::gpsd_update_settings(gps_settings_t &settings)
{
	return gps_connect();
}

void gpsd::callback_gpsd_read()
{
	_gpsData.latitude = 0;
	_gpsData.longitude = 0;

	if(_route && _route->points.length() > _routeIndex + 2)
	{

		if(_count == MAX_SECONDS)
		{
			_deltaLat = 0;
			_deltaLon = 0;
			_count = 0;
			_routeIndex++;
		}

		if(_deltaLat == 0)
		{
			_deltaLat = (_route->points[_routeIndex + 1].latitude - _route->points[_routeIndex].latitude) / MAX_SECONDS;
		}
		if(_deltaLon == 0)
		{
			_deltaLon = (_route->points[_routeIndex + 1].longitude - _route->points[_routeIndex].longitude) / MAX_SECONDS;
		}

		_gpsData.latitude = _route->points[_routeIndex].latitude + _count * _deltaLat;
		_gpsData.longitude = _route->points[_routeIndex].longitude + _count * _deltaLon;

		_count++;
	}

	_gpsData.fix = 3;
	_gpsData.seen_valid = true;
	_gpsData.valid = true;

	_readTimer.start(1000);

	emit gpsd_read();
}

