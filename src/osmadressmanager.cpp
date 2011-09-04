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
	QList <InterpolationWay> iWay;
	QDataStream hnData;
	QDataStream hnCoordsData;
	QDataStream hnWayData;
	QDataStream hnWayInterData;
	sqlite3 *_db = NULL;
	QString filename;
	QString sql;
	sqlite3_stmt *stmt = NULL;
	size_t count = 0;

	QFile hnFile( _dataDir + "/OSM Importer_hn" ); /* house numbers filename + "_hn"  */
	if (!hnFile.open(QIODevice::ReadOnly))
		return false;
	hnData.setDevice(&hnFile);

	/* Node coordinates from */
	QFile hnCoordsFile( _dataDir + "/OSM Importer_adress_coordinates" );
	if (!hnCoordsFile.open(QIODevice::ReadOnly))
		return false;
	hnCoordsData.setDevice(&hnCoordsFile);

	/*  nodes for a building tag of a way */
	QFile hnWayFile( _dataDir + "/OSM Importer_hn_ways" );
	if (!hnWayFile.open(QIODevice::ReadOnly))
		return false;
	hnWayData.setDevice(&hnWayFile);

	filename = dataDir + "/hn.sqlite";
	QFile::remove(filename);
	if(sqlite3_open(filename.toUtf8().constData(), &_db) != SQLITE_OK)
	{
		qDebug() << "Cannot open " << filename.toUtf8().constData();
		if(_db)
			sqlite3_close(_db);
		return false;
	}

	/* Create housenumber NODES table */
	if(sqlite3_exec(_db, "CREATE TABLE hn (osm_id INTEGER UNIQUE, lat DOUBLE NOT NULL, lon DOUBLE NOT NULL, housenumber TEXT NOT NULL, street TEXT, postcode INTEGER, city TEXT, country TEXT)", 0, NULL, NULL) != SQLITE_OK)
	{
		return false;
	}

	/* prepare insertion query */
	sql = "INSERT OR IGNORE INTO hn VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
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
		unsigned node = 0, tlat = 0, tlon = 0;

		hnCoordsData >> node >> tlat >> tlon;

		if ( hnCoordsData.status() == QDataStream::ReadPastEnd )
			break;

		UnsignedCoordinate coord(tlat, tlon);
		hm1[node] = coord.ToGPSCoordinate();
	}

	/* Loop over housenumber nodes */
	while (true) {
		unsigned osm_id;
		int postcode;
		double lat, lon;
		QString housenumber, street, city, country;

		hnData >> osm_id >> lat >> lon >> housenumber >> street >> postcode >> city >> country;

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

			for(unsigned i = 0; i < size; i++)
			{
				unsigned tmpNode;

				hnWayData >> tmpNode;

				if (hnWayData.status() == QDataStream::ReadPastEnd)
				{
					qDebug() << "Adress import error 2: Not enough size information for buildings.";
					break;
				}

				minLat = MIN(minLat, hm1[tmpNode].latitude);
				minLon = MIN(minLon, hm1[tmpNode].longitude);

				maxLat = MAX(maxLat, hm1[tmpNode].latitude);
				maxLon = MAX(maxLon, hm1[tmpNode].longitude);
			}
			lat = (minLat + maxLat) * 0.5;
			lon = (minLon + maxLon) * 0.5;

			sqlite3_bind_null(stmt, 1);
		}
		else
			sqlite3_bind_int(stmt, 1, osm_id);

		sqlite3_bind_double(stmt, 2, lat);
		sqlite3_bind_double(stmt, 3, lon);
		sqlite3_bind_text (stmt, 4, housenumber.toUtf8().constData(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text (stmt, 5, street.toUtf8().constData(), -1, SQLITE_TRANSIENT);
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
	if(sqlite3_exec (_db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK)
	{
		qDebug() << "Error: Cannot commit to database.";
	}

	/* Interpolation ways */

	/*  nodes for a interpolation tag of a way */
	QFile hnWayInterFile( _dataDir + "/OSM Importer_hn_ways_inter" );
	if (!hnWayInterFile.open(QIODevice::ReadOnly))
		return false;
	hnWayInterData.setDevice(&hnWayInterFile);

	/* Read ndoes from DB */
	sql = "SELECT housenumber, street, postcode, city, country, lat, lon FROM hn WHERE osm_id=? OR osm_id=? LIMIT 2;";
	sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL);

	while(true)
	{
		iWay.append(InterpolationWay());

		hnWayInterData >> iWay.back().osm_id[0] >> iWay.back().osm_id[1] >> iWay.back().interpolation;

		if (hnWayInterData.status() == QDataStream::ReadPastEnd)
		{
			iWay.removeLast();
			break;
		}

		sqlite3_bind_int(stmt, 1, iWay.back().osm_id[0]);
		sqlite3_bind_int(stmt, 2, iWay.back().osm_id[1]);

		int rowCount = 0;
		while(sqlite3_step(stmt) == SQLITE_ROW)
		{
			iWay.back().housenumber[rowCount] = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),sqlite3_column_bytes(stmt, 0) / sizeof(char));

			if(!rowCount)
			{
				iWay.back().street = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)),sqlite3_column_bytes(stmt, 1) / sizeof(char));
				iWay.back().postcode = sqlite3_column_int(stmt, 2);
				iWay.back().city = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)),sqlite3_column_bytes(stmt, 3) / sizeof(char));
				iWay.back().country = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)),sqlite3_column_bytes(stmt, 4) / sizeof(char));
			}
			iWay.back().latitude[rowCount] = sqlite3_column_double(stmt, 5);
			iWay.back().longitude[rowCount] = sqlite3_column_double(stmt, 6);

			rowCount++;
		}

		if(iWay.back().housenumber[0].isEmpty() || iWay.back().housenumber[1].isEmpty())
			iWay.removeLast();
		//else
		//	qDebug() << iWay.back().osm_id[0] << iWay.back().osm_id[1];


		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);

	/* ------------------------------------------------ */
	/* Interpolate housenumbers and insert them into db */
	/* ------------------------------------------------ */
	sql = "INSERT OR IGNORE INTO hn VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_exec (_db, "BEGIN;", NULL, NULL, NULL);
	sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &stmt, NULL);

	for(size_t i = 0; i < iWay.size(); i++)
		count += interpolateHousenumber(_db, stmt, iWay[i]);

	sqlite3_finalize(stmt);
	if(sqlite3_exec (_db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK)
		qDebug() << "interpolateHousenumber Error: Cannot commit to database.";
	/* ------------------------------------------------ */

	qDebug() << "Imported adress nodes: " << count;

	/* Create RTree index */
	sqlite3_exec(_db, "CREATE VIRTUAL TABLE hn_idx USING RTREE(rowid, minLat, maxLat, minLon, maxLon);", 0, NULL, NULL);
	sqlite3_exec(_db, "INSERT INTO hn_idx SELECT rowid, lat, lat, lon, lon FROM hn;", 0, NULL, NULL);

	sqlite3_exec(_db, "ANALYZE; COMPACT;", 0, NULL, NULL);
	sqlite3_close(_db);

	hnCoordsFile.close();
	QFile::remove(_dataDir + "/OSM Importer_adress_coordinates");

	return true;
}

