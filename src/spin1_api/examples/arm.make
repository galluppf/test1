# build example x.c with:   make EXAMPLE=x

EXAMPLE = simple
THUMB = no
CFLAGS = -DDEBUG

#LIB_DIR = /home/amulinks/spinnaker/code/lib
#INC_DIR = /home/amulinks/spinnaker/code/include
LIB_DIR = ../src
INC_DIR = ../src


CC := armcc -c --cpu=5te --c99 --apcs /interwork -I $(INC_DIR) $(CFLAGS)
CT := $(CC)
AS := armasm --keep --cpu=5te --apcs /interwork
RM := /bin/rm -f

ifeq ($(THUMB),yes)
  CT += --thumb -DTHUMB
  AS += --pd "THUMB seta 1" 
endif


OBJS = $(EXAMPLE).o


target: $(OBJS) example.sct
	armlink --scatter=example.sct --output $(EXAMPLE).elf $(LIB_DIR)/spin1_api_lib.o $(OBJS)
	fromelf $(EXAMPLE).elf --bin --output $(EXAMPLE).aplx
	fromelf $(EXAMPLE).elf -cds --output $(EXAMPLE).lst

$(EXAMPLE).o: $(EXAMPLE).c
	$(CT) $(EXAMPLE).c

clean:
	$(RM) *.o *.aplx *.elf *.lst *~
	${RM} APLX.bin RO_DATA.bin RW_DATA.bin a.out

tidy:
	$(RM) *.o *.elf *.lst *~
	${RM} APLX.bin RO_DATA.bin RW_DATA.bin a.out
