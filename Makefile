CC        = gcc
CFLAGS    = -g -O2 -Wall
XROOT     = /usr/X11R6
INCLUDES  = -I$(XROOT)/include
LIBS      = -lX11 -lXext -lXpm 
LDPATH    = -L$(XROOT)/lib

DESTDIR   =
PROG      = desklaunch
PREFIX    = $(DESTDIR)/usr
RCFILE    = .$(PROG)rc
DEFINES   = -DRCFILE=\"$(RCFILE)\" #-DDEBUG

OBJS      = $(PROG).o  

all: $(PROG) 

$(PROG): $(OBJS)
	$(CC) $(OBJS) $(LDPATH) $(LIBS) -o $@

$(OBJS): %.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

install: all
	install $(PROG) $(PREFIX)/bin
	install -d $(PREFIX)/share/$(PROG)

uninstall:
	rm -f $(PREFIX)/bin/$(PROG)
	rm -rf $(PREFIX)/share/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)
