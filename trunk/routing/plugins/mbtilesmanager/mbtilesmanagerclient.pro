TEMPLATE = lib
CONFIG += plugin static

INCLUDEPATH += ../.. ../../../

DESTDIR = ../../bin/plugins_client
unix {
	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE += -O3 \
		 -Wno-unused-function
	QMAKE_CXXFLAGS_DEBUG += -Wno-unused-function
}
HEADERS += mbtilesmanagerclient.h \
	 interfaces/itilemanager.h
SOURCES += mbtilesmanagerclient.cpp


