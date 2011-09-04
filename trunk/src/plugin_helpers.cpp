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
#include <QFileInfo>
#include <QPluginLoader>
#include <QString>

#include "osmadressmanager.h"
#include "plugin_helpers.h"
#include "system_helpers.h"
#include "interfaces/iimporter.h"
#include "interfaces/ipreprocessor.h"


/* not sure for what I still need "settingsFile" -- TODO dg */
bool importOsmPbf(QCoreApplication *app, char *file, QString settingsFile, int local)
{
	IImporter *importer = NULL;
	IPreprocessor *routerPlugins = NULL;
	IPreprocessor *gpsLookupPlugins = NULL;
	IPreprocessor *addressLookupPlugins = NULL;

	foreach(QObject *plugin, QPluginLoader::staticInstances())
	{
		if(IImporter *interface = qobject_cast< IImporter* >(plugin))
		{
			qDebug() << "Preprocessor found plugin:" << interface->GetName();

			if (interface->GetName() == "OpenStreetMap Importer" )
			{
				importer = interface;
			}
		}
		else if(IPreprocessor *interface = qobject_cast< IPreprocessor* >( plugin ))
		{
			qDebug() << "Preprocessor found plugin:" << interface->GetName();

			if (interface->GetType() == IPreprocessor::Router )
			{
				routerPlugins =  interface;
			}
			if (interface->GetType() == IPreprocessor::GPSLookup)
			{
				gpsLookupPlugins = interface;
			}
			if(interface->GetType() == IPreprocessor::AddressLookup)
			{
				addressLookupPlugins = interface;
			}
		}

	}

	if(!importer || !routerPlugins || !gpsLookupPlugins || !addressLookupPlugins)
	{
		qDebug() << "Couldn't find all necessary plugins to complete import.";
		return false;
	}

	QFileInfo filename(file);

	if(filename.completeSuffix() != "osm.pbf")
	{
		qDebug() << app->tr("Only accepting *.osm.pbf as input files.");
		return false;
	}
	else if(!filename.exists())
	{
		qDebug() << app->tr("Cannot find %1.").arg(filename.filePath());
		return false;
	}

	QFileInfo settingsFilename(settingsFile);

	if(!settingsFilename.exists())
	{
		qDebug() << app->tr("Cannot find %1.").arg(settingsFilename.filePath());
		return false;
	}

	/* Create data folder */
	QDir dir;
	QString dataDir = getDataHome(local) + "/" + filename.baseName() + "/" + settingsFilename.baseName();
	dir.mkpath(dataDir);

	// qDebug() << "Temporary folder: " << QDir::tempPath();

	/* The order of doing things here is important! */

	/* OSM PBF importer */
	importer->SetOutputDirectory(QDir::tempPath());
	bool result = importer->Preprocess(filename.filePath());

	/* Route plugin */
	result = routerPlugins->Preprocess(importer, dataDir);

	/* GPS Lookup */
	result = gpsLookupPlugins->Preprocess(importer, dataDir);

	/* Address lookup */
	result = addressLookupPlugins->Preprocess( importer, dataDir);

	/* House number lookup */
	osmAdressManager *hn = new osmAdressManager();
	hn->SetOutputDirectory(QDir::tempPath());
	result = hn->Preprocess(dataDir);
	delete hn;

	importer->DeleteTemporaryFiles();

	/* TODO: download mapnik world_boundaries here? */
	/* TODO: create mapnik sqlite file here? */

	return result;
}


