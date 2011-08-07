/*
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "import_export.h"

#include <QDomDocument>
#include <QXmlStreamWriter>
#include <QMessageBox>

#if 0
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
static void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
static void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}
static double tval()
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec*1000 + ( double ) _tstart.tv_usec/ ( 1000 );
	t2 = ( double ) _tend.tv_sec*1000 + ( double ) _tend.tv_usec/ ( 1000 );
	return t2-t1;
}
#endif

QString export_gpx(Track track, QString path)
{
    QFile file(path + "/" + track.name + ".gpx");
	QFileInfo fileInfo(file);

	if (!file.open(QIODevice::WriteOnly))
		return QString("");

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.writeStartDocument();

    xmlWriter.writeStartElement("gpx");
    xmlWriter.writeAttribute("version", "1.1");
	xmlWriter.writeAttribute("creator", "luckyGPS 0.75 - http://www.luckygps.com");
    xmlWriter.writeAttribute("xmlns", "http://www.topografix.com/GPX/1/1");
    xmlWriter.writeAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
	xmlWriter.writeAttribute("xmlns:rmc", "urn:net:trekbuddy:1.0:nmea:rmc");
	xmlWriter.writeAttribute("xsi:schemaLocation", "http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd");

    xmlWriter.writeStartElement("trk");
    xmlWriter.writeTextElement("name", track.name);

    for(int i = 0; i < track.segments.length(); i++)
    {
        xmlWriter.writeStartElement("trkseg");

		for(int j = 0; j < track.segments[i].points.length(); j++)
        {
            xmlWriter.writeStartElement("trkpt");

			xmlWriter.writeAttribute("lat", QString::number(track.segments[i].points[j].gpsInfo.latitude, 'f', 8));
			xmlWriter.writeAttribute("lon", QString::number(track.segments[i].points[j].gpsInfo.longitude, 'f', 8));

			xmlWriter.writeTextElement("ele", QString::number(track.segments[i].points[j].gpsInfo.altitude, 'f', 2));
			QDateTime t = QDateTime::fromTime_t(track.segments[i].points[j].gpsInfo.time);
            t = t.toUTC();
			QString time = t.toString("yyyy-MM-dd'T'hh:mm:ss.zzz'Z'");
            xmlWriter.writeTextElement("time", time);
			xmlWriter.writeTextElement("hdop", QString::number(track.segments[i].points[j].gpsInfo.hdop, 'f', 2));
			xmlWriter.writeTextElement("vdop", QString::number(track.segments[i].points[j].gpsInfo.vdop, 'f', 2));
			xmlWriter.writeTextElement("pdop", QString::number(track.segments[i].points[j].gpsInfo.pdop, 'f', 2));
			xmlWriter.writeTextElement("sat", QString::number(track.segments[i].points[j].gpsInfo.satellitesUse));

			/* satellite fix type */
			QString fix = nmeaFix_to_gpxType(track.segments[i].points[j].gpsInfo.fix);
			if(fix.length() > 0)
				xmlWriter.writeTextElement("fix", fix);

			/* speed and course extensions */
			xmlWriter.writeStartElement("extensions");
			xmlWriter.writeTextElement("rmc:speed", QString::number(track.segments[i].points[j].gpsInfo.speed, 'f', 2));
			xmlWriter.writeTextElement("rmc:course", QString::number(track.segments[i].points[j].gpsInfo.track, 'f', 2));
			xmlWriter.writeEndElement();

            xmlWriter.writeEndElement();
        }
        xmlWriter.writeEndElement();
    }

    xmlWriter.writeEndElement();

    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    file.close();

	return fileInfo.fileName();
}

