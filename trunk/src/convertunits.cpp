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

#include "convertunits.h"
#include "mainwindow.h"

#include <cmath>

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif /* !M_LN2 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif /* !M_PI */

#ifdef Q_CC_MSVC

#define isnan(n) _isnan(n)
#define finite _finite
#define hypot _hypot

static double msvc_atanh(double x)
{
   return ( 0.5 * log( ( 1.0 + x ) / ( 1.0 - x ) ) );
}
#define atanh msvc_atanh
#endif


double deg_to_rad(double d)
{
        return ((d * M_PI) / 180.0);
}

double rad_to_deg(double r)
{
        return ((r * 180.0) / M_PI);
}

int latitude_to_pixel(double zoom, double lat)
{
        double lat_m;
        int pixel_y;

        lat_m = atanh(sin(lat));
        pixel_y = -( (lat_m * (double)TILE_SIZE * exp(zoom * M_LN2) ) / (2.0 * M_PI)) +
                    (exp(zoom * M_LN2) * ((double)TILE_SIZE / 2.0) );

        return pixel_y;
}

int longitude_to_pixel(double zoom, double lon)
{
        int pixel_x;

        pixel_x = ( (lon * (double)TILE_SIZE * exp(zoom * M_LN2) ) / (2.0 * M_PI) ) +
                    ( exp(zoom * M_LN2) * ((double)TILE_SIZE / 2.0) );
        return pixel_x;
}

double pixel_to_longitude(double zoom, int pixel_x)
{
        double lon;

        lon = ((pixel_x - ( exp(zoom * M_LN2) * (TILE_SIZE/2.0) ) ) *2.0*M_PI) / (TILE_SIZE * exp(zoom * M_LN2) );

        return lon;
}

double pixel_to_latitude(double zoom, int pixel_y)
{
        double lat, lat_m;

        lat_m = (-( pixel_y - ( exp(zoom * M_LN2) * (TILE_SIZE/2.0) ) ) * (2.0*M_PI)) /(TILE_SIZE * exp(zoom * M_LN2));
        lat = asin(tanh(lat_m));

        return lat;
}

/* Return distance in km */
double fast_distance_rad(double lat1, double lon1, double lat2, double lon2)
{
    double dist = acos(sin(lat1)*sin(lat2)+cos(lat1)*cos(lat2)*cos(lon2-lon1))*6371.0;

    if(isnan(dist))
        dist = 0;

    return dist;
}

const double fast_distance_deg(const double v[2], const double w[2])
{
    return fast_distance_rad(deg_to_rad(v[0]), deg_to_rad(v[1]), deg_to_rad(w[0]), deg_to_rad(w[1]));
}

double fast_distance_deg(double lat1, double lon1, double lat2, double lon2)
{
    return fast_distance_rad(deg_to_rad(lat1), deg_to_rad(lon1), deg_to_rad(lat2), deg_to_rad(lon2));
}

double get_distance(double lat1, double lon1, double lat2_rad, double lon2_rad)
{
    return get_distance_rad(deg_to_rad(lat1), deg_to_rad(lon1), lat2_rad, lon2_rad);
}

double get_distance_deg(double lat1, double lon1, double lat2, double lon2)
{
    return get_distance_rad(deg_to_rad(lat1), deg_to_rad(lon1), deg_to_rad(lat2), deg_to_rad(lon2));
}

