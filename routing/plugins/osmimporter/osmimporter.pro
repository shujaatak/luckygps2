TEMPLATE = lib
CONFIG += plugin static

INCLUDEPATH += ../..

DEFINES += LIBPROTOBUF
# LIBXML

HEADERS += osmimporter.h \
	 oisettingsdialog.h \
	 statickdtree.h \
	 interfaces/iimporter.h \
	 utils/coordinates.h \
	 utils/config.h \
	 bz2input.h \
	 utils/intersection.h \
	 utils/qthelpers.h \
	 xmlreader.h \
	 ientityreader.h \
	 pbfreader.h \
	 "protobuf_definitions/osmformat.pb.h" \
	 "protobuf_definitions/fileformat.pb.h" \
	 lzma/Types.h \
	 lzma/LzmaDec.h \
	 waymodificatorwidget.h \
	 nodemodificatorwidget.h \
	 types.h \
	 highwaytypewidget.h
SOURCES += osmimporter.cpp \
	 oisettingsdialog.cpp \
	 "protobuf_definitions/osmformat.pb.cc" \
	 "protobuf_definitions/fileformat.pb.cc" \
	 lzma/LzmaDec.c \
	 waymodificatorwidget.cpp \
	 nodemodificatorwidget.cpp \
	 highwaytypewidget.cpp
DESTDIR = ../../bin/plugins_preprocessor
FORMS += oisettingsdialog.ui \
	 waymodificatorwidget.ui \
	 nodemodificatorwidget.ui \
	 highwaytypewidget.ui
unix {
	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE += -O3 -march=native -Wno-unused-function
	QMAKE_CXXFLAGS_DEBUG += -Wno-unused-function
}

win32 {
	INCLUDEPATH += ../../../lib/win64/protobuf/include
	INCLUDEPATH += ../../../lib/win64/zlib/include
	INCLUDEPATH += ../../../lib/win64/bzip2/include
}

RESOURCES += \
	 speedprofiles.qrc
