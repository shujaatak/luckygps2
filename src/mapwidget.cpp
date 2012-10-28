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

#include <QPainter>
#include <QPen>
#include <QPluginLoader>
#include <QTextLayout>
#include <QtDebug>

#include "mapwidget.h"
#include "mapnikthread.h"
#include "filetilemanager.h"

#include <climits>
#include <cmath>

#ifndef Q_CC_MSVC
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
#else
#include <time.h>
#include <stdio.h>
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER liFrequency;
static LARGE_INTEGER liStartTime;
static LARGE_INTEGER liCurrentTime;

static void tstart ( void )
{
    QueryPerformanceFrequency ( &liFrequency );
    QueryPerformanceCounter ( &liStartTime );
}
static void tend ( void )
{
    QueryPerformanceCounter ( &liCurrentTime );
}
static double tval()
{
    return ((double)( (liCurrentTime.QuadPart - liStartTime.QuadPart)*(double)1000.0/(double)liFrequency.QuadPart ));
}

#endif


MapWidget::MapWidget(QWidget *parent)
    : QFrame(parent)
{
    _fullscreenButton = new QToolButton(this);
    _fullscreenButton->setAutoRaise(1);
    _fullscreenButton->setIcon(QIcon(":/icons/full2.png"));
    connect(_fullscreenButton, SIGNAL(clicked()), this, SLOT(callback_fullscreen_clicked()));

    _mapOverviewWidget = NULL;

    _sameevent = false;
    _x = 0;
    _y = 0;
    _startx = 0;
    _starty = 0;
    _zoom = 0;
    _centerMap = false;
    _degree = 0.0;
    _activeTrack = false;
    _enableScrollWheel = true;
    _max_generate_zoom = 17;
    _cbInFocus = false;
    _grabGPSCoords = false;
    _routingInfoHeight = 0;
    _mapStyle = 0;
    _painterImg = NULL;
    _mapRedrawCounter = 0;
    _dsm = NULL;

    _rsImage = QImage(":/icons/start-bg.png");
    _rfImage = QImage(":/icons/finish-bg.png");
    _posImage = QImage(":/icons/position.png");
    _posinvImage = QImage(":/icons/position-invalid.png");
    _arrowImage = QImage(":/icons/position-direction.png");
    _cbImage = QImage(":/icons/compass-bg-small.png");
    _cfImage = QImage(":/icons/compass-wheel-small.png");

    memset(&_gpsdata, 0, sizeof(nmeaGPS));

    /* timer to redraw map totally */
    connect(&_redrawTimer, SIGNAL(timeout()), this, SLOT(callback_redraw()));
    _redrawTimer.start(2000);

    _routing = new Routing();
}

MapWidget::~MapWidget()
{
    if(_painterImg)
        delete _painterImg;

    delete _routing;
}

void MapWidget::callback_redraw()
{
    update();
}

void MapWidget::callback_fullscreen_clicked()
{
    emit fullscreen_clicked();

    if(_mapStyle)
        _fullscreenButton->setIcon(QIcon(":/icons/full2.png"));
    else
        _fullscreenButton->setIcon(QIcon(":/icons/full1.png"));

    _mapStyle = !_mapStyle;
}

bool MapWidget::draw_route(QPainter &painter)
{
    if(_route.points.length() > 0)
    {
        QPoint *pts = new QPoint[_route.points.length()];
        int ptscnt = 0;

        for(int i = 0; i < _route.points.length(); i++)
        {
            const RoutePoint &point = _route.points[i];

            int pixel_x = longitude_to_pixel(get_zoom(), point.longitude_rad) - _x;
            int pixel_y = latitude_to_pixel(get_zoom(), point.latitude_rad) - _y;

            pts[ptscnt] = QPoint(pixel_x, pixel_y);
            ptscnt++;
        }

        QPen backupPen  = painter.pen();
        QPen newPen = backupPen;

        newPen.setWidth(10);
        newPen.setColor(Qt::darkRed);

        /* temporarily disabling antialiasing for drawPolyline (takes 3 times longer with antialiasing) */
        // painter.setRenderHint(QPainter::Antialiasing, false);

        painter.setPen(newPen);
        painter.setOpacity(0.6);

        drawPolyline(&painter, QRect(0, 0, width(), height()), pts, ptscnt);
        delete pts;

        /* enable antialiasing again */
        // painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setOpacity(1.0);
        painter.setPen(backupPen);

        return true;
    }
    else
        return false;
}

