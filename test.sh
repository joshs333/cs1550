#!/bin/bash
g++ -Wall -Werror --std=c++11 vmsim.cpp -o vmsim_prog

if (($# == 0)); then
    ./vmsim_prog -n 2 -a opt test/test.trace
else
    ./vmsim_prog $@
fi
