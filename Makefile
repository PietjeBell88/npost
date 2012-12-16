.PHONY: default clean

default: npost

all: default

SRCS = socket.c nntp.c npost.c

OBJS = $(SRCS:%.c=%.o)

CFLAGS = -Wall -Wextra -Wshadow

# Assuming GCC for now
DEPMM = -MM -g0
DEPMT = -MT
LD = $(CC) -o 

.PHONY: all default clean

npost: .depend $(OBJS)
	$(LD)$@ $(OBJS) $(OBJASM) $(LDFLAGS)

$(OBJS): .depend

.depend:
	@rm -f .depend
	@$(foreach SRC, $(SRCS), $(CC) $(CFLAGS) $(SRC) $(DEPMT) $(SRC:%.c=%.o) $(DEPMM) 1>> .depend;)

depend: .depend

clean:
	rm -f $(OBJS) *.a *.lib npost .depend
