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

#include "convertunits.h"
#include "sqlite3.h"

#if defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
#include <climits>
#else
#include <float.h>
#endif

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

	/* Node coordinates from */
	QDataStream hnCoordsData;
	QFile hnCoordsFile( "/tmp/OSM Importer_adress_coordinates" );
	if ( !hnCoordsFile.open(QIODevice::ReadOnly) )
		return false;
	hnCoordsData.setDevice(&hnCoordsFile);

	/*  number of nodes for a building tag of a way */
	QDataStream hnWayData;
	QFile hnWayFile( "/tmp/OSM Importer_hn_ways" );
	if ( !hnWayFile.open(QIODevice::ReadOnly) )
		return false;
	hnWayData.setDevice(&hnWayFile);

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
		if(sqlite3_exec(_db, "CREATE TABLE hn (lat DOUBLE NOT NULL, lon DOUBLE NOT NULL, housenumber TEXT NOT NULL, street TEXT, postcode INTEGER, city TEXT, country TEXT)", 0, NULL, NULL) != SQLITE_OK)
		{
			return false;
		}

		/* prepare insertion query */
		sqlite3_stmt *stmt = NULL;
		QString sql = "INSERT OR IGNORE INTO hn VALUES(?, ?, ?, ?, ?, ?, ?);";

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

			if(osm_id == -1)
			{
				double minLat = DBL_MAX, minLon = DBL_MAX, maxLat = -DBL_MAX, maxLon = -DBL_MAX;
				unsigned size = 0;
				hnWayData >> size;

				if ( hnWayData.status() == QDataStream::ReadPastEnd )
				{
					qDebug() << "Adress import error: Not enough size information for buildings.";
					break;
				}

				for(unsigned i = 0; i < size; i++)
				{
					double tlat, tlon;
					hnCoordsData >> tlat >> tlon;

					if ( hnCoordsData.status() == QDataStream::ReadPastEnd )
					{
						// something bad happened
						qDebug() << "Adress import error: Not enough coordinates for buildings.";
						break;
					}

					minLat = MIN(minLat, tlat);
					minLon = MIN(minLon, tlon);

					maxLat = MAX(maxLat, tlat);
					maxLon = MAX(maxLon, tlon);
				}
				lat = (minLat + maxLat) * 0.5;
				lon = (minLon + maxLon) * 0.5;
			}

			sqlite3_bind_double(stmt, 1, lat);
			sqlite3_bind_double(stmt, 2, lon);
			sqlite3_bind_text (stmt, 3, housenumber.toUtf8().constData(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text (stmt, 4, streetname.toUtf8().constData(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_double(stmt, 5, postcode);
			sqlite3_bind_text (stmt, 6, city.toUtf8().constData(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text (stmt, 7, country.toUtf8().constData(), -1, SQLITE_TRANSIENT);

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
		sqlite3_exec(_db, "CREATE VIRTUAL TABLE hn_idx USING RTREE(rowid, minLat, maxLat, minLon, maxLon);", 0, NULL, NULL);
		sqlite3_exec(_db, "INSERT INTO hn_idx SELECT rowid, lat, lat, lon, lon FROM hn;", 0, NULL, NULL);

		// TODO: add interpolations
		// TODO: add housenumbers from buildings/areas
		// TODO: add missing streets

		sqlite3_exec(_db, "ANALYZE; COMPACT;", 0, NULL, NULL);
		sqlite3_close(_db);
	}

	hnCoordsFile.close();
	QFile::remove("/tmp/OSM Importer_adress_coordinates");

	return true;
}

bool osmAdressManager::getHousenumbers(QString streetname, HouseNumber &hn, IAddressLookup *addressLookupPlugins, size_t placeID)
{
	sqlite3 *_db;
	sqlite3_stmt *stmt = NULL;
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
		gpsMax.latitude = MAX(gpsMax.latitude, gpsTmp.latitude);
		gpsMax.longitude = MAX(gpsMax.longitude, gpsTmp.longitude);

	}

	double correctionLat = (111132.954 - 559.822*cos(2.0 * deg_to_rad(gpsMin.latitude)) + 1.175*cos(4.0 * deg_to_rad(gpsMin.latitude)) -0.0023 * cos(6 * deg_to_rad(gpsMin.latitude)) ) / 111132.954;
	double correctionLon = (111412.84 * cos(deg_to_rad(gpsMin.latitude*correctionLat)) -93.5 * cos(3 * deg_to_rad(gpsMin.latitude*correctionLat)) + 0.118 * cos(5.0 * deg_to_rad(gpsMin.latitude*correctionLat))) / 111412.84;

	/* Search in 200 meters radius of street for house numbers */
	gpsMin.latitude -= 0.001 * correctionLat; /* ~110 meters */
	gpsMin.longitude -= 0.001 * correctionLon; /* ~110 meters, too */
	gpsMax.latitude += 0.001 * correctionLat;
	gpsMax.longitude += 0.001 * correctionLon;

	QString sql = "SELECT lat, lon FROM hn WHERE street=? AND housenumber=? ";
	sql += "AND hn.rowid IN (SELECT rowid FROM hn_idx WHERE minLat>=%1 AND maxLat<=%2 AND minLon>=%3 AND maxLon<=%4);";
	sql = sql.arg(gpsMin.latitude).arg(gpsMax.latitude).arg(gpsMin.longitude).arg(gpsMax.longitude);

	qDebug() << sql;

	if(sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL) == SQLITE_OK)
	{
		sqlite3_bind_text (stmt, 1, streetname.toUtf8().constData(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text (stmt, 2, hn.housenumber.toUtf8().constData(), -1, SQLITE_TRANSIENT);

		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
			// QString hnString = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),sqlite3_column_bytes(stmt, 0) / sizeof(char));
			double latitude = sqlite3_column_double(stmt, 0);
			double longitude = sqlite3_column_double(stmt, 1);

			hn = HouseNumber(latitude, longitude);
		}
		sqlite3_finalize(stmt);
	}

	sqlite3_close(_db);
	return true;
}