double get_distance_rad(double lat1_rad, double lon1_rad, double lat2_rad, double lon2_rad)
{
    // get constants

    /* use WGS84/Mercator */
    double a = 6378137.0;
    double b = 6356752.314245;
    double f = 1.0 / 298.257223563;

    // get parameters as radians
    double phi1 = lat1_rad; // deg_to_rad(lat1);
    double lambda1 = lon1_rad; // deg_to_rad(lon1);
    double phi2 = lat2_rad; // deg_to_rad(lat2);
    double lambda2 = lon2_rad; // deg_to_rad(lon2);

    // calculations
    double a2 = a * a;
    double b2 = b * b;
    double a2b2b2 = (a2 - b2) / b2;

    double omega = lambda2 - lambda1;

    double tanphi1 = tan(phi1);
    double tanU1 = (1.0 - f) * tanphi1;
    double U1 = atan(tanU1);
    double sinU1 = sin(U1);
    double cosU1 = cos(U1);

    double tanphi2 = tan(phi2);
    double tanU2 = (1.0 - f) * tanphi2;
    double U2 = atan(tanU2);
    double sinU2 = sin(U2);
    double cosU2 = cos(U2);

    double sinU1sinU2 = sinU1 * sinU2;
    double cosU1sinU2 = cosU1 * sinU2;
    double sinU1cosU2 = sinU1 * cosU2;
    double cosU1cosU2 = cosU1 * cosU2;

    // eq. 13
    double lambda = omega;

    // intermediates we'll need to compute 's'
    double A = 0.0;
    double B = 0.0;
    double sigma = 0.0;
    double deltasigma = 0.0;
    double lambda0;
    bool converged = false;

    for (int i = 0; i < 10; i++)
    {
        lambda0 = lambda;

        double sinlambda = sin(lambda);
        double coslambda = cos(lambda);

        // eq. 14
        double sin2sigma = (cosU2 * sinlambda * cosU2 * sinlambda) +
             (cosU1sinU2 - sinU1cosU2 * coslambda) *
             (cosU1sinU2 - sinU1cosU2 * coslambda);

        double sinsigma = sqrt(sin2sigma);

        // eq. 15
        double cossigma = sinU1sinU2 + (cosU1cosU2 * coslambda);

        // eq. 16
        sigma = atan2(sinsigma, cossigma);

        // eq. 17 Careful! sin2sigma might be almost 0!
        double sinalpha = (sin2sigma == 0) ? 0.0 :
              cosU1cosU2 * sinlambda / sinsigma;

        double alpha = asin(sinalpha);
        double cosalpha = cos(alpha);
        double cos2alpha = cosalpha * cosalpha;

        // eq. 18 Careful! cos2alpha might be almost 0!
        double cos2sigmam = cos2alpha == 0.0 ? 0.0 :
            cossigma - 2 * sinU1sinU2 / cos2alpha;

        double u2 = cos2alpha * a2b2b2;

        double cos2sigmam2 = cos2sigmam * cos2sigmam;

        // eq. 3
        A = 1.0 + u2 / 16384 * (4096 + u2 *
            (-768 + u2 * (320 - 175 * u2)));

        // eq. 4
        B = u2 / 1024 * (256 + u2 * (-128 + u2 * (74 - 47 * u2)));

        // eq. 6
        deltasigma = B * sinsigma * (cos2sigmam + B / 4
            * (cossigma * (-1 + 2 * cos2sigmam2) - B / 6
            * cos2sigmam * (-3 + 4 * sin2sigma)
            * (-3 + 4 * cos2sigmam2)));

        // eq. 10
        double C = f / 16 * cos2alpha * (4 + f * (4 - 3 * cos2alpha));

        // eq. 11 (modified)
        lambda = omega + (1 - C) * f * sinalpha
            * (sigma + C * sinsigma * (cos2sigmam + C
            * cossigma * (-1 + 2 * cos2sigmam2)));

        // see how much improvement we got
        double change = fabs((lambda - lambda0) / lambda);

        if ((i > 1) && (change < 0.0000000000001))
        {
             converged = true;
             break;
        }
    }

    // eq. 19
    double s = b * A * (sigma - deltasigma);

    return s;
}

/* convert a distance between two points into a string with units */
QString distance_to_scale(double distance, double *scale_factor, int units)
{
    double unit_conversion = 1.0;
    QString unit_string = "";
    QString unit_name = "";

    if(units == 0) /* metrical */
    {
        unit_conversion = 1.0;
        unit_name = "km";
    }
    else if(units == 1) /* imperial */
    {
        unit_conversion = 1.0/1.6093444;
        unit_name = "m";
    }

    distance *= unit_conversion;
    if (distance >= 5000)
    {
        unit_string = "5000" + unit_name;
        *scale_factor = 5000 / distance;
    }
    else if (distance < 5000 && distance >= 2000)
    {
        unit_string = "2000" + unit_name;
        *scale_factor = 2000 / distance;
    }
    else if (distance < 2000 && distance >= 1000)
    {
        unit_string = "1000" + unit_name;
        *scale_factor = 1000 / distance;
    }
    else if (distance < 1000 && distance >= 500)
    {
        unit_string = "500" + unit_name;
        *scale_factor = 500 / distance;
    }
    else if (distance < 500 && distance >= 200)
    {
        unit_string = "200" + unit_name;
        *scale_factor = 200 / distance;
    }
    else if (distance < 200 && distance >= 100)
    {
        unit_string = "100" + unit_name;
        *scale_factor = 100 / distance;
    }
    else if (distance < 100 && distance >= 50)
    {
        unit_string = "50" + unit_name;
        *scale_factor = 50 / distance;
    }
    else if (distance < 50 && distance >= 20)
    {
        unit_string = "20" + unit_name;
        *scale_factor = 20 / distance;
    }
    else if (distance < 20 && distance >= 10)
    {
        unit_string = "10" + unit_name;
        *scale_factor = 10 / distance;
    }
    else if (distance < 10 && distance >= 5)
    {
        unit_string = "5" + unit_name;
        *scale_factor = 5 / distance;
    }
    else if (distance < 5 && distance >= 2)
    {
        unit_string = "2" + unit_name;
        *scale_factor = 2 / distance;
    }
    else if (distance < 2 && distance >= 1)
    {
        unit_string = "1" + unit_name;
        *scale_factor = 1 / distance;
    }
    else /* we have a smaller distance than 1km / 1m */
    {
        if(units == 0) /* metrical */
        {
            if (distance < 1 && distance >= 0.5)
            {
                unit_string = "500";
                *scale_factor = 0.5 / distance;
            }
            else if (distance < 0.5 && distance >= 0.2)
            {
                unit_string = "200";
                *scale_factor = 0.2 / distance;
            }
            else if (distance < 0.2 && distance >= 0.1)
            {
                unit_string = "100";
                *scale_factor = 0.1 / distance;
            }
            else if (distance < 0.1 && distance >= 0.05)
            {
                unit_string = "50";
                *scale_factor = 0.05 / distance;
            }
            else if (distance < 0.05 && distance >= 0.02)
            {
                unit_string = "20";
                *scale_factor = 0.02 / distance;
            }
            else
            {
                unit_string = "10";
                *scale_factor = 0.01 / distance;
            }

            unit_string += "m";
        }
        else if(units == 1) /* imperial */
        {
            distance *= 5280;
            if (distance >= 5000)
            {
                unit_string = "5000";
                *scale_factor = 5000 / distance;
            }
            else if (distance < 5000 && distance >= 2000)
            {
                unit_string = "2000";
                *scale_factor = 2000 / distance;
            }
            else if (distance < 2000 && distance >= 1000)
            {
                unit_string = "1000";
                *scale_factor = 1000 / distance;
            }
            else if (distance < 1000 && distance >= 500)
            {
                unit_string = "500";
                *scale_factor = 500 / distance;
            }
            else if (distance < 500 && distance >= 200)
            {
                unit_string = "200";
                *scale_factor = 200 / distance;
            }
            else if(distance < 200 && distance >= 100)
            {
                unit_string = "100";
                *scale_factor = 100 / distance;
            }
            else if(distance < 100 && distance >= 50)
            {
                unit_string = "50";
                *scale_factor = 50 / distance;
            }
            else
            {
                unit_string = "10";
                *scale_factor = 10/distance;
            }

            unit_string += "ft";
        }
    }

    return unit_string;
}

