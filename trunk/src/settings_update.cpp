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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "import_export.h"

#include "sqlite3.h"

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

/* check for compatible db version */
bool MainWindow::checkDBversion()
{
    sqlite3_stmt *st = NULL;
    bool ret = false;
    if(sqlite3_prepare_v2(_db, "SELECT * FROM version LIMIT 1;", -1, &st, NULL) == SQLITE_OK)
    {
        if(sqlite3_step(st) == SQLITE_ROW)
        {
            float version = sqlite3_column_double(st, 0);

            /* for now hardcoded version check */
			if(version == 0.75f)
                ret = true;
        }
        sqlite3_finalize(st);
    }

    return ret;
}

/* load GUI standard settings from luckygps.sqlite database */
/* TODO: better error handling in case of db/query failing */
bool MainWindow::loadSettings(gps_settings_t *settings, bool reset, bool startup)
{
    int sql_error;
    int map_id = 0, track_id = 0;
    sqlite3_stmt *st = NULL;

    /* check if load the backup values in row 1, other settings will be deleted */
    if(reset)
        sql_error = sqlite3_exec(_db, "DELETE FROM settings WHERE rowid>1;",NULL, 0, NULL);

    sql_error = sqlite3_prepare_v2(_db, "SELECT * FROM settings ORDER BY rowid DESC LIMIT 1;", -1, &st, NULL);
    if(sql_error == SQLITE_OK)
    {
        if(sqlite3_step(st) == SQLITE_ROW)
        {
            /* read settings from database into GUI */

			/* -------------- */
            /* track settings */
			/* -------------- */
            QString label_track_path = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 1)),sqlite3_column_bytes(st, 1) / sizeof(char));
            if(label_track_path.indexOf("~") == 0)
                label_track_path = QDir::homePath() + label_track_path.mid(1);
            _trackPath = label_track_path;
            ui->label_track_path->setText(label_track_path);
            track_id = sqlite3_column_int(st, 2);

            /* route settings */
            QString label_route_path = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 12)),sqlite3_column_bytes(st, 12) / sizeof(char));
            /* check if we got a relative path in map path settings */
            if(label_route_path.indexOf("~") == 0)
                label_route_path = QDir::homePath() + label_route_path.mid(1);
            _routePath = label_route_path;
            ui->label_route_path->setText(label_route_path);

			/* -------------*/
            /* map settings */
			/* ------------ */
            QString label_map_path = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 3)),sqlite3_column_bytes(st, 3) / sizeof(char));
            /* check if we got a relative path in map path settings */
            if(label_map_path.indexOf("~") == 0)
                label_map_path = QDir::homePath() + label_map_path.mid(1);
            ui->label_map_path->setText(label_map_path);
            map_id = sqlite3_column_int(st, 4);
            ui->cb_map_autodownload->setChecked(sqlite3_column_int(st, 6));
			ui->map->_tilesManager->set_autodownload(sqlite3_column_int(st, 6));

			/* ---------------- */
			/* general settings */
			/* ---------------- */
			int unit = sqlite3_column_int(st, 13);
			ui->map->set_unit(unit);
			if(unit == 0)
				ui->rb_settings_general_units_metric->setChecked(true);
			else
				ui->rb_settings_general_units_imperial->setChecked(true);
            ui->map->set_scroll_wheel(sqlite3_column_int(st, 7));
            ui->cb_map_view_zoomwheel->setChecked(sqlite3_column_int(st, 7));
            ui->map->set_mirror(sqlite3_column_int(st, 0));
            ui->cb_map_view_mirror->setChecked(sqlite3_column_int(st, 0));
            if(ui->map->get_mirror()) /* for now disable routing text + compass drawing in mirror mode */
            {
				ui->routeFrameBottom->setVisible(false);
            }
            else
            {
				ui->routeFrameBottom->setVisible(true);
            }
            ui->widget->update(); /* update UI after view settings changed */
            ui->tile_maxzoom_sb->setValue(sqlite3_column_int(st, 11));
            ui->map->set_max_generate_tiles(sqlite3_column_int(st, 11));
            if(startup)
            {
                ui->map->set_pos(QPoint(sqlite3_column_int(st, 8), sqlite3_column_int(st, 9)));
                ui->map->set_zoom(sqlite3_column_int(st, 10));
            }

			/* -------------- */
            /* cache settings */
			/* -------------- */
			_dsm->set_cache_size(sqlite3_column_int(st, 5));
            ui->settings_cache_spinbox->setValue(sqlite3_column_int(st, 5));
            on_settings_cache_spinbox_valueChanged(sqlite3_column_int(st, 5));
        }
        sqlite3_finalize(st);

    }

    /* get corresponding entries form other tables */

    /* get available maps */
    ui->cb_map_source->clear();
    QString dbquery = "SELECT name, url, folder, ID FROM map_repository;";
    sql_error = sqlite3_prepare_v2(_db, dbquery.toUtf8(), -1, &st, NULL);
    if(sql_error == SQLITE_OK)
    {
        while(sqlite3_step(st) == SQLITE_ROW)
        {
            ui->cb_map_source->addItem(QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 0)),sqlite3_column_bytes(st, 0) / sizeof(char)));
            if(map_id == sqlite3_column_int(st, 3))
            {
                /* apply settings */
                ui->map->set_path(ui->label_map_path->text() + "/" + QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 2)),sqlite3_column_bytes(st, 2) / sizeof(char)) + "/%1/%2%3%4%5");
                ui->map->set_url(QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 1)),sqlite3_column_bytes(st, 1) / sizeof(char)));
                ui->cb_map_source->setCurrentIndex(map_id - 1);
            }
        }
        sqlite3_finalize(st);
    }

    /* get available track export formats */
    ui->cb_track_format->clear();
    {
        ui->cb_track_format->addItem(tr("GPX"));
        ui->cb_track_format->setCurrentIndex(track_id-1);
    }

    /* check if load the backup values in row 1, other settings will be deleted */
    if(reset)
        sql_error = sqlite3_exec(_db, "DELETE FROM gps_settings WHERE rowid>1;",NULL, 0, NULL);

    sql_error = sqlite3_prepare_v2(_db, "SELECT * FROM gps_settings ORDER BY rowid DESC LIMIT 1;", -1, &st, NULL);
    if(sql_error == SQLITE_OK)
    {
        if(sqlite3_step(st) == SQLITE_ROW)
        {
            /* gpsd settings */
            if(settings)
            {
                settings->host = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 0)),sqlite3_column_bytes(st, 0) / sizeof(char));
                settings->port = QString::number(sqlite3_column_int(st, 1));

                /* serial/usb gps device settings */
                settings->portname = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 2)),sqlite3_column_bytes(st, 2) / sizeof(char));
                settings->baudrate = sqlite3_column_int(st, 3);
                settings->parity = sqlite3_column_int(st, 4);
                settings->databits = sqlite3_column_int(st, 5);
                settings->stopbits = sqlite3_column_int(st, 6);
                settings->flow = sqlite3_column_int(st, 7);

                set_gps_settings(*settings);

                if(_gpsd && reset)
                {
                    _gpsd->gpsd_close();
                    _gpsd->gpsd_update_settings(*settings);
                }
            }
        }
        sqlite3_finalize(st);
    }

    return true;
}

