# -------------------------------------------------
# Project created by QtCreator 2009-11-03T16:33:44
# -------------------------------------------------
QT += network \
	xml \
	opengl
TARGET = luckygps
TEMPLATE = app
SOURCES += main.cpp \
	mainwindow.cpp \
	tabs_update.cpp \
	mapwidget.cpp \
	tilemanagement.cpp \
	convertunits.cpp \
	tiledownload.cpp \
	mapnikthread.cpp \
	sqlite3.c \
	settings_update.cpp \
	customwidgets.cpp \
	import_export.cpp \
	system_helpers.cpp \
	nmea0183.cpp \
	plugin_helpers.cpp \
    routing.cpp \
    glmapwidget.cpp
HEADERS += mainwindow.h \
	mapwidget.h \
	tilemanagement.h \
	convertunits.h \
	tiledownload.h \
	mapnikthread.h \
	tile.h \
	sqlite3.h \
	customwidgets.h \
	track.h \
	import_export.h \
	route.h \
	plugin_helpers.h \
	system_helpers.h \
	nmea0183.h \
	gpsd.h \
    routing.h \
    route_description.h \
    glmapwidget.h
FORMS += mainwindow.ui
# CODECFORTR = UTF-8
TRANSLATIONS =	luckygps_de.ts \
				luckygps_hu.ts

DEFINES +=	SQLITE_ENABLE_RTREE=1 \
			SQLITE_ENABLE_FTS3

INCLUDEPATH += ./routing
LIBS += -L./routing/bin/plugins_client -lcontractionhierarchiesclient -lgpsgridclient -lunicodetournamenttrieclient -lmbtilesmanagerclient \
		-L./routing/bin/plugins_preprocessor -losmimporter -lcontractionhierarchies -lgpsgrid -lunicodetournamenttrie

win32 {
	SOURCES += gpsd_win.cpp
	RC_FILE = luckygps.rc

	!contains(QMAKE_HOST.arch, x86_64) {
		SYSLIBPATH = win32
	} else {
		SYSLIBPATH = win64
	}

	CONFIG( debug, debug|release )  {

		CONFIG += console

		# geos debug lib
		# LIBS += $$PWD/lib/$$SYSLIBPATH/geos/lib/geosd_c_i.lib

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
		# LIBS += $$PWD/lib/$$SYSLIBPATH/geos/lib/geos_c_i.lib

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
	# INCLUDEPATH += $$PWD/lib/$$SYSLIBPATH/geos/include

	# INCLUDEPATH += $$PWD/lib/$$SYSLIBPATH/crashrpt/include

	LIBS += $$PWD/lib/$$SYSLIBPATH/zlib/lib/zlib.lib
	LIBS += $$PWD/lib/$$SYSLIBPATH/bzip2/lib/libbz2.lib
}

linux-g++ {
	SOURCES += gpsd_linux.cpp

	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE += -O3 \
		 -Wno-unused-function

	# Disabled at the moment
	# geos lib for linux* platform
	# INCLUDEPATH += /usr/include/geos
	# -lgeos_c -lproj

	LIBS += -lprotobuf -lgomp -lbz2
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
