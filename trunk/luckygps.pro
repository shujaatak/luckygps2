TEMPLATE = subdirs

INCLUDEPATH += ./routing ./src

SUBDIRS = routing/plugins/client_plugins.pro
SUBDIRS += routing/plugins/preprocessor_plugins.pro

# build must be last:
CONFIG += ordered
SUBDIRS += luckygps-client.pro
