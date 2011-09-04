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

#ifndef OSMADRESSMANAGER_H
#define OSMADRESSMANAGER_H

#include <QString>

#include "interfaces/iaddresslookup.h"
#include "interfaces/igpslookup.h"
#include "interfaces/irouter.h"

#include "sqlite3.h"

class HouseNumber {
public:
	HouseNumber(QString hn, double lat, double lon) : housenumber(hn), latitude(lat), longitude(lon), valid(1) {}
	HouseNumber(double lat, double lon) : latitude(lat), longitude(lon), valid(1) {}
	HouseNumber() : housenumber(""), valid(0) {}

	QString housenumber;
	double latitude;
	double longitude;
	bool valid;
};


class InterpolationWay {
public:
	InterpolationWay() {}

	QString housenumber[2];
	QString street;
	QString city;
	QString country;
	int postcode;
	double latitude[2];
	double longitude[2];
	unsigned osm_id[2];
	QString interpolation;
};

class osmAdressManager
{
public:
	osmAdressManager();

	bool Preprocess(QString dataDir);
	static bool getHousenumbers(QString street, HouseNumber &hn, IAddressLookup *addressLookupPlugins, size_t placeID);

private:
	size_t interpolateHousenumber(sqlite3 *db, sqlite3_stmt *stmt, InterpolationWay &iWay);

};

#endif // OSMADRESSMANAGER_H
