#!/usr/bin/python3

import fastecdsa.curve

c = fastecdsa.curve.secp256k1


from tqdm import tqdm

import sys

if len(sys.argv[0])>2:
    bits   = int(sys.argv[1])
    start = int(sys.argv[2])
else:
    bits = 32
    start = 0

f = open('small-k-rs-%d-%016x.bin' % (bits, start),'wb')

end = start + 2**bits

if start == 0: start =1

x = c.G*start;
for i in tqdm(range(start,end)):
    f.write(x.x.to_bytes(32,'big'))
    f.write(i.to_bytes(4,'big'))
    x = x + c.G
