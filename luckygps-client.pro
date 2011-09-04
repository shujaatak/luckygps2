# -------------------------------------------------
# Project created by QtCreator 2009-11-03T16:33:44
# -------------------------------------------------
QT += network \
        xml
TARGET = luckygps
TEMPLATE = app
SOURCES += ./src/main.cpp \
	./src/mainwindow.cpp \
	./src/tabs_update.cpp \
	./src/mapwidget.cpp \
	./src/convertunits.cpp \
	./src/tileHttpDownload.cpp \
	./src/mapnikthread.cpp \
	./src/settings_update.cpp \
	./src/customwidgets.cpp \
	./src/import_export.cpp \
	./src/system_helpers.cpp \
	./src/nmea0183.cpp \
	./src/plugin_helpers.cpp \
	./src/routing.cpp \
	./src/spatialite.c \
	./src/sqlite3.c \
	./src/datasourcemanager.cpp \
	./src/datasource.cpp \
	./src/filetilemanager.cpp \
	src/sqliteTileManager.cpp \
    src/osmadressmanager.cpp
HEADERS += ./src/mainwindow.h \
	./src/mapwidget.h \
	./src/convertunits.h \
	./src/tileHttpDownload.h \
	./src/mapnikthread.h \
	./src/tile.h \
	./src/sqlite3.h \
	./src/customwidgets.h \
	./src/track.h \
	./src/import_export.h \
	./src/route.h \
	./src/plugin_helpers.h \
	./src/system_helpers.h \
	./src/nmea0183.h \
	./src/gpsd.h \
	./src/routing.h \
	./src/route_description.h \
	./src/datasourcemanager.h \
	./src/datasource.h \
    src/filetilemanager.h \
    src/sqliteTileManager.h \
    src/osmadressmanager.h
FORMS += ./src/mainwindow.ui
# CODECFORTR = UTF-8
TRANSLATIONS =	./translations/luckygps_de.ts \
				./translations/luckygps_hu.ts

DEFINES +=	SQLITE_ENABLE_RTREE=1 \
			SQLITE_ENABLE_FTS4 OMIT_PROJ=1

INCLUDEPATH += ./routing ./src
LIBS += -L./routing/bin/plugins_client -lcontractionhierarchiesclient -lgpsgridclient -lunicodetournamenttrieclient \
		-L./routing/bin/plugins_preprocessor -losmimporter -lcontractionhierarchies -lgpsgrid -lunicodetournamenttrie

win32 {
	SOURCES += ./src/gpsd_win.cpp
	RC_FILE = luckygps.rc

	!contains(QMAKE_HOST.arch, x86_64) {
		SYSLIBPATH = win32
	} else {
		SYSLIBPATH = win64
	}

	CONFIG( debug, debug|release )  {

		CONFIG += console

		# geos debug lib
		LIBS += $$PWD/lib/$$SYSLIBPATH/geos/lib/geosd_c_i.lib

		# qextserialport debug lib
		LIBS += $$PWD/lib/$$SYSLIBPATH/qextserialport/lib/qextserialportd1.lib

		# protobuf debug lib
		LIBS += $$PWD/lib/$$SYSLIBPATH/protobuf/lib/libprotobufd.lib
	} else {

		# DEFINES += QT_NO_DEBUG_OUTPUT
		CONFIG += console

		# avoid msvc redist dependancy
		QMAKE_CXXFLAGS_RELEASE -= /O2 -O2
		QMAKE_CXXFLAGS_RELEASE += /Zi /Od
		LIBS += /NODEFAULTLIB:libcmt.lib /DEBUG /pdb:"Release/luckygps.pdb"

		# geos lib
		LIBS += $$PWD/lib/$$SYSLIBPATH/geos/lib/geos_c_i.lib

		# qextserialport lib
		LIBS += $$PWD/lib/$$SYSLIBPATH/qextserialport/lib/qextserialport1.lib

		# protobuf lib
		LIBS += $$PWD/lib/$$SYSLIBPATH/protobuf/lib/libprotobuf.lib

		# CrashRpt lib
		# LIBS += $$PWD/lib/$$SYSLIBPATH/crashrpt/lib/CrashRpt.lib
	}

	# iconv lib
	LIBS += $$PWD/lib/$$SYSLIBPATH/iconv/lib/iconv.lib
	INCLUDEPATH += $$PWD/lib/$$SYSLIBPATH/iconv/include

	# proj lib
	# LIBS += $$PWD/lib/$$SYSLIBPATH/proj/lib/proj.lib
	# INCLUDEPATH += $$PWD/lib/$$SYSLIBPATH/proj/include

	# qextserialport lib include path
	INCLUDEPATH += $$PWD/lib/$$SYSLIBPATH/qextserialport/include

	# geos lib include path
	INCLUDEPATH += $$PWD/lib/$$SYSLIBPATH/geos/include

	# INCLUDEPATH += $$PWD/lib/$$SYSLIBPATH/crashrpt/include

	LIBS += $$PWD/lib/$$SYSLIBPATH/zlib/lib/zlib.lib
	LIBS += $$PWD/lib/$$SYSLIBPATH/bzip2/lib/libbz2.lib
}

linux-g++ {
	SOURCES += ./src/gpsd_debug.cpp
	# ./src/gpsd_linux.cpp

	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE += -O3 -Wno-unused-function
	# `GraphicsMagick++-config --cppflags --cxxflags --ldflags --libs`
	# QMAKE_CXXFLAGS_DEBUG += `GraphicsMagick++-config --cppflags --cxxflags --ldflags --libs`

	DEFINES += WITH_MAPNIK=1 GPS_DEBUG=1
	# Disable Graphicsmagick for now until libpng bugs/Crashes are solved (png.c 1369) --> WITH_IMAGEMAGICK=1

	# Disabled at the moment
	# geos lib for linux* platform
	# INCLUDEPATH += /usr/include/geos

	LIBS += -lprotobuf -lgomp -lbz2 -lgeos_c -lproj -lmapnik
	# `GraphicsMagick++-config --cppflags --libs`
	desktop.path += /usr/share/applications
	desktop.files += ./luckygps.desktop

	# TODO: Need to update xpm icon from .ico file
	icon.path += /usr/share/pixmaps
	icon.files += icons/*.xpm

	# data.path is also hardcoded into main.cpp :(
	# At the moment only language file are in there
	data.path += /usr/share/luckygps
	data.files += ./*.qm

	INSTALLS += desktop
	INSTALLS += icon
	INSTALLS += data

	target.path += /usr/bin/
}

INSTALLS += target
RESOURCES += luckygps.qrc
