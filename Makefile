# Very basic Makefile, requires libmbus to be installed and reachable
# via the pkg-config interface (the .pc file distributed with libmbus)
EXEC         := mbus-device
OBJS         := mbus-device.o
CFLAGS       += $(shell pkg-config --cflags libmbus)
LDLIBS       += $(shell pkg-config --libs libmbus)
PREFIX       ?= /usr/local

all: $(EXEC)

$(EXEC): $(OBJS)

.PHONY: clean
clean:
	$(RM) $(OBJS) $(EXEC)

.PHONY: all
distclean: clean
	$(RM) *~ *.bak *.o

.PHONY: install
install: all
	install -D $(EXEC) $(DESTDIR)/$(PREFIX)/bin/$(EXEC)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)/$(PREFIX)/bin/$(EXEC)