static void import_route_rte(QXmlStreamReader &xml, Route &route)
{
    while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "rte"))
    {
        if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "rtept")
        {
            QString desc;
            QXmlStreamAttributes attrs = xml.attributes();
            /*get value of each attribute from QXmlStreamAttributes */
            QStringRef fLat = attrs.value("lat");
            QStringRef fLon = attrs.value("lon");
            double lat = 0, lon = 0;

            if(!fLat.isEmpty() && !fLon.isEmpty())
            {
                lat = fLat.toString().toDouble();
                lon = fLon.toString().toDouble();
            }

            while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "rtept"))
            {
                if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "desc")
                {
                    desc = xml.readElementText();
                }
                else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "cmt" && desc.length() == 0)
                {
                    desc = xml.readElementText();
                }

                xml.readNext();
            }
            route.insert_point(RoutePoint(lat, lon, desc));
        }
        else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "name")
        {
            route.name = xml.readElementText();
        }

        xml.readNext();
    }
}

static void import_route_trk(QXmlStreamReader &xml, Route &route)
{
    /* only support one track segment per file for routing */
    bool oneTrack = 0;

    while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "trk"))
    {
        if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "trkseg")
        {
            while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "trkseg"))
            {
                if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "trkpt")
                {
                    QString desc;
                    QXmlStreamAttributes attrs = xml.attributes();
                    /*get value of each attribute from QXmlStreamAttributes */
                    QStringRef fLat = attrs.value("lat");
                    QStringRef fLon = attrs.value("lon");
                    double lat = 0, lon = 0;

                    if(!fLat.isEmpty() && !fLon.isEmpty())
                    {
                        lat = fLat.toString().toDouble();
                        lon = fLon.toString().toDouble();
                    }

                    while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "trkpt"))
                    {
                        if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "desc")
                        {
                            desc = xml.readElementText();
                        }
                        else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "cmt" && desc.length() == 0)
                        {
                            desc = xml.readElementText();
                        }

                        xml.readNext();
                    }
                    route.insert_point(RoutePoint(lat, lon, desc));
                }
                oneTrack = true;
                xml.readNext();
            }

            /* only support one track segment per file for routing */
            if(oneTrack)
                break;
        }
        else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "name")
        {
            route.name = xml.readElementText();
        }

        /* only support one track segment per file for routing */
        if(oneTrack)
            break;

        xml.readNext();
    }
}

void import_route(Route &route, QString filename)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly))
        return;

    /* prepare new route */
    route.init();

    QXmlStreamReader xml(&file);

    while(!xml.atEnd() && !xml.hasError())
    {
        /* Read next element.*/
        QXmlStreamReader::TokenType token = xml.readNext();

        /* If token is StartElement, we'll see if we can read it.*/
        if(token == QXmlStreamReader::StartElement && xml.name() == "gpx")
        {
            xml.readNext();

            while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "gpx"))
            {
                if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "rte")
                {
					import_route_rte(xml, route);
                    break;
                }
                else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "trk")
                {
					import_route_trk(xml, route);
                    break;
                }

                xml.readNext();
            }
            break;
        }
    }

    /*
	// TODO: show error message to user
    // Debug message to check if gpx/xml import went fine
	qDebug("%s\n", xml.errorString().toAscii().constData());
    */

    file.close();

    /* now fill additional information about e.g. descriptions */
    int j = -1;
    for(int i = route.points.length() - 1; i >= 0; i--)
    {
        RoutePoint &point = route.points[i];

		point.nextDesc = j;

        /* we like to know the index of the next item with a non-empty description/name/stretname tag */
        if(point.desc.length() > 0)
            j = i;
    }
}

