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

#ifndef CONVERTUNITS_H
#define CONVERTUNITS_H

#include <QString>

#define TILE_SIZE 256

/* convert degree to radiance and backwards */
double deg_to_rad(double d);
double rad_to_deg(double r);

/* convert latitude/longitude to pixel on map and backwards */
int longitude_to_pixel(double zoom, double lon);
int latitude_to_pixel(double zoom, double lat);
double pixel_to_longitude(double zoom, int pixel_x);
double pixel_to_latitude(double zoom, int pixel_y);

/* --------------------------------- */
/* get distance between 2 gps points */
/* --------------------------------- */
// input: point A in degree, point B in rad
double get_distance(double lat1, double lon1, double lat2_rad, double lon2_rad);
// input: point A and B in rad
double get_distance_rad(double lat1_rad, double lon1_rad, double lat2_rad, double lon2_rad);
// input: point A and B in degree
double get_distance_deg(double lat1, double lon1, double lat2, double lon2);

double fast_distance_rad(double lat1, double lon1, double lat2, double lon2);
double fast_distance_deg(double lat1, double lon1, double lat2, double lon2);

/* --------------------------------- */

/* point - line - distance */
double minimum_distance(const double v[2], const double w[2], const double p[2]);

/* convert a distance between two points into a string with units */
QString distance_to_scale(double distance, double *scale_factor, int units);

/* convert GPS fix type to GPX compatible text */
QString nmeaFix_to_gpxType(int fix);

/* when loading a new route, zoom to it so it is fully covered on screen */
int get_route_zoom(int width, int height, double lat_max, double lon_min, double lat_min, double lon_max);

/* Ensure a proper gps coordinate format string */
bool verifyGpsString(QString text, double &lat, double &lon);

#endif // CONVERTUNITS_H
