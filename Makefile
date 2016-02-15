# Objects to link together - Make knows how to make .o from .c
OBJ=engMQTTClient.o decoder.o dev_HRF.o

# Libraries to link in to the final executable
LDLIBS:=$(LDLIBS) -llog4c -lbcm2835 -lmosquitto

# Name of our application
APP_NAME=engMQTTClient

CFLAGS=-Wall

$(APP_NAME): $(OBJ)

clean:
	rm $(OBJ) $(APP_NAME)
