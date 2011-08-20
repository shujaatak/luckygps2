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

#ifndef ROUTE_H
#define ROUTE_H

#include "convertunits.h"

#include "interfaces/irouter.h"

#if defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
#include <climits>
#else
#include <float.h>
#endif

#include <QList>
#include <QString>
#include <QStringList>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(a) (((a) < 0) ? -(a) : (a))


class RoutePoint
{
public:
    RoutePoint(): latitude(0), longitude(0), nextDesc(-1) {}
    RoutePoint(double lat, double lon)
		: latitude(lat), longitude(lon), nextDesc(-1), enterLink(0), exitLink(0), exitNumber(0), direction(0)
    {
        latitude_rad = deg_to_rad(lat);
        longitude_rad = deg_to_rad(lon);
    }

    RoutePoint(double lat, double lon, QString d)
		: latitude(lat), longitude(lon), desc(d), nextDesc(-1)
    {
        latitude_rad = deg_to_rad(lat);
        longitude_rad = deg_to_rad(lon);
    }

	double latitude;		/* Latitude in degrees	*/
	double longitude;		/* Longitude in degrees */

	double latitude_rad;    /* Latitude in rad		*/
	double longitude_rad;   /* Longitude in rad		*/

	QString desc;		/* description							*/
	int		nextDesc;	/* index to point with next description */

	/* Data to create an updated Description */
	int exitNumber;
	QString lastType;
	QString name;
	QString nextName;
	int direction;
	double length; /* length of edge to next description point */
	double edgeLength;
	bool enterLink, exitLink; /* used for traffic circle entering/exiting, TODO: also for motorway links? */
};

class Route
{
public:
    Route(): pos(0) { bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX; }
    Route(QString n) : name(n), pos(0) { bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX; }

    void insert_point(RoutePoint point)
    {
		bb[0] = MIN(bb[0], point.longitude);	/* min x */
		bb[1] = MIN(bb[1], point.latitude);		/* min y */
		bb[2] = MAX(bb[2], point.longitude);	/* max x */
		bb[3] = MAX(bb[3], point.latitude);		/* max y */

        points.append(point);
    }

    void init()
    {
        points.clear();
		pathNodes.clear();
		pathEdges.clear();

        name = "";
        pos = 0;

		bb[0] = bb[1] = DBL_MAX;
		bb[2] = bb[3] = -DBL_MAX;
    }

	int getCurrentPosOnRoute(const double lat, const double lon)
	{
		if(pos == points.length() - 1)
			return pos;

		double dist = DBL_MAX;
		int start = pos - 5;
		start = start < 0 ? 0 : start;
		int index = start;
		for(int i = start; i < points.length() - 1; i++)
		{
			const double v[2] = {points[i].latitude, points[i].longitude};
			const double w[2] = {points[i + 1].latitude, points[i + 1].longitude};
			const double gps[2] = {lat, lon};
			const double tDist = minimum_distance(v, w, gps);

			/* don't check further points if > 50km away */
			if(tDist > 50.0)
			{
				break;
			}

			if(tDist < dist)
			{
				dist = tDist;
				index = i + 1; // TODO: is this correct? Better use i?
			}
		}
		pos = index;
		return index;
	}

	QString				name;	/* route name	*/
	QList<RoutePoint>	points; /* route points */

	QVector< IRouter::Node > pathNodes;
	QVector< IRouter::Edge > pathEdges;

	int		pos;			/* current position/point on route	*/
	double	dist;			/* distance until next turn			*/
	double	bb[4];			/* bounding box of segment			*/
	int		zoom;
};

#endif // ROUTE_H
