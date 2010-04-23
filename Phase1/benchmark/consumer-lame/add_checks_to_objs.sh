#!/bin/bash
# A script to add bounds checks to all of the objects compiled here, because
# for whatever reason llvm-ld removes the annotations used to create the bounds
# checks.

for f in $@; do
    opt -f -load ../../phase1.so -BoundChecks -OptimizeChecks -InsertChecks \
    -o $f.checked -stats $f
done
