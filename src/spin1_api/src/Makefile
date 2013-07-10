#
# Makefile for spin1_api using ARM tools
#

THUMB = no
CTIME = $(shell perl -e "print time")

LIB_DIR = .
INC_DIR = .


CC := armcc -c --cpu=5te --c99 --apcs /interwork
CT := $(CC)
AS := armasm --keep --cpu=5te --apcs /interwork

ifeq ($(THUMB),yes)
  CT += --thumb -DTHUMB
  AS += --pd "THUMB seta 1"
endif


API_OBJS = sark_init.o sark.o spin1_isr.o spin1_api.o spinn_io.o


lib:    $(API_OBJS)
	armlink --partial $(API_OBJS) --output spin1_api_lib.o

sark_init.o: spinnaker.h sark_init.s
	../../../tools/h2asm $(INC_DIR)/spinnaker.h > spinnaker.s
	$(AS) --pd "SARK_API seta 1" sark_init.s

sark.o: sark.c $(INC_DIR)/spinnaker.h spin1_api.h spin1_api_params.h
	$(CT) -DSARK_API -DCTIME=$(CTIME) sark.c

spin1_api.o: spin1_api.c spinnaker.h spin1_api.h spinn_io.h spin1_api_params.h
	$(CT) spin1_api.c

spin1_isr.o: spin1_isr.c spin1_api.h spin1_api_params.h spinnaker.h
	$(CC) spin1_isr.c

spinn_io.o: spinn_io.c spinn_io.h spinnaker.h
	$(CT) spinn_io.c

spin1_api_lib.o: $(API_OBJS)
	armlink --partial $(API_OBJS) --output spin1_api_lib.o

clean:
	rm -f $(API_OBJS) spinnaker.s spinnaker.gas sark_init.gas *~
