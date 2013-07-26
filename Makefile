
CC =	gcc
LD =	ld
LINK =	$(CC)
CFLAGS = -O3 -m64 -std=gnu99 -Wall
LFLAGS = -lrt

BUILD = build
TMP = tmp
SRC = src
BIN_PREF = sqam_

all: logs2db
logs2db: squid2mq mq2db



squid2mq: $(TMP)/logs2db $(TMP)/logs2db/squid2mq.o $(TMP)/logs2db/utils.o
	$(LINK) $(LFLAGS) -o $(BUILD)/$(BIN_PREF)squid2mq \
	$(TMP)/logs2db/utils.o $(TMP)/logs2db/squid2mq.o

mq2db: $(TMP)/logs2db $(TMP)/logs2db/mq2db.o $(TMP)/logs2db/mongo.o $(TMP)/logs2db/utils.o
	$(LINK) $(LFLAGS) -o $(BUILD)/$(BIN_PREF)mq2db \
	$(TMP)/logs2db/utils.o $(TMP)/logs2db/mongo.o $(TMP)/logs2db/mq2db.o

########################################

$(TMP)/logs2db:
	mkdir -p $(TMP)/logs2db

$(TMP)/logs2db/utils.o: $(SRC)/logs2db/config.h $(SRC)/logs2db/utils.h $(SRC)/logs2db/utils.c
	$(CC) -c $(CFLAGS) -I$(SRC)/logs2db/ \
	-o $(TMP)/logs2db/utils.o \
	$(SRC)/logs2db/utils.c

$(TMP)/logs2db/squid2mq.o: $(SRC)/logs2db/config.h $(SRC)/logs2db/utils.h $(SRC)/logs2db/squid2mq.c
	$(CC) -c $(CFLAGS) -I$(SRC)/logs2db/ \
	-o $(TMP)/logs2db/squid2mq.o \
	$(SRC)/logs2db/squid2mq.c

$(TMP)/logs2db/mq2db.o: $(SRC)/logs2db/config.h $(SRC)/logs2db/utils.h $(SRC)/logs2db/mq2db.c
	$(CC) -c $(CFLAGS) -I$(SRC)/logs2db/ -I$(SRC)/logs2db/mongo/src \
	-o $(TMP)/logs2db/mq2db.o \
	$(SRC)/logs2db/mq2db.c


#########################################
#      MongoDB sources
MONGO_SRC := $(wildcard $(SRC)/logs2db/mongo/src/*.c)
MONGO_OBJ := $(addprefix $(TMP)/logs2db/mongo/,$(notdir $(MONGO_SRC:.c=.o)))

$(TMP)/logs2db/mongo.o: $(TMP)/logs2db/mongo $(MONGO_OBJ)
	$(LD) -r $(MONGO_OBJ) -o $(TMP)/logs2db/mongo.o

$(TMP)/logs2db/mongo:
	mkdir -p $(TMP)/logs2db/mongo

$(TMP)/logs2db/mongo/%.o: $(SRC)/logs2db/mongo/src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

#########################################

clean:
	rm -rf ./$(TMP)/logs2db/