bool MapWidget::draw_track(QPainter &painter)
{
    bool status = false;

    if(_track.name.length() > 0)
    {
        QPen backupPen  = painter.pen();
        QPen newPen = backupPen;

        newPen.setWidth(5);
        newPen.setStyle(Qt::DotLine);
        painter.setOpacity(0.6);

        /* temporarily disabling antialiasing for drawPolyline (takes 3 times longer with antialiasing) */
        // painter.setRenderHint(QPainter::Antialiasing, false);

        const Track &track = _track;

        if(track.active)
            newPen.setColor(Qt::blue);
        else
            newPen.setColor(Qt::darkBlue);

        painter.setPen(newPen);

        for(int j = 0; j < track.segments.length(); j++)
        {
            const TrackSegment &segment = track.segments[j];

            // TODO check if segment bounding box is in view

            if(segment.points.length() > 1)
            {
                QPoint *pts = new QPoint[segment.points.length()];
                int ptscnt = 0;

                for(int k = 0; k < segment.points.length(); k++)
                {
                    const TrackPoint &point = segment.points[k];

                    double longitude_rad = deg_to_rad(point.gpsInfo.longitude);
                    double latitude_rad = deg_to_rad(point.gpsInfo.latitude);

                    int pixel_x = longitude_to_pixel(get_zoom(), longitude_rad) - _x;
                    int pixel_y = latitude_to_pixel(get_zoom(), latitude_rad) - _y;

                    pts[ptscnt] = QPoint(pixel_x, pixel_y);
                    ptscnt++;
                }

                drawPolyline(&painter, QRect(0, 0, width(), height()), pts, ptscnt);
                delete pts;
            }
        }

        /* change line color */
        // TODO

        /* enable antialiasing again */
        // painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setOpacity(1.0);
        painter.setPen(backupPen);

        status = true;
    }

    /* Draw start and end point of the route */
    if(_route.points.length() > 0)
    {
        if(!_rsImage.isNull())
        {
            double longitude_rad = 0.0;
            double latitude_rad = 0.0;

            longitude_rad = deg_to_rad(_route.points.first().longitude);
            latitude_rad = deg_to_rad(_route.points.first().latitude);

            int pixel_x = longitude_to_pixel(get_zoom(), longitude_rad);
            int pixel_y = latitude_to_pixel(get_zoom(), latitude_rad);

            pixel_x -= _x + _rsImage.width() / 2 + 12;
            pixel_y -= _y + _rsImage.height();

            painter.drawImage(pixel_x, pixel_y, _rsImage);

            status = true;
        }

        if(!_rfImage.isNull())
        {
            double longitude_rad = 0.0;
            double latitude_rad = 0.0;

            longitude_rad = deg_to_rad(_route.points.last().longitude);
            latitude_rad = deg_to_rad(_route.points.last().latitude);

            int pixel_x = longitude_to_pixel(get_zoom(), longitude_rad);
            int pixel_y = latitude_to_pixel(get_zoom(), latitude_rad);

            /* draw the icon centered according to the position */
            pixel_x -= _x + _rfImage.width() / 2;
            pixel_y -= _y + _rfImage.height() - 10;

            painter.drawImage(pixel_x, pixel_y, _rfImage);

            status = true;
        }
    }

    return status;
}

bool MapWidget::draw_position(QPainter &painter)
{
    if((_gpsdata.seen_valid && !_gpsdata.valid) || (_gpsdata.valid && !check_speed(_gpsdata.speed)))
    {
        QImage *img = NULL;

        /* differ between valid and invalid satellite fix */
        if(_gpsdata.valid)
            img = &_posImage;
        else
            img = &_posinvImage;

        if(!img->isNull())
        {
            double longitude_rad = 0.0;
            double latitude_rad = 0.0;

            /* differ between valid and invalid satellite fix */
            if(_gpsdata.valid)
            {
                longitude_rad = deg_to_rad(_gpsdata.longitude);
                latitude_rad = deg_to_rad(_gpsdata.latitude);
            }
            else
            {
                /* in case of no fix, take last known coordinates */
                longitude_rad = deg_to_rad(_lon);
                latitude_rad = deg_to_rad(_lat);
            }

            int pixel_x = longitude_to_pixel(get_zoom(), longitude_rad);
            int pixel_y = latitude_to_pixel(get_zoom(), latitude_rad);

            /* draw the icon centered according to the position */
            pixel_x -= _x + img->width() / 2;
            pixel_y -= _y + img->height() / 2;

            painter.drawImage(pixel_x, pixel_y, *img);

            return true;
        }
    }

    return false;
}