static void read_gpx_point(QXmlStreamReader &xml, TrackPoint &point)
{
	QXmlStreamAttributes attrs = xml.attributes();
	/*get value of each attribute from QXmlStreamAttributes */
	QStringRef fLat = attrs.value("lat");
	QStringRef fLon = attrs.value("lon");

	if(!fLat.isEmpty() && !fLon.isEmpty())
	{
		point.gpsInfo.latitude = fLat.toString().toDouble();
		point.gpsInfo.longitude = fLon.toString().toDouble();
	}

	while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "trkpt") &&
		  !(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "rtept") &&
		  !(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "wpt") &&
		  !xml.atEnd())
	{
		if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "time")
		{
			QString value = xml.readElementText();
			if (!value.isEmpty())
			{
				QString timestring;
				int tp = 0;

				if(value[19] == '.')
				{
					timestring = "yyyy-MM-ddTHH:mm:ss.SSS";
					tp = 23;
				}
				else
				{
					timestring = "yyyy-MM-ddTHH:mm:ss";
					tp = 19;
				}

				QDateTime dt(QDateTime::fromString(value.left(tp), timestring));
				dt.setTimeSpec(Qt::UTC);
				point.gpsInfo.time = (double) dt.toTime_t();
			}
		}
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "ele")
			point.gpsInfo.altitude = xml.readElementText().toDouble();
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "hdop")
			point.gpsInfo.hdop = xml.readElementText().toDouble();
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "vdop")
			point.gpsInfo.vdop = xml.readElementText().toDouble();
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "pdop")
			point.gpsInfo.pdop = xml.readElementText().toDouble();
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "sat")
			point.gpsInfo.satellitesUse = xml.readElementText().toDouble();
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "fix")
		{
			// TODO
		}
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "extensions")
		{
			// read speed + course as extension
			while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "extensions") && !xml.atEnd())
			{
				if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "speed")
					point.gpsInfo.speed = xml.readElementText().toDouble();
				else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "course")
					point.gpsInfo.track = xml.readElementText().toDouble();

				xml.readNext();
			}
		}

		xml.readNext();
	}
}

static void import_track_trk(QXmlStreamReader &xml, Track &track)
{
	while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "trk") && !xml.atEnd())
	{
		if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "trkseg")
		{
			TrackSegment segment;

			while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "trkseg") && !xml.atEnd())
			{
				if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "trkpt")
				{
					TrackPoint point;

					read_gpx_point(xml, point);
					segment.insert_point(point);
				}
				else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "name" && track.name.length() == 0)
				{
					track.name = xml.readElementText();
				}

				xml.readNext();
			}

			/* delete segments with no points */
			if(segment.points.length() != 0)
				track.segments.append(segment);

		}
		else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "name")
		{
			track.name = xml.readElementText();
		}

		xml.readNext();
	}
}

static void import_gpx_rte(QXmlStreamReader &xml, Track &track)
{
	track.segments.append(TrackSegment(-1));

	while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "rte") && !xml.atEnd())
	{
		if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "rtept")
		{
			TrackPoint point;

			read_gpx_point(xml, point);
			track.insert_point(point);
		}
		else if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "name")
			track.name = xml.readElementText();
		else if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "desc" && track.name.length() == 0)
			track.name = xml.readElementText();
		else if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "cmt" && track.name.length() == 0)
			track.name = xml.readElementText();

		xml.readNext();
	}

	/* delete segments with no points */
	if( track.segments.last().points.length() == 0)
		track.segments.pop_back();

}

bool import_track(Track &track, QString filename)
{
	QFile file(filename);

	if (!file.open(QIODevice::ReadOnly))
		return false;

	QXmlStreamReader xml(&file);

	while(!xml.atEnd() && !xml.hasError())
	{
		/* Read next element.*/
		QXmlStreamReader::TokenType token = xml.readNext();

		/* If token is StartElement, we'll see if we can read it.*/
		if(token == QXmlStreamReader::StartElement && xml.name() == "gpx")
		{
			xml.readNext();

			while(!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "gpx") && !xml.atEnd())
			{
				if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "rte")
				{
					import_gpx_rte(xml, track);

					track.updateBB();

					if(track.segments.length() == 0)
						track.name = ""; /* this indicates an empty track */

					break;
				}
				else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "trk")
				{
					import_track_trk(xml, track);
					track.updateBB();

					if(track.segments.length() == 0)
						track.name = ""; /* this indicates an empty track */

					break;
				}
				else if(xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "wpt")
				{
					TrackPoint point;

					if(track.segments.length() == 0)
					{
						track.segments.append(TrackSegment(-1));
						track.name = MainWindow::tr("Waypoints");
					}

					read_gpx_point(xml, point);
					track.insert_point(point);
				}

				xml.readNext();
			}
			break;
		}
	}

	// TODO: show error message to user
	// Debug message to check if gpx/xml import went fine
	// qDebug("%s\n", xml.errorString().toAscii().constData());

	file.close();

	return true;
}
