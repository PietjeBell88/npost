.PHONY: default clean

default: npost

all: default

SRCS =  common.c diskfile.c yenc.c socket.c nntp.c npost.c \
	extern/crc32.c

ifneq ($(HAVE_GETOPT_LONG),1)
SRCS += extern/getopt.c
endif

OBJS = $(SRCS:%.c=%.o)

CFLAGS = -Wall -Wextra -Wshadow -std=gnu99 -g
LDFLAGS = -lpthread

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
