
CC =	gcc
LD =	ld
LINK =	$(CC)
CFLAGS = -O3 -m64 -std=gnu99 -Wall
LFLAGS = -lrt

BUILD = build
TMP = tmp
SRC = src
BIN_PREF = sqam_

all: fifo2mq mq2db

fifo2mq: $(TMP)/fifo2mq.o
	$(LINK) $(LFLAGS) -o $(BUILD)/$(BIN_PREF)fifo2mq \
	$(TMP)/fifo2mq.o

mq2db: $(TMP)/mq2db.o $(TMP)/mongo.o
	$(LINK) $(LFLAGS) -o $(BUILD)/$(BIN_PREF)mq2db \
	$(TMP)/mongo.o $(TMP)/mq2db.o


$(TMP)/fifo2mq.o: $(SRC)/config.h $(SRC)/fifo2mq/fifo2mq.c
	$(CC) -c $(CFLAGS) -I$(SRC)/ \
	-o $(TMP)/fifo2mq.o \
	$(SRC)/fifo2mq/fifo2mq.c

$(TMP)/mq2db.o: $(SRC)/config.h $(SRC)/mq2db/mq2db.c
	$(CC) -c $(CFLAGS) -I$(SRC)/ -I$(SRC)/mq2db/mongo/src \
	-o $(TMP)/mq2db.o \
	$(SRC)/mq2db/mq2db.c


#########################################
#      MongoDB sources
MONGO_SRC := $(wildcard $(SRC)/mq2db/mongo/src/*.c)
MONGO_OBJ := $(addprefix $(TMP)/mongo/,$(notdir $(MONGO_SRC:.c=.o)))

$(TMP)/mongo.o: $(TMP)/mongo $(MONGO_OBJ)
	$(LD) -r $(MONGO_OBJ) -o $(TMP)/mongo.o

$(TMP)/mongo:
	mkdir -p $(TMP)/mongo

$(TMP)/mongo/%.o: $(SRC)/mq2db/mongo/src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

#########################################

clean:
	rm -rf ./$(TMP)/mongo/ ./$(TMP)/*.o

