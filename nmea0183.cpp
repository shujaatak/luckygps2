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

#include "nmea0183.h"

#include <time.h>

#include <QStringList>


/* check if speed is high enough to give directional information to compass etc */
bool check_speed(double speed)
{
	return ((speed * 3.6) > 2.0);
}

bool parse_nmea_checksum(QString nmea)
{
	/* test for correct checksum, nice GPS devices got this and only "nice" GPS devices are supported */
	if(nmea.count('*') == 1)
	{
		QStringList rawlist = nmea.split('*');
		QString rawstring = rawlist[0];
		bool ok = 0;
		int orig_checksum = rawlist[1].left(2).toInt(&ok, 16);
		int checksum = 0;

		/* check for missing checksum ("*" is there but no actual checksum) */
		if(rawlist[1].length() < 2)
			return true;

		for(int i = 0; i < rawstring.length(); i++)
			checksum ^= (unsigned char)(rawstring[i].toAscii());

		if(checksum != orig_checksum)
			return false;
	}
	else
		return true; // return true in case that no checksum is given

	return true;
}

bool parse_nmea_rmc(QString nmea, nmeaGPS &data)
{
	QStringList array;
	typedef struct tm tm_t;
	static tm_t tm;

	memset(&tm, 0, sizeof(tm_t));

	if(!parse_nmea_checksum(nmea))
		return false;

	array = nmea.split(',');
	if (array.length() > 9)
	{
		data.valid = (array[2].compare("A")==0) ? true : false;

		/* reset fix */
		if(!data.valid)
			data.fix = 0;

		/* only update data if there is a valid satellite fix */
		if(!data.valid)
			return false;

		data.seen_valid = true;

		if (array[1].length() >= 6)
		{
			tm.tm_hour = array[1].left(2).toInt();
			tm.tm_min = array[1].mid(2,2).toInt();
			tm.tm_sec = (int)array[1].mid(4).toDouble();
		}

		if (array[9].length() == 6)
		{
			tm.tm_mday= array[9].left(2).toInt();
			tm.tm_mon = array[9].mid(2,2).toInt() - 1;
			tm.tm_year= (int)array[9].mid(4).toDouble() + 100;

			data.time = (double) mktime(&tm);

			double milli = array[1].mid(4).toDouble() - tm.tm_sec;
			data.time += milli;
		}

		/* parse latitude */
		if (array[3].length() >= 3)
		{
			 data.latitude = array[3].left(2).toDouble() + (array[3].mid(2).toDouble() / 60.0);

			 if(array[4].compare("S") == 0)
				 data.latitude *= -1.0;
		}

		/* parse longitude */
		if (array[5].length() >= 4)
		{
			data.longitude = array[5].left(3).toDouble() + (array[5].mid(3).toDouble() / 60.0);

			 if(array[6].compare("W") == 0)
				 data.longitude *= -1.0;
		}

		/* calculate speed in m/s from knots */
		data.speed = array[7].toDouble() * 0.514444;

		/* Track angle in degrees True */
		data.track = array[8].toDouble();
	}
	else
		return false;

	return true;
}

/* for more information see http://www.gpsinformation.org/dale/nmea.htm#GGA */
bool parse_nmea_gga(QString nmea, nmeaGPS &data)
{
	QStringList array;

	if(!parse_nmea_checksum(nmea))
		return false;

	array = nmea.split(',');

	if (array.length() > 9)
	{
		/* check if there is a valid fix */
		int fix = array[6].toInt();

		if(fix > 0)
		{
			data.satellitesUse = array[7].toInt();
			data.hdop = array[8].toDouble();
			data.altitude = array[9].toDouble();
		}

		data.fix = fix;

		return true;
	}
	return false;
}

/* for more information see http://www.gpsinformation.org/dale/nmea.htm#GGA */
bool parse_nmea_gsv(QString nmea, nmeaGPS &data)
{
	QStringList array;

	if(!parse_nmea_checksum(nmea))
		return false;

	array = nmea.split(',');

	if (array.length() > 3)
	{
		data.satellitesView = array[3].toInt();
		return true;
	}
	return false;
}

/* for more information see http://www.gpsinformation.org/dale/nmea.htm#GGA */
bool parse_nmea_gsa(QString nmea, nmeaGPS &data)
{
	QStringList array;

	if(!parse_nmea_checksum(nmea))
		return false;

	array = nmea.split(',');

	if (array.length() > 17 )
	{
		/* check if there is a valid fix */
		int fix = array[2].toInt();

		if(fix > 1)
		{
			QStringList vdopArray;

			data.pdop = array[15].toDouble();
			data.hdop = array[16].toDouble();

			vdopArray = array[17].split('*');
			data.vdop = vdopArray[0].toDouble();
		}

		data.fix = 10 + fix;

		return true;
	}
	return false;
}

int parse_nmea0183(QString nmea, nmeaGPS &data)
{
	QStringList result_array;
	bool ret = false;

	/* each gpsd nmea message starts with "$" */
	result_array = nmea.split('$');

	/* walk through all received messages */
	for(int i = 0; i < result_array.length(); i++)
	{
		/* first 5 characters define the message type */
		QString subset = result_array[i].left(5);

		if(subset.compare("GPGGA")==0)
		{
			if(parse_nmea_gga(result_array[i], data))
				ret = true;

			data.seen_valid = true;
		}
		else if (subset.compare("GPRMC")==0)
		{
			if(parse_nmea_rmc(result_array[i], data))
				ret = true;

			data.seen_valid = true;
		}
		else if (subset.compare("GPTXT")==0)
		{
			ret = true;
			data.seen_valid = true;
		}
		else if (subset.compare("GPGLL")==0)
		{
			ret = true;
			data.seen_valid = true;
		}
		else if (subset.compare("GPGSV")==0)
		{
			if(parse_nmea_gsv(result_array[i], data))
				ret = true;

			data.seen_valid = true;
		}
		else if (subset.compare("GPVTG")==0)
		{
			ret = true;
			data.seen_valid = true;
		}
		else if (subset.compare("GPGSA")==0)
		{
			if(parse_nmea_gsa(result_array[i], data))
				ret = true;

			data.seen_valid = true;
		}
	}

	return ret;
}
