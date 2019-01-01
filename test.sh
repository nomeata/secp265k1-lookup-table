#!/bin/bash

set -e

make

rm -rf test
mkdir test

./enumerate-and-write test/lsb-20-%02hhx-%d.bin 19 0
./enumerate-and-write test/lsb-20-%02hhx-%d.bin 19 1
for i in $(seq 0 255); do
  cat test/lsb-20-$(printf "%02hhx" $i)-*.bin > test/lsb-20-$(printf "%02hhx" $i).bin
  rm test/lsb-20-$(printf "%02hhx" $i)-*.bin
done
for i in $(seq 0 255); do
bsort -k 8 -r 8 test/lsb-20-$(printf "%02hhx" $i).bin
done
for i in $(seq 0 255); do
./create-index test/lsb-20-$(printf "%02hhx" $i).bin 24
done
for i in $(seq 0 255); do
./keep-low-32 test/lsb-20-$(printf "%02hhx" $i).bin
rm test/lsb-20-$(printf "%02hhx" $i).bin
done
./mk-lookup-table.py 9 $((0 * (1<<9)))
./mk-lookup-table.py 9 $((1 * (1<<9)))
cat small-k-rs-9-*.bin > test/small-k-rs-10.bin
rm small-k-rs-9-*.bin
bsort -k 32 -r 36 test/small-k-rs-10.bin

# ./hashes-to-r.py --output r-values.bin .../hashes.log-2
./multiple-of-g --binary \
    $(for n in $(seq 0 1023); do echo $(( (RANDOM + RANDOM * (1<<16) + RANDOM * (1<<32)) % (1<<26) )); done) \
	> test/r-values.bin
./lookup 24 test/lsb-20-%02hhx.bin 20 24 test/small-k-rs-10.bin 10 test/r-values.bin test/r-to-k.bin |& tail -n 1

