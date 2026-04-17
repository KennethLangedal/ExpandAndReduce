SHELL = /usr/bin/env bash

CC ?= gcc
override CFLAGS += -std=gnu17 -fopenmp -O3 -I include -g
override LDFLAGS += -L bin/

OBJ_SHARED = graph.o

OBJ_MWIS = main.o $(OBJ_SHARED)
OBJ_MWIS := $(addprefix bin/, $(OBJ_MWIS))

DEP = $(OBJ_MWIS) $(OBJ_VIS)
DEP := $(sort $(DEP))

vpath %.c src
vpath %.h include

all : MWIS

-include $(DEP:.o=.d)

MWIS : $(OBJ_MWIS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

bin/%.o : %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

.PHONY : clean
clean :
	rm -f MWIS $(DEP) $(DEP:.o=.d)