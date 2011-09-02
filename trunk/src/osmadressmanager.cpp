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

#ifdef __GNUC__
#include <ext/hash_map>
#else
#include <hash_map>
#endif

namespace std
{
 using namespace __gnu_cxx;
}


typedef std::hash_map<int, GPSCoordinate>::value_type hashGPS;

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(a) (((a) < 0) ? -(a) : (a))


osmAdressManager::osmAdressManager()
{
}


bool osmAdressManager::Preprocess(QString dataDir)
{
	std::hash_map<unsigned, GPSCoordinate> hm1;

	QDataStream hnData;
	QFile hnFile( "/tmp/OSM Importer_hn" ); /* house numbers filename + "_hn"  */
	if (!hnFile.open(QIODevice::ReadOnly))
		return false;
	hnData.setDevice(&hnFile);

	/* Node coordinates from */
	QDataStream hnCoordsData;
	QFile hnCoordsFile( "/tmp/OSM Importer_adress_coordinates" );
	if (!hnCoordsFile.open(QIODevice::ReadOnly))
		return false;
	hnCoordsData.setDevice(&hnCoordsFile);

	/*  nodes for a building tag of a way */
	QDataStream hnWayData;
	QFile hnWayFile( "/tmp/OSM Importer_hn_ways" );
	if (!hnWayFile.open(QIODevice::ReadOnly))
		return false;
	hnWayData.setDevice(&hnWayFile);

	sqlite3 *_db;
	QString filename = "/home/daniel/hn.sqlite";
	QFile::remove(filename);
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

		/* cache needed nodes */
		while(true)
		{
			unsigned node = 0;
			unsigned tlat = 0, tlon = 0;
			hnCoordsData >> node >> tlat >> tlon;

			if ( hnCoordsData.status() == QDataStream::ReadPastEnd )
				break;

			UnsignedCoordinate coord(tlat, tlon);

			hm1[node] = coord.ToGPSCoordinate();

			// qDebug() << "Fetching node: " << node << hm1[node].latitude << hm1[node].longitude;
		}

		/* Loop over housenumber nodes */
		int count = 0;
		while (true) {
			unsigned osm_id;
			int postcode;
			double lat, lon;
			QString housenumber, streetname, city, country;

			hnData >> osm_id >> lat >> lon >> housenumber >> streetname >> postcode >> city >> country;

			if (hnData.status() == QDataStream::ReadPastEnd)
				break;

			if(osm_id == 0)
			{
				double minLat = DBL_MAX, minLon = DBL_MAX, maxLat = -DBL_MAX, maxLon = -DBL_MAX;
				unsigned size = 0;
				hnWayData >> size;

				if (hnWayData.status() == QDataStream::ReadPastEnd)
				{
					qDebug() << "Adress import error: Not enough size information for buildings.";
					break;
				}
				// qDebug() << "Nodes for this building: " << size;

				for(unsigned i = 0; i < size; i++)
				{
					unsigned tmpNode;
					hnWayData >> tmpNode;

					if (hnWayData.status() == QDataStream::ReadPastEnd)
					{
						qDebug() << "Adress import error 2: Not enough size information for buildings.";
						break;
					}

					// qDebug() << "Fetching node: " << tmpNode << hm1[tmpNode].latitude << hm1[tmpNode].longitude;

					minLat = MIN(minLat, hm1[tmpNode].latitude);
					minLon = MIN(minLon, hm1[tmpNode].longitude);

					maxLat = MAX(maxLat, hm1[tmpNode].latitude);
					maxLon = MAX(maxLon, hm1[tmpNode].longitude);
				}
				lat = (minLat + maxLat) * 0.5;
				lon = (minLon + maxLon) * 0.5;

				// This was needed for some bad OSM data
				// if(streetname[0] == '<')
				//	qDebug() << "Found building data: " << streetname << housenumber << lat << lon;
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
		if(sqlite3_exec (_db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK)
		{
			qDebug() << "Error: Cannot commit to database.";
		}

		/* Interpolation ways */

		/*  nodes for a interpolation tag of a way */
		QDataStream hnWayInterData;
		QFile hnWayInterFile( "/tmp/OSM Importer_hn_ways_inter" );
		if (!hnWayInterFile.open(QIODevice::ReadOnly))
			return false;
		hnWayInterData.setDevice(&hnWayInterFile);

		sqlite3_exec (_db, "BEGIN;", NULL, NULL, NULL);
		if(sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL) != SQLITE_OK)
		{
			sql = "Could not prepare statement: " + sql;
			qDebug(sql.toUtf8().constData());
			sqlite3_finalize(stmt);
			return false;
		}

		sqlite3_stmt *stmtQuery = NULL;
		while(true)
		{
			QString interpolation;
			unsigned nodeA = 0, nodeB = 0;
			int postcode = 0;
			QString streetname, city, country;
			int housenumberA = 0, housenumberB = 0;
			bool ok = 0;

			hnWayInterData >> interpolation >> nodeA >> nodeB;

			if (hnWayInterData.status() == QDataStream::ReadPastEnd)
				break;

			QString query;
			QString sql2 = "SELECT housenumber, street, postcode, city, country FROM hn WHERE lat=? AND lon=? ";
			sql2 += "AND hn.rowid IN (SELECT rowid FROM hn_idx WHERE minLat>=%1 AND maxLat<=%2 AND minLon>=%3 AND maxLon<=%4) LIMIT 1;";
			query = sql2.arg(hm1[nodeA].latitude - 0.001).arg(hm1[nodeA].latitude + 0.001).arg(hm1[nodeA].longitude - 0.001).arg(hm1[nodeA].longitude + 0.001);
			// qDebug() << sql2;
			if(sqlite3_prepare_v2(_db, sql2.toUtf8().constData(), -1, &stmtQuery, NULL) == SQLITE_OK)
			{
				sqlite3_bind_double(stmtQuery, 1, hm1[nodeA].latitude);
				sqlite3_bind_double(stmtQuery, 2, hm1[nodeA].longitude);

				if(sqlite3_step(stmtQuery) == SQLITE_ROW)
				{
					QString housenumber = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 0)),sqlite3_column_bytes(stmtQuery, 0) / sizeof(char));
					streetname = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 1)),sqlite3_column_bytes(stmtQuery, 1) / sizeof(char));
					postcode = sqlite3_column_int(stmtQuery, 2);
					city = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 3)),sqlite3_column_bytes(stmtQuery, 3) / sizeof(char));
					country = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 4)),sqlite3_column_bytes(stmtQuery, 4) / sizeof(char));

					housenumberA = housenumber.toInt(&ok);
				}
				sqlite3_finalize(stmt);
			}

			if(!ok)
				continue;

			query = sql2.arg(hm1[nodeB].latitude - 0.001).arg(hm1[nodeB].latitude + 0.001).arg(hm1[nodeB].longitude - 0.001).arg(hm1[nodeB].longitude + 0.001);
			if(sqlite3_prepare_v2(_db, sql2.toUtf8().constData(), -1, &stmtQuery, NULL) == SQLITE_OK)
			{
				sqlite3_bind_double(stmtQuery, 1, hm1[nodeB].latitude);
				sqlite3_bind_double(stmtQuery, 2, hm1[nodeB].longitude);

				if(sqlite3_step(stmtQuery) == SQLITE_ROW)
				{
					QString housenumber = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 0)),sqlite3_column_bytes(stmtQuery, 0) / sizeof(char));
					/*
					street = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 1)),sqlite3_column_bytes(stmtQuery, 1) / sizeof(char));
					postcode = sqlite3_column_int(stmtQuery, 2);
					city = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 3)),sqlite3_column_bytes(stmtQuery, 3) / sizeof(char));
					country = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmtQuery, 4)),sqlite3_column_bytes(stmtQuery, 4) / sizeof(char));
					*/

					housenumberB = housenumber.toInt(&ok);
				}
				sqlite3_finalize(stmt);
			}

			if(!ok)
				continue;

			// all numbers
			int countNumber = (housenumberB - housenumberA - 1);
			int increment = 1;

			if(interpolation != "all")
			{
				countNumber /= 2;
				increment = 2;
			}

			/* put every housenumber into database */
			for(int i = increment; i <= countNumber; i += increment)
			{
				/* interpolate gps coordinates */
				double latitude = 0;
				double longitude = 0;
				QString housenumber = QString::number(housenumberA + i);

				sqlite3_bind_double(stmt, 1, latitude);
				sqlite3_bind_double(stmt, 2, longitude);
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

				qDebug() << latitude << longitude << housenumber << streetname << postcode << city << country;

				sqlite3_reset(stmt);

				count++;
			}

		}

		sqlite3_finalize(stmt);
		if(sqlite3_exec (_db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK)
		{
			qDebug() << "Error: Cannot commit to database.";
		}


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

	for(int i = 1; i < coordinates.size(); i++)
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

	// qDebug() << sql;

	if(sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL) == SQLITE_OK)
	{
		sqlite3_bind_text (stmt, 1, streetname.toUtf8().constData(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text (stmt, 2, hn.housenumber.toUtf8().constData(), -1, SQLITE_TRANSIENT);

		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
			double latitude = sqlite3_column_double(stmt, 0);
			double longitude = sqlite3_column_double(stmt, 1);

			hn = HouseNumber(latitude, longitude);
		}
		sqlite3_finalize(stmt);
	}

	sqlite3_close(_db);
	return true;
}




