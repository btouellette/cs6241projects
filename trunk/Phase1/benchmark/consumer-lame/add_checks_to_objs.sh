#!/bin/bash
# A script to add bounds checks to all of the objects compiled here, because
# for whatever reason llvm-ld removes the annotations used to create the bounds
# checks.

PASSES="-BoundChecks -OptimizeChecks -InsertChecks"
# PASSES="-InsertChecks"

for f in $@; do
    opt -f -load ../../phase1.so $PASSES \
    -o $f.checked -stats $f
done
