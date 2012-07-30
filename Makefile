BASE		=		/home/erik/avr
LIBDIR		=		$(BASE)
UTSBASE		=		$(LIBDIR)/usitwislave
UTSLIBNAME	=		libusitwislave.a
UTSLDLIB	=		-lusitwislave

MCU			=		attiny861
PROGRAMMER	=		dragon_isp
PRGFLAGS	=		-b 0 -P usb

PROGRAM		=		twimain
OBJFILES	=		$(PROGRAM).o
HEXFILE		=		$(PROGRAM).hex
ELFFILE		=		$(PROGRAM).elf
CFLAGS		=		-Wall -Winline -Os -g -mmcu=$(MCU) -DF_CPU=8000000UL -I$(UTSBASE)
LD1FLAGS	=		-Wall -mmcu=$(MCU) -L$(UTSBASE) 
LD2FLAGS	=		$(UTSLDLIB)

.PHONY:				all clean program library
.SUFFIXES:
.SUFFIXES:			.c .o .elf .hex
.PRECIOUS:			.c .h

all:				library program

library:
					$(MAKE) -C $(UTSBASE) $(UTSLIBNAME)

$(PROGRAM).o:		$(PROGRAM).c $(UTSBASE)/usitwislave.h

%.o:				%.c
					avr-gcc -c $(CFLAGS) $< -o $@

$(ELFFILE):			$(OBJFILES) $(UTSBASE)/$(UTSLIBNAME)
					avr-gcc $(LD1FLAGS) $(OBJFILES) $(LD2FLAGS) -o $@

$(HEXFILE):			$(ELFFILE)
					avr-objcopy -j .text -j .data -O ihex $< $@

program:			$(HEXFILE)
					avrdude -qq -c $(PROGRAMMER) -p $(MCU) $(PRGFLAGS) -U flash:w:$^

clean:			
					@echo rm $(OBJFILES) $(ELFFILE) $(HEXFILE)
					$(MAKE) -C $(UTSBASE) clean
					@-rm $(OBJFILES) $(ELFFILE) $(HEXFILE) 2> /dev/null || true
