#!/bin/bash

# script used to boot a spinnaker board
# syntax: ./boot.sh <spinnaker board address>

#FILENAME=boot-48.ybug
FILENAME=boot.ybug
echo $1

./ybug $1 << EOF
@ ./$FILENAME
EOF
