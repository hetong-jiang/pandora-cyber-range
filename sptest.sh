#!/bin/bash

inputFilePath=$(realpath $1)

# clear past work
rm -rf /dev/shm/work

# use fuzzer to fuzz the legit_00003
python -m phuzzer -d 6 -C $inputFilePath

# use the python script to export PoV
python ./userex.py







