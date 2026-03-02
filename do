#!/bin/bash

PROG_NAME="./new_lebench"

#check -ne flag
if [ "$1" == "-ne" ]; then
    PROG_NAME="./sym_no_elevate"
fi
#check -sc flag
if [ "$1" == "-sc" ]; then
    PROG_NAME="./sym_sc"
fi

if [ "$1" == "-sc-stat" ]; then
    PROG_NAME="./sym_lebench_static"
fi

COMMAND="sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH $PROG_NAME"
# COMMAND="sudo LD_DEBUG=all LD_BIND_NOW=1 LD_LIBRARY_PATH=$LD_LIBRARY_PATH $PROG_NAME"

echo $COMMAND
$COMMAND