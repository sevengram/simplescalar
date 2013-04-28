#! /bin/bash
make clean
make config-pisa
make
./sim-pipe-withstall ../../project1/part2/loop 1>~/1.txt 2>&1
