TEMPLATE = lib
CONFIG += plugin static

PROTOS = ../../utils/osm/osmformat.proto ../../utils/osm/fileformat.proto
include(../../utils/osm/protobuf.pri)

PRE_TARGETDEPS += osmformat.pb.h fileformat.pb.h osmformat.pb.cc fileformat.pb.cc

INCLUDEPATH += ../..

unix {
CONFIG += link_pkgconfig
PKGCONFIG += protobuf
}

HEADERS += osmimporter.h \
	 statickdtree.h \
	 ../../interfaces/iimporter.h \
	 ../../utils/coordinates.h \
	 ../../utils/config.h \
	 bz2input.h \
	 ../../utils/intersection.h \
	 ../../utils/qthelpers.h \
	 ../../utils/osm/ientityreader.h \
	 ../../utils/osm/pbfreader.h \
	 ../../utils/osm/types.h
SOURCES += osmimporter.cpp \
	 types.cpp
DESTDIR = ../../bin/plugins_preprocessor
unix {
	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE += -O3 -Wno-unused-function
	QMAKE_CXXFLAGS_DEBUG += -Wno-unused-function
}

win32 {
	INCLUDEPATH += ../../../lib/win64/protobuf/include
	INCLUDEPATH += ../../../lib/win64/zlib/include
	INCLUDEPATH += ../../../lib/win64/bzip2/include
}

RESOURCES += \
	 speedprofiles.qrc

!nogui {
	SOURCES += oisettingsdialog.cpp \
		speedprofiledialog.cpp \
		waymodificatorwidget.cpp \
		nodemodificatorwidget.cpp \
		highwaytypewidget.cpp
	HEADERS += highwaytypewidget.h \
		speedprofiledialog.h \
		oisettingsdialog.h \
		waymodificatorwidget.h \
		nodemodificatorwidget.h
	FORMS += oisettingsdialog.ui \
		waymodificatorwidget.ui \
		nodemodificatorwidget.ui \
		highwaytypewidget.ui \
		speedprofiledialog.ui
}
nogui {
	DEFINES += NOGUI
	QT -= gui
}
