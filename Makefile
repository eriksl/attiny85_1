BASE		=		/home/erik/avr
LIBDIR		=		$(BASE)
UTSBASE		=		$(LIBDIR)/usitwislave
UTSLDLIB	=		-lusitwislave

MCU			=		attiny861
PROGRAMMER	=		dragon_isp
#PROGRAMMER	=		dragon_pp
PRGFLAGS	=		-b 0 -P usb

PROGRAM		=		twimain
OBJFILES	=		$(PROGRAM).o
HEXFILE		=		$(PROGRAM).hex
ELFFILE		=		$(PROGRAM).elf
PROGRAMMED	=		.programmed
CFLAGS		=		-Wall -Winline -Os -g -mmcu=$(MCU) -DF_CPU=8000000UL -I$(UTSBASE)
LD1FLAGS	=		-Wall -mmcu=$(MCU) -L$(UTSBASE) 
LD2FLAGS	=		$(UTSLDLIB)

.PHONY:				all clean library
.SUFFIXES:
.SUFFIXES:			.c .o .elf .hex
.PRECIOUS:			.c .h

all:				$(PROGRAMMED)

$(PROGRAM).o:		$(PROGRAM).c ioports.h

%.o:				%.c
					avr-gcc -c $(CFLAGS) $< -o $@

$(ELFFILE):			$(OBJFILES)
					avr-gcc $(LD1FLAGS) $(OBJFILES) $(LD2FLAGS) -o $@

$(HEXFILE):			$(ELFFILE)
					avr-objcopy -j .text -j .data -O ihex $< $@

$(PROGRAMMED):		$(HEXFILE)
					sh -c "avrdude -vv -c $(PROGRAMMER) -p $(MCU) $(PRGFLAGS) -U flash:w:$^ > $(PROGRAMMED) 2>&1"

clean:			
					@echo rm $(OBJFILES) $(ELFFILE) $(HEXFILE) $(PROGRAMMED)
					@-rm $(OBJFILES) $(ELFFILE) $(HEXFILE) 2> /dev/null || true
