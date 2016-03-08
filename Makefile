# Objects to link together - Make knows how to make .o from .c
OBJ=engMQTTClient.o decoder.o dev_HRF.o cJSON.o

# Libraries to link in to the final executable
LDLIBS:=$(LDLIBS) -llog4c -lbcm2835 -lmosquitto -lm

# Name of our application
APP_NAME=engMQTTClient

CFLAGS=-Wall

$(APP_NAME): $(OBJ)

engMQTTClient.o: engMQTTClient.c engMQTTClient.h dev_HRF.h  OpenThings.h cJSON.h

dev_HRF.o: dev_HRF.c dev_HRF.h decoder.h OpenThings.h

decoder.o: decoder.c decoder.h

cJSON.o: cJSON.c cJSON.h

clean:
	rm $(OBJ) $(APP_NAME)
