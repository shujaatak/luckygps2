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

#include <QTime>

#include "routing.h"


#include <QFile>
#include <QTextStream>

#include "osmadressmanager.h"


Routing::Routing()
{
	_gpsLookup = NULL,
	_router = NULL;
	_addressLookup = NULL;
	_descGenerator = new DescriptionGenerator();

	_pos[0][0] = _pos[0][1] = 0.0;
	_pos[1][0] = _pos[1][1] = 0.0;

	foreach (QObject *plugin, QPluginLoader::staticInstances())
	{
		if (IGPSLookup *interface = qobject_cast< IGPSLookup* >(plugin))
		{
			qDebug() << "found plugin:" << interface->GetName();
			if (interface->GetName() == "GPS Grid" )
				_gpsLookup = interface;
		}
		if (IRouter *interface = qobject_cast< IRouter* >(plugin))
		{
			qDebug() << "found plugin:" << interface->GetName();
			if (interface->GetName() == "Contraction Hierarchies")
				_router = interface;
		}
		if (IAddressLookup *interface = qobject_cast< IAddressLookup* >(plugin))
		{
			qDebug() << "found plugin:" << interface->GetName();
			if (interface->GetName() == "Unicode Tournament Trie")
			{
				_addressLookup = interface;
			}
		}
	}
}

Routing::~Routing()
{
	/* Free static plugins */
	foreach (QObject *plugin, QPluginLoader::staticInstances())
		delete plugin;

	/* Description Generator */
	if(_descGenerator)
		delete _descGenerator;
}

int Routing::dataFolder(QString dir, QString *result)
{
	int ret = 0;

	if((ret = loadPlugins(dir)) >= 0)
	{
		*result = dir;
		return ret;
	}

	QDir tdir(dir);
	QFileInfoList fileList = tdir.entryInfoList(QStringList("*.*"), QDir::AllDirs  | QDir::NoDot | QDir::NoDotDot);
	for(int i = 0; i < fileList.length(); i++)
	{
		ret = dataFolder(fileList[i].filePath(), result);

		if(ret >= 0)
			return ret;
	}

	return ret;
}

bool Routing::init(QString dataDirectory)
{
	if(!_addressLookup || !_router || !_gpsLookup || !_descGenerator)
	{
		_init = false;
		return false;
	}

	/* check if plugin files are in this folder otherwise browse into subfolder */
	QString folder;

	int ret = dataFolder(dataDirectory, &folder);

	if(ret < 1)
		_init = false;
	else
		_init = true;

	return ret;
}

int Routing::loadPlugins(QString dataDirectory)
{
	/* ---------------------------------- */
	/*         GPS lookup plugin          */
	/* ---------------------------------- */
	if (_gpsLookup == NULL)
	{
		qCritical() << "GPSLookup plugin not found.";
		return false;
	}
	int gpsLookupFileFormat = 1; // pluginSettings.value( "gpsLookupFileFormatVersion" ).toInt();
	if (!_gpsLookup->IsCompatible( gpsLookupFileFormat))
	{
		qCritical() << "GPS Lookup file format not compatible";
		return false;
	}
	_gpsLookup->SetInputDirectory( dataDirectory );
	if (!_gpsLookup->LoadData())
	{
		qDebug() << "could not load GPSLookup data";
		return -4;
	}
	/* ---------------------------------- */


	/* ---------------------------------- */
	/*            Route plugin            */
	/* ---------------------------------- */
	if (_router == NULL)
	{
		qCritical() << "router plugin not found.";
		return false;
	}
	int routerFileFormat = 1; // pluginSettings.value( "routerFileFormatVersion" ).toInt();
	if (!_router->IsCompatible(routerFileFormat))
	{
		qCritical() << "Router file format not compatible";
		return false;
	}
	_router->SetInputDirectory(dataDirectory);
	if (!_router->LoadData())
	{
		qDebug() << "could not load router data";
		return -8;
	}
	/* ---------------------------------- */


	/* ---------------------------------- */
	/*       Start Address plugin         */
	/* ---------------------------------- */
	if (_addressLookup == NULL)
	{
		qCritical() << "Start Address plugin not found.";
		return false;
	}
	int addressLookupFileFormat = 1; // pluginSettings.value( "addressLookupFileFormatVersion" ).toInt();
	if (!_addressLookup->IsCompatible( addressLookupFileFormat))
	{
		qCritical() << "Address Lookup file format not compatible";
		return false;
	}
	_addressLookup->SetInputDirectory(dataDirectory);
	if (!_addressLookup->LoadData())
	{
		qDebug() << "could not load address data";
		return -16;
	}
	/* ---------------------------------- */

	qDebug("loadPlugins: successfull.");

	return true;
}

