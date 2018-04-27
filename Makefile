
BIN   	= tb
SRC 	= tb.c

%.c: %.rl
	ragel $<

PKG	+=
CFLAGS  += -Wall -Werror -Wno-unused-const-variable
CFLAGS	+= -O3 -MMD
CFLAGS	+= -g
LDFLAGS += -g

CFLAGS	+= $(shell pkg-config --cflags $(PKG))
LDFLAGS	+= $(shell pkg-config --libs $(PKG))

CROSS	=
OBJS    = $(subst .c,.o, $(SRC))
DEPS    = $(subst .c,.d, $(SRC))
CC 	= $(CROSS)gcc
LD 	= $(CROSS)gcc

$(BIN):	$(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(BIN) core main.c

-include $(DEPS)