/* save GUI settings into database */
bool MainWindow::save_settings()
{
    sqlite3_stmt *stmt = NULL;

    /* cleanup database settings */
    sqlite3_exec(_db, "DELETE FROM settings WHERE rowid>1;",NULL, 0, NULL);

	if(sqlite3_prepare_v2(_db, "INSERT INTO settings VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not prepare statement.");
        return false;
    }

    /* mirror */
    if(sqlite3_bind_int(stmt, 1, ui->cb_map_view_mirror->isChecked()) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind int 1.");
        return false;
    }

    /* track path */
    QByteArray path = ui->label_track_path->text().toUtf8();
    if(sqlite3_bind_text (stmt, 2, path.constData(), -1, SQLITE_STATIC) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind text 2.");
        return false;
    }

    /* track id */
    if(sqlite3_bind_int(stmt, 3, ui->cb_track_format->currentIndex() + 1) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind int 3.");
        return false;
    }

    /* map path */
    QByteArray mappath = ui->label_map_path->text().toUtf8();
    if(sqlite3_bind_text (stmt, 4, mappath.constData(), -1, SQLITE_STATIC) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind text 4.");
        return false;
    }

    /* map int */
    if(sqlite3_bind_int(stmt, 5, ui->cb_map_source->currentIndex() + 1) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind int 5.");
        return false;
    }

     /* cache size */
    if(sqlite3_bind_int(stmt, 6, ui->settings_cache_spinbox->value()) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind int 6.");
        return false;
    }

    /* autodownload map */
    if(sqlite3_bind_int(stmt, 7, ui->cb_map_autodownload->isChecked()) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind int 7.");
        return false;
    }

    /* scroll wheel for zoom control on MapWidget */
    if(sqlite3_bind_int(stmt, 8, ui->cb_map_view_zoomwheel->isChecked()) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind int 8.");
        return false;
    }

    /* x, y, zoom are saved on exit */
    sqlite3_bind_int(stmt, 9, ui->map->get_pos().x());
    sqlite3_bind_int(stmt, 10, ui->map->get_pos().y());
    sqlite3_bind_int(stmt, 11, ui->map->get_zoom());

    /* max zoom level where you get tiles generated automatically */
    sqlite3_bind_int(stmt, 12, ui->tile_maxzoom_sb->value());

    /* route path */
    QByteArray routepath = ui->label_route_path->text().toUtf8();
    if(sqlite3_bind_text (stmt, 13, routepath.constData(), -1, SQLITE_STATIC) != SQLITE_OK)
    {
        qDebug("save_settings() - Could not bind text 13.");
        return false;
    }

	/* units */
	int unit = ui->rb_settings_general_units_metric->isChecked() ? 0 : 1;
	sqlite3_bind_int(stmt, 14, unit);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        qDebug("save_settings() - Could not step (execute) stmt.");
        return false;
    }

    /* cleanup database gps_settings */
    sqlite3_exec(_db, "DELETE FROM gps_settings WHERE rowid>1;",NULL, 0, NULL);

    if(sqlite3_prepare_v2(_db, "INSERT INTO gps_settings VALUES (?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not prepare statement.");
        return false;
    }

    /* gps_settings */
    gps_settings_t settings;
    get_gps_settings(settings);

    /* ------------------------------ */
    /* gpsd settings */
    /* ------------------------------ */
    /* gpsd host */
    QByteArray host = settings.host.toUtf8();
    if(sqlite3_bind_text(stmt, 1, host.constData(), -1, SQLITE_STATIC) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind text 1.");
        return false;
    }
    /* gpsd port */
    if(sqlite3_bind_int(stmt, 2, settings.port.toInt()) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind int 2.");
        return false;
    }
    /* ------------------------------ */

    /* ------------------------------ */
    /* serial/usb gps device settings */
    /* ------------------------------ */
    QByteArray portname = settings.portname.toUtf8();
    if(sqlite3_bind_text(stmt, 3, portname.constData(), -1, SQLITE_STATIC) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind text 3.");
        return false;
    }
    /* baudrate */
    if(sqlite3_bind_int(stmt, 4, settings.baudrate) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind int 4.");
        return false;
    }
    /* parity */
    if(sqlite3_bind_int(stmt, 5, settings.parity) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind int 5.");
        return false;
    }
    /* databits */
    if(sqlite3_bind_int(stmt, 6, settings.databits) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind int 6.");
        return false;
    }
    /* stopbits */
    if(sqlite3_bind_int(stmt, 7, settings.stopbits) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind int 7.");
        return false;
    }
    /* flow */
    if(sqlite3_bind_int(stmt, 8, settings.flow) != SQLITE_OK)
    {
        qDebug("save_settings() gps - Could not bind int 8.");
        return false;
    }
    /* ------------------------------ */

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        qDebug("save_settings() gps - Could not step (execute) stmt.");
        return false;
    }

    if(_gpsd)
    {
        _gpsd->gpsd_close();
        _gpsd->gpsd_update_settings(settings);
    }

    sqlite3_exec(_db, "VACUUM;", 0, NULL, 0);

    return true;
}

