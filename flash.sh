#!/bin/bash
set -e
set -o pipefail

# compile miosix
make -j4

sleep 1

# flash binary on micro
wandstem-flash -m USB -f main.bin

sleep 1

# open serial communication
screen /dev/ttyUSB0 230400 #115200
