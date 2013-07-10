#!/bin/bash

# script used to start the simulation of the files in ./binaries
# syntax: ./run.sh <spinnaker board address>

./tubotron -net -log &
ping $1 -c 1 -i 1 -q
./ybug $1 << EOF
@ ../binaries/automatic.ybug
EOF