/* draw direction icon */
bool MapWidget::draw_direction(QPainter &painter)
{
    if(_gpsdata.valid && check_speed(_gpsdata.speed))
    {
        if(!_arrowImage.isNull())
        {
            QTransform mat;
            QImage img2;
            double longitude_rad = 0.0;
            double latitude_rad = 0.0;

            longitude_rad = deg_to_rad(_gpsdata.longitude);
            latitude_rad = deg_to_rad(_gpsdata.latitude);

            int pixel_x = longitude_to_pixel(get_zoom(), longitude_rad);
            int pixel_y = latitude_to_pixel(get_zoom(), latitude_rad);

            /* rotate icon to match track direction */
            mat.reset();
            mat.rotate(_degree);
            img2 = _arrowImage.transformed(mat, Qt::SmoothTransformation);

            /* draw the icon centered according to the position */
            pixel_x -= _x + img2.width() / 2;
            pixel_y -= _y + img2.height() / 2;

            painter.drawImage(pixel_x, pixel_y, img2);

            return true;
        }
    }

    return false;
}

bool MapWidget::draw_overview_map()
{
    if(get_zoom() > 8) // MINIMUM is min_map which is 5 atm DG TODO: use global define for min
    {
        int w = width() / 6;
        int h = height() / 6;
        int zoom = get_zoom() - 4;
        float factor = exp(zoom * M_LN2) / exp(get_zoom() * M_LN2);
        float x = ((_x + width()/2) * factor) - w/2;
        float y = ((_y + height()/2) * factor) - h/2;

        /* enforce overview map to have width > height */
        if(h > w)
        {
            int tmp = h;
            h = w;
            w = tmp;
        }

        /* requested tile information */
        TileInfo tile_info;
        QImage *mapImg = _dsm->getImage(x, y, zoom, w, h, tile_info, true);

        /* update overview map */
        _mapOverviewWidget->update_overviewMap(mapImg, _x, _y, width(), height(), -tile_info.offset_tile_x, -tile_info.offset_tile_y, w, h, factor);

        return true;
    }
    else
    {
        /* update overview map */
        if(_mapOverviewWidget->_outlineMapImg)
        {
            _mapOverviewWidget->update_overviewMap(NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0);

            return true;
        }
    }

    return false;
}

