#!/bin/bash

timeout -k 99d -s INT 1h ./MWIS $1 kernels/$(basename $1 .sh).kernel metas/$(basename $1 .sh).meta