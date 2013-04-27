#! /bin/bash
make clean
make config-pisa
make
./sim-fast ../../project1/part1/tests/test1
