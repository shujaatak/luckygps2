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

#ifndef TRACK_H
#define TRACK_H

#include <float.h>

#include <QList>
#include <QString>

#include "nmea0183.h"


#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(a) (((a) < 0) ? -(a) : (a))

class TrackPoint
{
public:
	TrackPoint() { memset(&gpsInfo, 0, sizeof(nmeaGPS)); }
	TrackPoint(nmeaGPS &origData)
	{
		memcpy(&gpsInfo, &origData, sizeof(nmeaGPS));
		gpsInfo.speed *= 3.6; /* convert unit to km/h */
	}
    
	nmeaGPS gpsInfo;
};

class TrackSegment
{
public:
	TrackSegment(int i) : id(i) { bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX; }
	TrackSegment() : id(-1) { bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX; }

	void insert_point(TrackPoint point)
	{
		bb[0] = MIN(bb[0], point.gpsInfo.longitude); /* min x */
		bb[1] = MIN(bb[1], point.gpsInfo.latitude); /* min y */
		bb[2] = MAX(bb[2], point.gpsInfo.longitude); /* max x */
		bb[3] = MAX(bb[3], point.gpsInfo.latitude); /* max y */

		points.append(point);
	}

	int id;		/* database ID, -1 = not saved yet */
	double bb[4];	/* bounding box of segment */

	QList<TrackPoint> points;
};

class Track
{
public:
	Track() : id(-1), active(0) { bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX; }
	Track(int i, QString n, bool a) : id(i), name(n), active(a) { bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX; }
	Track(int i, QString n, TrackSegment segment, bool a) : id(i), name(n), active(a) { bb[0] = bb[1] = DBL_MAX; bb[2] = bb[3] = -DBL_MAX; segments.append(segment); }

	void insert_point(TrackPoint point)
	{
		bb[0] = MIN(bb[0], point.gpsInfo.longitude); /* min x */
		bb[1] = MIN(bb[1], point.gpsInfo.latitude); /* min y */
		bb[2] = MAX(bb[2], point.gpsInfo.longitude); /* max x */
		bb[3] = MAX(bb[3], point.gpsInfo.latitude); /* max y */

		segments.last().insert_point(point);
	}

	void updateBB()
	{
		for(int i = 0; i < segments.length(); i++)
		{
			bb[0] = MIN(bb[0], segments[i].bb[0]); /* min x */
			bb[1] = MIN(bb[1], segments[i].bb[1]); /* min y */
			bb[2] = MAX(bb[2], segments[i].bb[2]); /* max x */
			bb[3] = MAX(bb[3], segments[i].bb[3]); /* max y */
		}
	}

	int id;							/* database ID, -1 = not saved yet */
	QString name;					/* track name */
	QString filename;
	QList<TrackSegment> segments;	/* track segments (see GPX format) */

    bool active;
	double bb[4];			/* bounding box of track */
};

#endif // TRACK_H
