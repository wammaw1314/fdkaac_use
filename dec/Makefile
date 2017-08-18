# Makefile for Lua BitOp -- a bit operations library for Lua 5.1.
# To compile with MSVC please run: msvcbuild.bat

INCLUDES= -I./ -I../../fdk-aac-master/out/include/fdk-aac/
LIB= -L../../fdk-aac-master/out/lib -lfdk-aac
CC= gcc
STRIP=strip
CFLAGS= -Wall -O2 $(INCLUDES)
RM= rm -f


MODNAME= aac-dec

all: $(MODNAME)

$(MODNAME):
	@$(CC) -o $(MODNAME) $(MODNAME).c $(CFLAGS) $(LIB)
	@$(STRIP) $(MODNAME)

clean:
	@$(RM) $(MODNAME) *.o *.so *.obj *.lib *.exp *.dll *.manifest

.PHONY: all clean