bool Routing::calculateRoute(Route &route, int units, QString hnStart, QString hnDest)
{
	if(!_init)
		return false;

	/* HACK */
	// Check if house number is given, too */
	if(!hnStart.isEmpty())
	{
		HouseNumber hn;
		hn.housenumber = hnStart;
		bool ret = osmAdressManager::getHousenumbers(_startStreet, hn, _addressLookup, _startPlaceID);

		if(ret && hn.valid)
		{
			_pos[0][0] = hn.latitude;
			_pos[0][1] = hn.longitude;

			qDebug() << "Found house number! (start)";
		}
	}

	if(!hnDest.isEmpty())
	{
		HouseNumber hn;
		hn.housenumber = hnDest;
		bool ret = osmAdressManager::getHousenumbers(_destStreet, hn, _addressLookup, _destPlaceID);

		if(ret && hn.valid)
		{
			_pos[1][0] = hn.latitude;
			_pos[1][1] = hn.longitude;

			qDebug() << "Found house number! (dest)";
		}
	}


	double lookupRadius = 10000; // 10km should suffice for most applications
	GPSCoordinate source(_pos[0][0], _pos[0][1]);
	GPSCoordinate target(_pos[1][0], _pos[1][1]);
	double segmentDistance;
	bool success = true;

	/* prepare new route */
	route.init();

	// TODO: travel time variable
	if(!computeRoute(&segmentDistance, &(route.pathNodes), &(route.pathEdges), source, target, lookupRadius))
	{
		success = false;
	}

	/* construct navigation instructions */
	// QStringList labels;
	QStringList icons;
	getInstructions(route, &icons, units);

	{
		QFile file("/home/daniel/test.txt");
		if (file.open(QIODevice::WriteOnly))
		{
			QTextStream stream(&file);

			for(int i = 0; i < route.points.length(); i++)
			{
				const RoutePoint &point = route.points.at(i);

				stream << "Point " << i << endl;
				stream << "-------------" << endl;
				stream << "Lat/lon: " << point.latitude << ", " << point.longitude << endl;
				stream << "Name: " << point.name << endl;
				stream << "NextName: " << point.nextName << endl;
				stream << "NextDesc: " << point.nextDesc << endl;
				stream << "Desc: " << point.desc << endl;
				stream << "Direction: " << point.direction << endl;
				stream << "exitNumber: " << point.exitNumber << endl;
				stream << "exitLink: " << point.exitLink << endl;
				stream << "enterLink: " << point.enterLink << endl;
				stream << "lastType: " << point.lastType << endl;
				stream << "type: " << point.type << endl;
				stream << "length: " << point.length << endl << endl;

			}

			file.close();
		}
	 }

	return success;
}

bool Routing::computeRoute(double *resultDistance, QVector< IRouter::Node > *resultNodes, QVector< IRouter::Edge > *resultEdge, GPSCoordinate source, GPSCoordinate target, double lookupRadius)
{
	if(!_init)
		return false;

	UnsignedCoordinate sourceCoordinate(source);
	UnsignedCoordinate targetCoordinate(target);
	IGPSLookup::Result sourcePosition;
	QTime time;

	time.start();

	bool found = _gpsLookup->GetNearestEdge(&sourcePosition, sourceCoordinate, lookupRadius);
	qDebug() << "GPS Lookup:" << time.restart() << "ms";
	if ( !found )
	{
		qDebug() << "no edge near source found";
		return false;
	}

	IGPSLookup::Result targetPosition;
	found = _gpsLookup->GetNearestEdge(&targetPosition, targetCoordinate, lookupRadius);
	qDebug() << "GPS Lookup:" << time.restart() << "ms";
	if ( !found )
	{
		qDebug() << "no edge near target found";
		return false;
	}

	found = _router->GetRoute(resultDistance, resultNodes, resultEdge, sourcePosition, targetPosition);
	qDebug() << "Routing:" << time.restart() << "ms";

	return found;
}

