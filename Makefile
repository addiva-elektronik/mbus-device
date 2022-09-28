# Very basic Makefile, requires libmbus to be installed and reachable
# via the pkg-config interface (the .pc file distributed with libmbus)
EXEC   = mbus-device
OBJS   = mbus-device.o
CFLAGS = $(shell pkg-config --cflags libmbus)
LDLIBS = $(shell pkg-config --libs libmbus)

all: $(EXEC)

$(EXEC): $(OBJS)

.PHONY: clean
clean:
	$(RM) $(OBJS) $(EXEC)

.PHONY: all
distclean: clean
	$(RM) *~ *.bak *.o
