/*
 * This file is part of the luckyGPS project.
 *
 * Copyright (C) 2009 - 2010 Daniel Genrich <dg@luckygps.com>
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

#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include "mainwindow.h"

#include "plugin_helpers.h"
#include "system_helpers.h"

#include <Magick++.h>

Q_IMPORT_PLUGIN(contractionhierarchiesclient);
Q_IMPORT_PLUGIN(gpsgridclient);
Q_IMPORT_PLUGIN(unicodetournamenttrieclient);
Q_IMPORT_PLUGIN(mbtilesmanagerclient);

Q_IMPORT_PLUGIN(osmimporter);
Q_IMPORT_PLUGIN(contractionhierarchies);
Q_IMPORT_PLUGIN(gpsgrid);
Q_IMPORT_PLUGIN(unicodetournamenttrie);


int main(int argc, char *argv[])
{
	Magick::InitializeMagick(*argv);

	QCoreApplication *a = (argc == 1) ? new QApplication(argc, argv) : new QCoreApplication(argc, argv);

	if(a)
	{
		/* we need nice getDataHome */
		#if defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
		QCoreApplication::setApplicationName("luckygps");
		QCoreApplication::setOrganizationName(".");
		#endif

		QLocale locale = QLocale::system();
		QString locale_name = locale.name();
		QTranslator translator;

		/* Detect if luckyGPS is executed in "local mode" (no installation) */
		int local = 0;
		QDir dir(QCoreApplication::applicationDirPath());
		QFileInfoList fileList = dir.entryInfoList(QStringList("luckygps_*.qm"), QDir::AllEntries | QDir::NoDot | QDir::NoDotDot);
		if(fileList.length() > 0)
			local = 1;

#if defined(Q_OS_LINUX)
		translator.load(QString("luckygps_") + locale_name, "/usr/share/luckygps");
#else
		translator.load(QString("luckygps_") + locale_name, getDataHome(local));
#endif

		a->installTranslator(&translator);
		setlocale(LC_NUMERIC, "C");

		if(argc == 1)
		{
			MainWindow w(0, local);
			w.show();

			a->exec();
		}
		else
		{
			/* check if 1 argument is given */
			if(argc > 3)
			{
				printf("\nUsage: luckygps FILE.osm.pbf [SETTINGS.spp]\n");
			}
			else
			{
				QString settingsFile;
				if(argc == 2)
					settingsFile = ":/data/motorcar.spp";
				else
					settingsFile = QString(argv[2]);

				/* import osm file into routing database */
				if(importOsmPbf(a, argv[1], settingsFile, local))
					qWarning() << "Import successfully.";
				else
					qWarning() << "Import failed.";
			}
		}

		delete a;
	}

	return true;
}
