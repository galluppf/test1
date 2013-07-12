#!/bin/bash
echo "Usage: get_outputs.sh SPINN_CHIP_ADDR filename start length X_CHIP_ID Y_CHIP_ID PROC_ID"

MYDIR=$(pwd)

SPINN_CHIP_ADDR=$1

FILENAME=$2
START=$3
LENGTH=$4

XID=$5
YID=$6
PROCID=$7

./ybug $SPINN_CHIP_ADDR  << EOF
sp $XID $YID $PROCID
sdump $FILENAME $START $LENGTH
EOF
