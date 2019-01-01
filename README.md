secp265k1 lookup table
======================

This directory contains code to create a lookup table for the discrete log in
the secp265k1 curve.

It was written for a small part of the calculations in the paper
*Nonce Sense: Lattice Attacks against Weak ECDSA Signatures in Cryptocurrencies*
by Joachim Breitner and Nadia Heninger.

We share this code here as is, for archival reasons and in case it is useful
for anyone else. But note that this is not polished production-ready code. If
you have questions, please do not heistate to contact Joachim Breitner
<mail@joachim-breitner.de>.

Installation
------------

    git clone --recursive https://github.com/nomeata/secp265k1-lookup-table
    make

Also install `bsort` from https://github.com/pelotoncycle/bsort.

Also run `pip3 install fastecdsa`.

Speed advice
------------

The `lookup` program loads all files using `mmap`, and does lots of random
accesses. This is only performant enough if the files are indeed in memory. It
does wonder to load them into memory first, e.g. using `vmtouch -t -v
/path/to/tables` with the [`vmtouch` tool](https://github.com/hoytech/vmtouch).

Example usage
-------------

This is for a local test run, which finds *n×G* with $n<2^24$:

* Generate a list of the least significant 64 bits, up to 2^20:

      ./enumerate-and-write test/lsb-20-%02hhx-%d.bin 19 0
      ./enumerate-and-write test/lsb-20-%02hhx-%d.bin 19 1

  You can parallize this. The filenames encode the most significant byte.

* Combine the files from the different runs:

       for i in $(seq 0 255); do
         cat test/lsb-20-$(printf "%02hhx" $i)-*.bin > test/lsb-20-$(printf "%02hhx" $i).bin
         rm test/lsb-20-$(printf "%02hhx" $i)-*.bin
       done

* Sort each file:

       for i in $(seq 0 255); do
       bsort -k 8 -r 8 lsb-20-$(printf "%02hhx" $i).bin
       done

* Create an index for each file, for faster lookup:

       for i in $(seq 0 255); do
       ./create-index test/lsb-20-$(printf "%02hhx" $i).bin 24
       done

* Since the first 8 bits are addressed by the file, and the next 24 bit by the
  index, we only need to keep the low 32 bits of each nonce:

       for i in $(seq 0 255); do
       ./keep-low-32 test/lsb-20-$(printf "%02hhx" $i).bin
       rm test/lsb-20-$(printf "%02hhx" $i).bin
       done

* Generate a lookup table for for nonces with $n<2^10$. Again, this can easily
  be parallized.

      ./mk-lookup-table.py 9 $((0 * (1<<9)))
      ./mk-lookup-table.py 9 $((1 * (1<<9)))
      mv small-k-rs-9-*.bin > test/small-k-rs-10.bin
      rm small-k-rs-9-*.bin
      bsort -k 32 -r 36 test/small-k-rs-10.bin

* Put the values you want to lookup into a binary files. You will have to code
  that up yourself.  You can look at `hashes-to-r.py` for inspiration. For
  testing, you can generate some random values using

      ./multiple-of-g --binary \
        $(for n in $(seq 0 1023); do echo $(( (RANDOM + RANDOM * (1<<16) + RANDOM * (1<<32)) % (1<<26) )); done) \
        > r-values.bin

* Look up the powers, brute-forcing values up to n<2^24:

      ./lookup 24 lsb-20-%02hhx.bin 20 24 small-k-rs-10.bin 10 r-values.bin r-to-k.bin