bool MapWidget::draw_info(QPainter &painter, QFont &font)
{
    /* top bar holds gps info, scale and fullscreen button */
    int topBarWidth = width() - 35;
    int topBarHeight = 25;

    /* prepare half transparent information area on the top */
    painter.setOpacity(0.8627); /* a = 220 */
    painter.fillRect(0, 0, topBarWidth, topBarHeight, Qt::white);
    painter.setOpacity(1.0);
    painter.drawRect(-1, -1, topBarWidth + 1, topBarHeight);

    /* draw fullscreen button */
    painter.setOpacity(0.8627); /* a = 220 */
    painter.fillRect(topBarWidth + 4, 0, width() - topBarWidth - 4, topBarHeight, Qt::white);
    painter.setOpacity(1.0);
    painter.drawRect(topBarWidth + 4, -1, width() - topBarWidth - 4, topBarHeight);
    _fullscreenButton->setGeometry(QRect(topBarWidth + 4 + 2, 0, width() - topBarWidth - 4 - 3, topBarHeight - 1));

    /* draw gps info */
    if(!_gpsdata.nice_connection)
    {
        /* choose correct message according to GPS interface */
#ifdef Q_CC_MSVC
        painter.drawText(10, topBarHeight - ((topBarHeight - 9) / 2), tr("Cannot find GPS device."));
#else
        painter.drawText(10, topBarHeight - ((topBarHeight - 9) / 2), tr("Cannot connect to GPSD."));
#endif
    }
    else if(!_gpsdata.seen_valid)
    {
        painter.drawText(10, topBarHeight - ((topBarHeight - 9) / 2), tr("No GPS found."));
    }
    else if(_gpsdata.seen_valid && !_gpsdata.valid)
    {
        painter.drawText(10, topBarHeight - ((topBarHeight - 9) / 2), tr("Searching satelites..."));
    }
    else
    {
        /* display small gps information string on top left of the MapWidget */
        /* speed - position */
        QString info;
        int speed = _gpsdata.speed * 3.6; // km/h

        if(_unit == 0)
            info = QString::number(speed, 'f', 1) + " km/h   ";
        else
        {
            speed *= 0.62137; // mph
            info = QString::number(speed, 'f', 1) + " mph   ";
        }
        info += QString::number(_gpsdata.latitude, 'f', 6) + "N - ";
        info += QString::number(_gpsdata.longitude, 'f', 6) + "E";
        painter.drawText(10, 20, info);
    }

    drawFrame( &painter );

    /* draw scale in info area on the top */
    if(get_zoom() > 6)
    {
        int scale_width = 0;
        int length = 200; /* scale bar width*/

        /* change scale bar width according to visible map area */
        if(width() < 400)
            length /= 2;

        QString scale = get_scale(pixel_to_latitude(get_zoom(), _y + 15), pixel_to_longitude(get_zoom(), 0), pixel_to_longitude(get_zoom(), length), &scale_width, _unit, length);

        QPen pen = painter.pen();
        pen.setWidth(2);
        painter.setPen(pen);
        painter.drawLine(topBarWidth - 10 - scale_width, topBarHeight-10,topBarWidth - 10, topBarHeight-10);
        painter.drawLine(topBarWidth - 10 - scale_width, topBarHeight-15, topBarWidth - 10 - scale_width, topBarHeight-5);
        painter.drawLine(topBarWidth -10, topBarHeight-15, topBarWidth -10, topBarHeight-5);

        /* tricky text aligning to the right */
        QTextLayout layout(scale, font);
        layout.beginLayout();
        QTextLine line = layout.createLine();
        layout.endLayout();
        QRectF rect = line.naturalTextRect();
        painter.drawText(topBarWidth - rect.width() - 20, topBarHeight - 12, scale);
    }

    return true;
}

void MapWidget::paintEvent(QPaintEvent *event)
{
    /* Valid DataSourceManager is needed to draw a map */
    if(!_dsm)
        return;

    if(!_mapOverviewWidget)
    {
        QList<QObject*> childs = this->children();
        for(int i = 0; i < childs.length(); i++)
        {
            if(childs[i]->objectName() == "overviewMapFrame")
            {
                _mapOverviewWidget = (MapOverviewWidget *)childs[i];
                break;
            }
        }
        if(!_mapOverviewWidget)
            return;
    }

    int toDrawX = event->rect().x();
    int toDrawY = event->rect().y();
    int toDrawWidth = event->rect().width();
    int toDrawHeight = event->rect().height();
    QPainter painter;
    QFont font;

    /* HACK TODO FIXME */
    /* Overview map widget got drawing issues when there is no full repaint at least 3 times */
    _mapRedrawCounter++;

    if(!_painterImg || (_painterImg->width() != width() || _painterImg->height() != height()))
    {
        if(_painterImg)
            delete _painterImg;

        _painterImg = new QImage(size(), QImage::Format_RGB32);
        _mapRedrawCounter = 0;
    }
    else if(_mapRedrawCounter > 3)
    {
        /* check for partial redraw */
        if(width() != toDrawWidth || height() != toDrawHeight)
        {
            QPainter widgetPainter;
            if(!widgetPainter.begin(this))
            {
                qDebug("MapWidget::paintEvent: Cannot create widgetPainter.");
                return;
            }
            widgetPainter.drawImage(toDrawX, toDrawY, *_painterImg, toDrawX, toDrawY, toDrawWidth, toDrawHeight);
            widgetPainter.end();

            _mapRedrawCounter = 3;

            return;
        }
    }
    else if(_mapRedrawCounter >= 3)
        _mapRedrawCounter = 3;

    if(!painter.begin(_painterImg))
    {
        qDebug("MapWidget::paintEvent: Cannot create painter.");
        return;
    }

    tstart();

    /* update local gps data in case satellite contact gets lost anyhow */
    if(_gpsdata.valid)
    {
        _lat = _gpsdata.latitude;
        _lon = _gpsdata.longitude;

        if(check_speed(_gpsdata.speed))
            _degree = _gpsdata.track;
    }

    QPen pen = painter.pen();
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.fillRect(0,0, width(), height(), Qt::white);

    /* disable antialiasing (speed, nicer lines, etc) */
    painter.setRenderHint(QPainter::Antialiasing, false);


    /* ---------------------------------- */
    /*           draw map tiles           */
    /* ---------------------------------- */

    /* requested tile information */
    TileInfo tile_info;
    QImage *mapImg = _dsm->getImage(_x, _y, get_zoom(), width(), height(), tile_info, false);

    if(mapImg)
        painter.drawImage(tile_info.offset_tile_x, tile_info.offset_tile_y, *mapImg);

    /* ---------------------------------- */

    /* draw route */
    draw_route(painter);

    /* draw tracks */
    draw_track(painter);

    /* draw position image (car, bicycle, ..) */
    draw_position(painter);

    /* draw direction icon */
    draw_direction(painter);

    /* "globally" set cap style to square since rounded ones are not needed anymore */
    {
        QPen pen = painter.pen();
        pen.setCapStyle(Qt::SquareCap);
        painter.setPen(pen);
    }

    /* draw overview map */
    draw_overview_map();

    /* draw gps data, scale, etc. in info area on the top */
    draw_info(painter, font);

    painter.end();

    QPainter widgetPainter;
    if(!widgetPainter.begin(this))
    {
        qDebug("MapWidget::paintEvent: Cannot create widgetPainter.");
        return;
    }
    widgetPainter.drawImage(0, 0, *_painterImg);
    widgetPainter.end();

    tend();
    qDebug(QString::number(tval()).toAscii());

    /* setup minimum redraw timer */
    _redrawTimer.stop();
    _redrawTimer.start(1000);
}

