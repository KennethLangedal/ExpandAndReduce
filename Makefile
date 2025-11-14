SHELL = /usr/bin/env bash

CC ?= gcc
override CFLAGS += -std=gnu17 -O3 -I include -g

OBJ_SHARED = graph.o cliques.o

OBJ_MWIS = main.o $(OBJ_SHARED)
OBJ_MWIS := $(addprefix bin/, $(OBJ_MWIS))

OBJ_VIS = main_vis.o sreen.o barnes_hut.o $(OBJ_SHARED)
OBJ_VIS := $(addprefix bin/, $(OBJ_VIS))

DEP = $(OBJ_MWIS) $(OBJ_VIS)
DEP := $(sort $(DEP))

vpath %.c src
vpath %.h include

all : MWIS VIS

-include $(DEP:.o=.d)

MWIS : $(OBJ_MWIS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

VIS : $(OBJ_VIS)
	$(CC) $(CFLAGS) -o $@ $^ `sdl2-config --cflags --libs` -lm

bin/%.o : %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

.PHONY : clean
clean :
	rm -f MWIS VIS $(DEP) $(DEP:.o=.d)