QStringList MainWindow::loadTrackFormats()
{
    QStringList list;

    /* get available track export formats */
    list.append("*.gpx");
    // list.append("*.kml");
    // list.append("*.log");

    return list;
}


QString MainWindow::saveTrack(Track *newtrack)
{
	/* export to gpx format, we use the track gpx format here */
	return export_gpx(*newtrack, _trackPath);
}

/* gps settings helper function to assign correct values from UI */
void MainWindow::getGpsSettings(gps_settings_t &settings)
{
    settings.host = ui->label_gpsd_host->text();
    settings.port = ui->label_gpsd_port->text();

    settings.portname = ui->cb_gps_portname->currentText();
    settings.databits = ui->cb_gps_databits->currentIndex();
    settings.flow = ui->cb_gps_flow->currentIndex();
    settings.parity = ui->cb_gps_parity->currentIndex();

#ifdef Q_OS_WIN
    if(ui->cb_gps_baudrate->currentIndex() == 0)
        settings.baudrate = BAUD4800;
    else if(ui->cb_gps_baudrate->currentIndex() == 1)
        settings.baudrate = BAUD9600;
    else if(ui->cb_gps_baudrate->currentIndex() == 2)
        settings.baudrate = BAUD19200;
    else if(ui->cb_gps_baudrate->currentIndex() == 3)
        settings.baudrate = BAUD38400;
    else if(ui->cb_gps_baudrate->currentIndex() == 4)
        settings.baudrate = BAUD57600;
    else if(ui->cb_gps_baudrate->currentIndex() == 5)
        settings.baudrate = BAUD115200;
#else
    settings.baudrate = 0;
#endif
}