bool Routing::getCitySuggestions(QString city, QStringList &suggestions)
{
	if(!_init)
		return false;

	QStringList characters;
	QTime time;
	IAddressLookup *addressLookup = _addressLookup;

	time.start();

	bool found = addressLookup->GetPlaceSuggestions(city, 10, &suggestions, &characters);
	qDebug() << "City Lookup:" << time.elapsed() << "ms";

	if (!found)
		return false;
	else
		return true;
}

bool Routing::getStreetSuggestions(QString street, QStringList &suggestions, int typeID)
{
	if(!_init)
		return false;

	QStringList characters;
	QTime time;
	IAddressLookup *addressLookup = _addressLookup;
	int m_placeID;

	time.start();

	if(typeID == ID_START_STREET)
		m_placeID = _startPlaceID;
	else
		m_placeID = _destPlaceID;

	bool found = addressLookup->GetStreetSuggestions(m_placeID, street, 10, &suggestions, &characters);
	qDebug() << "Street Lookup:" << time.elapsed() << "ms";

	if (!found)
		return false;
	else
		return true;
}

bool Routing::suggestionClicked(QString text, int typeID)
{
	if(!_init)
		return false;

	IAddressLookup *addressLookup = _addressLookup;

	if(typeID == ID_START_CITY || typeID == ID_DEST_CITY)
	{
		QVector< int > placeIDs;
		QVector< UnsignedCoordinate > placeCoordinates;

		if (!addressLookup->GetPlaceData(text, &placeIDs, &placeCoordinates))
			return false;

		int m_placeID = placeIDs.front();
#if 0
		if (placeIDs.size() > 1)
		{
			/* TODO: cannot handle 2 cities with the same name yet */
			return false;
		}
#endif

		if(typeID == ID_START_CITY)
			_startPlaceID = m_placeID;
		else
			_destPlaceID = m_placeID;
	}
	else
	{
		QVector< int > segmentLength;
		QVector< UnsignedCoordinate > coordinates;
		int m_placeID;

		if(typeID == ID_START_STREET)
		{
			m_placeID = _startPlaceID;
			_startStreet = text;
		}
		else
		{
			m_placeID = _destPlaceID;
			_destStreet = text;
		}

		if (!addressLookup->GetStreetData(m_placeID, text, &segmentLength, &coordinates))
			return false;

		if (coordinates.size() == 0)
			return false;

		GPSCoordinate gps = coordinates.first().ToGPSCoordinate();
		setPos(gps.latitude, gps.longitude, typeID);

		// TODO set only streetID

		/* TODO: cannot handle house numbers or 2 streets with the same name */
		// return false;
	}

	return true;
}

/* does not only verify GPS coordinates but also inserts them into the _pos list */
bool Routing::gpsEntered(QString text, int typeID)
{
	double lon, lat;
	QStringList string_arrayS;

	resetPos(typeID);

	/* 2 gps coordinates should have at least 7 digits in total (2 * 6 + 1) */
	if(text.length() < 3)
		return false;

	text = text.remove(QChar(' '));
	string_arrayS = text.split(',');

	if(string_arrayS.length() == 2) // found direct latitude/longitude input
	{
		if(!string_arrayS[0].length() || !string_arrayS[1].length())
			return false;

		/* check for longitude/latitude */
		bool isDigit1, isDigit2;

		lat = string_arrayS[0].toDouble(&isDigit1);
		lon = string_arrayS[1].toDouble(&isDigit2);

		if(isDigit1 && isDigit2)
		{
			setPos(lat, lon, typeID);
			return true;
		}
	}

	return false;
}

void Routing::getInstructions(Route &route, QStringList* icons, int units)
{
	if(!_init)
		return;

	_descGenerator->descriptions(route, _router, icons, route.pathNodes, route.pathEdges, units); // maxSeconds
}

void Routing::getInstructions(RoutePoint *rp, RoutePoint *nextRp, double distance, QStringList* labels, QStringList* icons, int units)
{
	if(!_init)
		return;

	DescriptionGenerator::descInfo desc = {rp->name, rp->nextName, rp->direction, distance, icons, labels, rp->exitNumber, rp->lastType, units, nextRp ? 0 : 1, rp->exitLink, rp->enterLink, rp->type};
	_descGenerator->describe(desc);
}
