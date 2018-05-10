
BIN   	= ./zbasic
SRC 	= main.c

%.c: %.rl Makefile
	ragel -T1 $<
	ragel -Vp $< -Mmain | dot -Tjpg > tb.jpg

PKG	+=
CFLAGS  += -Wall -Werror -Wextra -pedantic 
CFLAGS  += -Wno-unused-parameter -Wno-unused-const-variable -Wno-unused-variable -Wno-clobbered
CFLAGS	+= -Os -MMD
CFLAGS	+= -g
LDFLAGS += -g -lm

#CFLAGS += -fno-omit-frame-pointer

CFLAGS	+= $(shell pkg-config --cflags $(PKG))
LDFLAGS	+= $(shell pkg-config --libs $(PKG))

CROSS	=
OBJS    = $(subst .c,.o, $(SRC))
DEPS    = $(subst .c,.d, $(SRC))
CC 	= $(CROSS)gcc
LD 	= $(CROSS)gcc

$(BIN):	$(OBJS) Makefile
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

test: $(BIN) basic/test.bas
	$(BIN) < basic/test.bas
	
clean:
	rm -f $(OBJS) $(BIN) core $(BIN).jpg

size: $(BIN)
	nm main.o  -S --size-sort |grep " [tTdD] " && size main.o

flame:
	perf script | ~/external/FlameGraph/./stackcollapse-perf.pl > /tmp/folded
	~/external/FlameGraph/flamegraph.pl /tmp/folded > /tmp/out.svg

-include $(DEPS)