/* gps settings helper function to assign correct values to UI */
void MainWindow::setGpsSettings(gps_settings_t &settings)
{
    ui->label_gpsd_host->setText(settings.host);
    ui->label_gpsd_port->setText(settings.port);

    ui->cb_gps_portname->setCurrentIndex(0);
    /* fill port name combobox with available ones from OS */
    ui->cb_gps_portname->clear();
    ui->cb_gps_portname->addItem("AUTO");

#ifdef Q_OS_WIN
    ui->cb_gps_portname->addItems(gps_get_portnames());
    for(int i = 0; i < ui->cb_gps_portname->count(); i++)
    {
        if(settings.portname == ui->cb_gps_portname->itemText(i))
            ui->cb_gps_portname->setCurrentIndex(i);
    }
    ui->cb_gps_databits->setCurrentIndex(settings.databits);
    ui->cb_gps_flow->setCurrentIndex(settings.flow);
    ui->cb_gps_parity->setCurrentIndex(settings.parity);

    if(settings.baudrate == BAUD4800)
        ui->cb_gps_baudrate->setCurrentIndex(0);
    else if(settings.baudrate == BAUD9600)
        ui->cb_gps_baudrate->setCurrentIndex(1);
    else if(settings.baudrate == BAUD19200)
        ui->cb_gps_baudrate->setCurrentIndex(2);
    if(settings.baudrate == BAUD38400)
        ui->cb_gps_baudrate->setCurrentIndex(3);
    if(settings.baudrate == BAUD57600)
        ui->cb_gps_baudrate->setCurrentIndex(4);
    if(settings.baudrate == BAUD115200)
        ui->cb_gps_baudrate->setCurrentIndex(5);
#endif
}

