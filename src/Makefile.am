bin_PROGRAMS = oscope2100


oscope2100_SOURCES = oscope2100_common.cpp oscope2100_common.h ConfigFile.cpp ConfigFile.h oscope2100_interface.cpp oscope2100_interface.h oscope2100_usb.cpp oscope2100_usb.h
#oscope2100_LDFLAGS = $(GTK_LIBS)
oscope2100_LDADD= -lusb -lpthread $(GTK_LIBS)

AM_CPPFLAGS = $(GTK_CFLAGS)




