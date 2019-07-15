# REDACTED
# REDACTED
# REDACTED

# Replace these targets with your target (hex file) name, including the .hex part.

TARGETS = \
	Assignment.hex \


# Set the name of the folder containing libcab202_teensy.a
CAB202_TEENSY_FOLDER=../Software/teensy

# Set the name of the folder containing usb_serial.o
USB_SERIAL_FOLDER =../Software/usb_serial
USB_SERIAL_OBJ =../Software/usb_serial/usb_serial.o

# Set the name of the folder containing uart.o
ADC_FOLDER =../Software/cab202_adc
ADC_OBJ =../Software/cab202_adc/cab202_adc.o

UAR_FOLDER =../Software/uart
UAR_OBJ =../Software/uart/uart.o

# ---------------------------------------------------------------------------
#	Leave the rest of the file alone.
# ---------------------------------------------------------------------------

all: $(TARGETS)

TEENSY_LIBS = $(USB_SERIAL_OBJ) $(ADC_OBJ) $(UAR_OBJ) -lcab202_teensy -lprintf_flt -lm 

TEENSY_DIRS =-I$(CAB202_TEENSY_FOLDER) -L$(CAB202_TEENSY_FOLDER) \
	-I$(USB_SERIAL_FOLDER) -I$(ADC_FOLDER) -I$(UAR_FOLDER)

TEENSY_FLAGS = \
	-std=gnu99 \
	-mmcu=atmega32u4 \
	-DF_CPU=8000000UL \
	-funsigned-char \
	-funsigned-bitfields \
	-ffunction-sections \
	-fpack-struct \
	-fshort-enums \
	-Wall \
	-Werror \
	-Wl,-u,vfprintf \
	-Os 

clean:
	for f in $(TARGETS); do \
		if [ -f $$f ]; then rm $$f; fi; \
		if [ -f $$f.elf ]; then rm $$f.elf; fi; \
		if [ -f $$f.obj ]; then rm $$f.obj; fi; \
	done

rebuild: clean all

%.hex : %.c
	avr-gcc $< $(TEENSY_FLAGS) $(TEENSY_DIRS) $(TEENSY_LIBS) -o $@.obj
	avr-objcopy -O ihex $@.obj $@