/* -------------------------------------------- */
/* special code for fast painter.drawPolyLine() */
/* -------------------------------------------- */
inline static float ldot(float a[2], float b[2])
{
    return a[0] * b[0] + a[1] * b[1];
}

// 2nd part of the function:
// Geometric Tools, LLC
// Copyright (c) 1998-2010
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
static bool lbi(int p1x, int p1y, int p2x, int p2y, int len1, int len2)
{
    int p0[2] = {p1x, p1y};
    int p1[2] = {p2x, p2y};
    int len[2] = {len1 - 1, len2 - 1};

    if(p0[0] < 0 && p1[0] < 0) // line is left of box
        return false;
    else if(p0[0] > len[0] && p1[0] > len[0]) // line is right of box
        return false;
    else if(p0[1] > len[1] && p1[1] > len[1]) // line is above box
        return false;
    else if(p0[1] < 0 && p1[1] < 0) // line is below box
        return false;
    else if ((p0[0] < 0 && p0[1] >= 0 && p0[1] <= len[1]) &&
             (p1[0] > len[0] && p1[1] >= 0 && p1[1] <= len[1]))
        return true;
    else if ((p0[1] < 0 && p0[0] >= 0 && p0[0] <= len[0]) &&
             (p1[1] > len[1] && p1[0] >= 0 && p1[0] <= len[0]))
        return true;

    return false;
}

void MapWidget::drawPolyline( QPainter* painter, const QRect& boundingBox,  QPoint *points, int size)
{
    QList< bool > isInside;
    QVector< QPoint > polygon;
    bool firstPoint = true;
    QPoint lastCoord;

    for (int i = 0; i < size; i++)
        isInside.push_back(boundingBox.contains(points[i]));

    QList< bool > draw = isInside;
    for (int i = 1; i < size; i++)
    {
        if (isInside[i - 1])
            draw[i] = true;
        if ( isInside[i] )
            draw[i - 1] = true;
        if(!isInside[i - 1] && !isInside[i])
        {
            bool toDraw = lbi(points[i-1].x(), points[i-1].y(), points[i].x(), points[i].y(), boundingBox.width(), boundingBox.height());
            draw[i - 1] |= toDraw;
            draw[i] |= toDraw;
        }
    }

    for(int i = 0; i < size; i++)
    {
        if(!draw[i])
        {
            if(!polygon.empty())
            {
                painter->drawPolyline( polygon.data(), polygon.size());
                polygon.clear();

                firstPoint = true;
            }
            continue;
        }

        QPoint pos = points[i];
        double xDiff = fabs((double)(pos.x() - lastCoord.x()));
        double yDiff = fabs((double)(pos.y() - lastCoord.y()));

        // don't draw very near route points
        if (!firstPoint && i != size - 1 && draw[i + 1] &&  xDiff * xDiff + yDiff * yDiff <= 64)
            continue;

        polygon.push_back( pos );
        lastCoord = pos;
        firstPoint = false;
    }
    painter->drawPolyline( polygon.data(), polygon.size() );
}
/* -------------------------------------------- */