/* when loading a new route, zoom to it so it is fully covered on screen */
int get_route_zoom(int width, int height, double lat_max, double lon_min, double lat_min, double lon_max)
{
    int pixel_x1, pixel_y1, pixel_x2, pixel_y2;
    double zoom = 18.0;

    while (zoom>=2)
    {
        pixel_x1 = longitude_to_pixel((double)zoom, deg_to_rad(lon_min));
        pixel_y1 = latitude_to_pixel((double)zoom, deg_to_rad(lat_max));

        pixel_x2 = longitude_to_pixel((double)zoom, deg_to_rad(lon_max));
        pixel_y2 = latitude_to_pixel((double)zoom, deg_to_rad(lat_min));


        if((pixel_x1+width) > pixel_x2 && (pixel_y1+height) > pixel_y2)
             return zoom;

        zoom--;
    }

    return zoom;
}

/* convert GPS fix type to GPX compatible text */
QString nmeaFix_to_gpxType(int fix)
{
    QString info = "";

    if(fix < 10)
    {
        if(fix == 2)
            info = "dgps";
        else if(fix == 3)
            info = "pps";
        else if(fix == 0)
            info = "none";
    }
    else
    {
        if(fix == 11)
            info = "none";
        else if(fix == 12)
            info = "2d";
        else if(fix == 13)
            info = "3d";
    }

    return info;
}

inline static double dot(const double v[2], const double w[2])
{
    return (v[0] * w[0] + v[1] * w[1]);
}

inline static double distance(const double v[2], const double w[2])
{
    return sqrt((w[0] - v[0]) * (w[0] - v[0]) + (w[1] - v[1]) * (w[1] - v[1]));
}

// Return minimum distance between line segment vw and point p
double minimum_distance(const double v[2], const double w[2], const double p[2])
{
    const double l2 = (w[0] - v[0]) * (w[0] - v[0]) + (w[1] - v[1]) * (w[1] - v[1]); // i.e. |w-v|^2 -  avoid a sqrt

    if (l2 < DBL_EPSILON)
      return  fast_distance_deg(p, v);   // v == w case

    // Consider the line extending the segment, parameterized as v + t (w - v).
    // We find projection of point p onto the line.
    // It falls where t = [(p-v) . (w-v)] / |w-v|^2

    const double pv[2] = {p[0] - v[0], p[1] - v[1]};
    const double wv[2] = {w[0] - v[0], w[1] - v[1]};
    const double t = dot(pv, wv) / l2;

    if (t < 0.0)
        return  fast_distance_deg(p, v);	// Beyond the 'v' end of the segment
    else if (t > 1.0)
        return  fast_distance_deg(p, w);  // Beyond the 'w' end of the segment

    const double projection[2] = {v[0] + t * (w[0] - v[0]), v[1] + t * (w[1] - v[1])}; // Projection falls on the segment

    return  fast_distance_deg(p, projection);
}
