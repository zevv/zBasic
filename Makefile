
BIN   	= tb
SRC 	= main.c

%.c: %.rl Makefile
	ragel -T1 $<
	ragel -Vp $< -Mmain | dot -Tjpg > tb.jpg

PKG	+=
CFLAGS  += -Wall -Werror -Wno-unused-const-variable -Wno-unused-variable
CFLAGS	+= -Os -MMD
CFLAGS	+= -g
LDFLAGS += -g -lm

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
	rm -f $(OBJS) $(BIN) core main.c $(BIN).jpg

-include $(DEPS)
