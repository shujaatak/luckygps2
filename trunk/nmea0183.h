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

#ifndef NMEA0183_H
#define NMEA0183_H

#include <QString>


///////////////////////////////////////////////////////

typedef struct {
	double latitude;    /* Latitude in degrees */
	double longitude;   /* Longitude in degrees */
	double altitude;    /* Altitude in meters */
	double track;       /* Course made good (relative to true north) */
	double speed;	    /* Speed over ground, meters/sec */
	double time;        /* Time of update, seconds since Unix epoch */
	double hdop;		/* horizontal dilution of precision */
	double vdop;		/* vertical dilution of precision */
	double pdop;		/* dilution of precision */
	int satellitesView;	/* number of satellites in view */
	int satellitesUse;	/* number of satellites in use */
	int fix;			/* type of satellite fix */

	bool valid;
	bool seen_valid; /* ever had a valid fix? */
	bool nice_connection; /* is it possible to connect to the socket? */
} nmeaGPS;

///////////////////////////////////////////////////////

int parse_nmea0183(QString nmea, nmeaGPS &data);
bool check_speed(double speed);

#endif // NMEA0183_H
