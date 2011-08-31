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

#include "osmadressmanager.h"

/*
select "addr:city", "addr:street", "addr:postcode","addr:housenumber" as addr_housenumber
from world_point
where "addr:housenumber" is not null and "addr:street" is not null
and world_point.rowid in (SELECT pkid FROM 'idx_world_point_way' WHERE xmin >= -113.6 AND xmax <= -113.55 AND ymin >= 53.25 AND ymax <= 53.3)
*/

/*
select osm_id, "addr:interpolation"
FROM world_line
WHERE "addr:interpolation" is not null
*/

// CANNOT use map sqlite: for interpolations, node id's are not saved
// TODO: import data from temp file and insert into sqlite file, create spatial index


#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QStringList>

#include "sqlite3.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(a) (((a) < 0) ? -(a) : (a))


osmAdressManager::osmAdressManager()
{
}


bool osmAdressManager::Preprocess(QString dataDir)
{
	QDataStream hnData;
	QFile hnFile( "/tmp/OSM Importer_hn" ); /* house numbers filename + "_hn"  */
	if ( !hnFile.open(QIODevice::ReadOnly) )
		return false;
	hnData.setDevice(&hnFile);

	sqlite3 *_db;
	QString filename = "/home/daniel/hn.sqlite";
	if(sqlite3_open(filename.toUtf8().constData(), &_db) != SQLITE_OK)
	{
		qDebug() << "Cannot open " << filename.toUtf8().constData();

		if(_db)
		{
			sqlite3_close(_db);
			_db = NULL;
		}

		return false;
	}
	else
	{
		/* Create housenumber NODES table */
		sqlite3_exec(_db, "DROP TABLE hn;", 0, NULL, NULL);
		if(sqlite3_exec(_db, "CREATE TABLE hn (osm_id INTEGER PRIMARY KEY NOT NULL, lat DOUBLE NOT NULL, lon DOUBLE NOT NULL, housenumber TEXT NOT NULL, street TEXT, postcode INTEGER, city TEXT, country TEXT)", 0, NULL, NULL) != SQLITE_OK)
		{
			return false;
		}

		/* prepare insertion query */
		sqlite3_stmt *stmt = NULL;
		QString sql = "INSERT OR IGNORE INTO hn VALUES(?, ?, ?, ?, ?, ?, ?, ?);";

		sqlite3_exec (_db, "BEGIN;", NULL, NULL, NULL);

		if(sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL) != SQLITE_OK)
		{
			sql = "Could not prepare statement: " + sql;
			qDebug(sql.toUtf8().constData());
			sqlite3_finalize(stmt);
			return false;
		}

		/* Loop over housenumber nodes */
		int count = 0;
		while ( true ) {
			unsigned osm_id;
			int postcode;
			double lat, lon;
			QString housenumber, streetname, city, country;

			hnData >> osm_id >> lat >> lon >> housenumber >> streetname >> postcode >> city >> country;

			if ( hnData.status() == QDataStream::ReadPastEnd )
				break;

			sqlite3_bind_int(stmt, 1, osm_id);
			sqlite3_bind_double(stmt, 2, lat);
			sqlite3_bind_double(stmt, 3, lon);
			sqlite3_bind_text (stmt, 4, housenumber.toUtf8().constData(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text (stmt, 5, streetname.toUtf8().constData(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_double(stmt, 6, postcode);
			sqlite3_bind_text (stmt, 7, city.toUtf8().constData(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text (stmt, 8, country.toUtf8().constData(), -1, SQLITE_TRANSIENT);

			if(sqlite3_step(stmt) != SQLITE_DONE)
			{
				qDebug("Could not step (execute) stmt.");
				sqlite3_finalize(stmt);
				return false;
			}

			sqlite3_reset(stmt);

			count++;
		}

		sqlite3_finalize(stmt);
		sqlite3_exec (_db, "COMMIT;", NULL, NULL, NULL);

		qDebug() << "Imported adress nodes: " << count;

		/* Create RTree index */
		sqlite3_exec(_db, "CREATE VIRTUAL TABLE hn_idx USING RTREE(osm_id, minLat, maxLat, minLon, maxLon);", 0, NULL, NULL);
		sqlite3_exec(_db, "INSERT INTO hn_idx SELECT osm_id, lat, lat, lon, lon FROM hn;", 0, NULL, NULL);

		// TODO: add interpolations
		// TODO: add housenumbers from buildings/areas
		// TODO: add missing streets

		sqlite3_exec(_db, "ANALYZE; COMPACT;", 0, NULL, NULL);
		sqlite3_close(_db);
	}
	return true;
}

bool osmAdressManager::getHousenumbers(QString streetname, QStringList hnList, IAddressLookup *addressLookupPlugins, size_t placeID)
{
	sqlite3 *_db;
	QString filename = "/home/daniel/hn.sqlite";
	if(sqlite3_open(filename.toUtf8().constData(), &_db) != SQLITE_OK)
	{
		qDebug() << "Cannot open " << filename.toUtf8().constData();

		if(_db)
		{
			sqlite3_close(_db);
			_db = NULL;
		}

		return false;
	}

	// get boundary of street ???? ---> TODO: put street ID into db in preprocess???

	/* Get street osm id for the street name */
	QVector< int > segmentLength;
	QVector< UnsignedCoordinate > coordinates;
	if (!addressLookupPlugins->GetStreetData(placeID, streetname, &segmentLength, &coordinates))
		return false;

	if (coordinates.size() == 0)
		return false;

	GPSCoordinate gpsMin = coordinates.first().ToGPSCoordinate();
	GPSCoordinate gpsMax = gpsMin;

	for(unsigned int i = 1; i < coordinates.size(); i++)
	{
		GPSCoordinate gpsTmp = coordinates[i].ToGPSCoordinate();

		/* Update minimum */
		gpsMin.latitude = MIN(gpsMin.latitude, gpsTmp.latitude);
		gpsMin.longitude = MIN(gpsMin.longitude, gpsTmp.longitude);

		/* Update maximum */
		gpsMin.latitude = MAX(gpsMin.latitude, gpsTmp.latitude);
		gpsMin.longitude = MAX(gpsMin.longitude, gpsTmp.longitude);

	}

	// SELECT housenumber FROM hn where street=street

	sqlite3_close(_db);
	return true;
}




