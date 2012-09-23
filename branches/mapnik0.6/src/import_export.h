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

#ifndef IMPORT_EXPORT_H
#define IMPORT_EXPORT_H

#include "route.h"
#include "track.h"

#include <QString>


bool import_track(Track &track, QString filename);
QString export_gpx(Track track, QString path);

/* importing from gpx routes */
void import_route(Route &route, QString filename);

#endif // IMPORT_EXPORT_H
