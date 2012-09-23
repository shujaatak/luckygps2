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

#include <QCoreApplication>
#include <QDesktopServices>

#include "system_helpers.h"


/* only works for Ubuntu for now, other systems untested */
#if defined(Q_OS_LINUX)
QString getDataHome(int local /* not supported for Linux yet */)
{
    QDir dir;
    QString xdgDataHome = QLatin1String(qgetenv("XDG_DATA_HOME"));
    if (xdgDataHome.isEmpty())
        xdgDataHome = QDir::homePath() + QLatin1String("/.local/share");

    /* add application name */
    xdgDataHome += "/luckygps";

    if(!dir.exists(xdgDataHome))
        dir.mkpath(xdgDataHome);

    if(!dir.exists(xdgDataHome))
    {
        QString message = "Error: Cannot create data home. Please check permision to write in " + xdgDataHome;
        printf("%s\n", message.toAscii().constData());
    }

    return xdgDataHome;
}
#endif

#if defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
QString getDataHome(int local)
{
	if(local)
		return QCoreApplication::applicationDirPath();

	QDir dir;
    QString datahome = QDesktopServices::storageLocation( QDesktopServices::DataLocation );

    if(!dir.exists(datahome))
        dir.mkpath(datahome);

    if(!dir.exists(datahome))
    {
        QString message = "Error: Cannot create data home. Please check permision to write in " + datahome;
        printf("%s\n", message.toAscii().constData());
    }

    return datahome;
}
#endif
