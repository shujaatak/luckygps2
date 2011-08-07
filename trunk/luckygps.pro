TEMPLATE = subdirs

INCLUDEPATH += ./routing

SUBDIRS = routing/plugins/client_plugins.pro
SUBDIRS += routing/plugins/preprocessor_plugins.pro

# build must be last:
CONFIG += ordered
SUBDIRS += luckygps-client.pro