void MapWidget::mousePressEvent( QMouseEvent *event )
{
    if ((event->buttons() & Qt::LeftButton))
    {
        if((event->x() > width() - 31) &&
           (event->y() < 25))
        {
            /* fullscreen button click started */
        }
        else
        {
            _redrawTimer.stop();
            _sameevent = true;

            /* only change to hand cursor if "get gps coordinates" modus is not active */
            if(!_cbInFocus)
                setCursor(Qt::ClosedHandCursor);
            else
                _grabGPSCoords = true;

            _startx = event->x();
            _starty = event->y();

            _centerMap = false;
            emit map_dragged();
        }
    }
    event->accept();
}

void MapWidget::mouseMoveEvent( QMouseEvent *event )
{
    if (_sameevent)
    {
        /* change to hand cursor if "get gps coordinates" modus is still active */
        if(_grabGPSCoords)
        {
            setCursor(Qt::ClosedHandCursor);
            _grabGPSCoords = false;
        }

        _x -= event->x() - _startx;
        _y -= event->y() - _starty;

        _startx = event->x();
        _starty = event->y();

        /* don't do full redraw here */
        update();
    }
    event->accept();
}

void MapWidget::mouseReleaseEvent( QMouseEvent *event )
{
    if (_sameevent)
    {
        _sameevent = false;
        setCursor(Qt::ArrowCursor);

        /* don't do full redraw here */
        update();

        if(_cbInFocus && _grabGPSCoords)
            emit map_clicked(rad_to_deg(pixel_to_longitude(get_zoom(), _x + event->x())), rad_to_deg(pixel_to_latitude(get_zoom(), _y + event->y())));
        _grabGPSCoords = false;
    }
    event->accept();
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->delta() / 8;
    int numSteps = numDegrees / 15;

    event->accept();

    if(!_enableScrollWheel)
        return;

    if (!(event->orientation() == Qt::Horizontal))
    {
        int old_zoom = get_zoom(), new_zoom;
        double factor;

        set_zoom(old_zoom + numSteps);
        new_zoom = get_zoom();

        if(old_zoom == new_zoom)
            return;

        factor = exp(new_zoom * M_LN2) / exp(old_zoom * M_LN2);

        if(numSteps > 0)
        {
            /* on zoom-in */
            _x = factor * _x + event->pos().x();
            _y = factor * _y + event->pos().y();
        }
        else
        {
            /* on zoom-out */
            _x = _x*factor - event->pos().x()*factor;
            _y = _y*factor - event->pos().y()*factor;
        }
        emit zoom_level_changed(-1, new_zoom);
    }
}

void MapWidget::callback_routecb_getFocus()
{
    /* tell mapwidget to switch modus */
    _cbInFocus = true;
}

void MapWidget::callback_routecb_lostFocus()
{
    /* tell mapwidget to switch modus */
    _cbInFocus = false;
}

void MapWidget::set_center(double latitude, double longitude, int hor_corr, bool autocenter, bool do_update)
{
    int pixel_x, pixel_y;
    double longitude_rad, latitude_rad;

    _centerMap = autocenter;

    /* convert units to radiance */
    longitude_rad = deg_to_rad(longitude);
    latitude_rad = deg_to_rad(latitude);

    /* get their "pixel" value to load correct tiles from file */
    pixel_x = longitude_to_pixel(get_zoom(), longitude_rad);
    pixel_y = latitude_to_pixel(get_zoom(), latitude_rad);

    // hor_corr = horizontal correction if e.g. a settings tab is open
    _x = pixel_x - (width() - hor_corr)/2;
    _y = pixel_y - height()/2;

    if(do_update)
        update();
}

void MapWidget::set_zoom(int zoom)
{
    /* zoom range is 5-17 */
    /* Original OSM supports 2 - 18 */
    if(zoom >= 5 && zoom <= 17)
    {
        _zoom = zoom;
        update();
    }
}

void MapWidget::generate_tiles(int x, int y, int zoom, int w, int h)
{
    if(!_dsm)
        return;

    TileInfo info;
    _dsm->getImage(x, y, zoom, w, h, info, 0);
}