size_t osmAdressManager::interpolateHousenumber(sqlite3 *db, sqlite3_stmt *stmt, InterpolationWay &iWay)
{
	bool ok = false;
	int housenumber[2];
	int countNumber; /* Total number count to insert into DB */
	int increment = 1;
	double deltaLat, deltaLon;
	size_t count = 0;
	int iA = 0, iB = 1; /* order of housenumber */

	if(iWay.interpolation == "alphabetic")
		return 0; /* not supoprted yet */
	else if(iWay.interpolation != "even" && iWay.interpolation != "odd")
		return 0; /* not supoprted yet */

	/* Only support numbers as housenumbers */
	housenumber[iA] = iWay.housenumber[iA].toInt(&ok);
	if(!ok) return 0;
	housenumber[iB] = iWay.housenumber[iB].toInt(&ok);
	if(!ok) return 0;

	if(housenumber[iB] < housenumber[iA])
	{
		iA = 1; iB = 0;
	}

	/* Count all needed numbers for interpolation */
	countNumber = (housenumber[iB] - housenumber[iA] - 1);

	/* Compute increment of latitude and longitude along the street interpolation */
	deltaLat = (iWay.latitude[iB] - iWay.latitude[iA]) / countNumber;
	deltaLon = (iWay.longitude[iB] - iWay.longitude[iA]) / countNumber;

	/* Only half the numbers are needed in case of odd/even */
	if(iWay.interpolation != "all")
	{
		countNumber /= 2;
		increment = 2;
	}

	if(!countNumber)
		return 0;

	/* Put every housenumber into database */
	for(int i = 1; i <= countNumber; i++)
	{
		/* interpolate gps coordinates */
		double latitude = iWay.latitude[iA] + deltaLat * i * increment;
		double longitude = iWay.longitude[iA] + deltaLon * i * increment;
		QString hn = QString::number(housenumber[iA] + i * increment);

		sqlite3_bind_null(stmt, 1);
		sqlite3_bind_double(stmt, 2, latitude);
		sqlite3_bind_double(stmt, 3, longitude);
		sqlite3_bind_text (stmt, 4, hn.toUtf8().constData(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text (stmt, 5, iWay.street.toUtf8().constData(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_double(stmt, 6, iWay.postcode);
		sqlite3_bind_text (stmt, 7, iWay.city.toUtf8().constData(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text (stmt, 8, iWay.country.toUtf8().constData(), -1, SQLITE_TRANSIENT);

		if(sqlite3_step(stmt) != SQLITE_DONE)
		{
			qDebug("Could not step (execute) stmt.");
			sqlite3_finalize(stmt);
			return count;
		}
		sqlite3_reset(stmt);

		count++;
	}
	return count;
}

bool osmAdressManager::getHousenumbers(QString street, HouseNumber &hn, IAddressLookup *addressLookupPlugins, size_t placeID)
{
	sqlite3 *_db;
	sqlite3_stmt *stmt = NULL;
	QString filename = _dataDir + "/hn.sqlite";

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
	if (!addressLookupPlugins->GetStreetData(placeID, street, &segmentLength, &coordinates))
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
		sqlite3_bind_text (stmt, 1, street.toUtf8().constData(), -1, SQLITE_TRANSIENT);
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




