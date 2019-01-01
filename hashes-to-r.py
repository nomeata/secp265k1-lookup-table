#!/usr/bin/python3

import sys
import fileinput

import argparse
parser = argparse.ArgumentParser(description="converts hashes to a binary of r values (256 bits each)")
parser.add_argument("-n", type=int, help="samples", default=100000)
parser.add_argument("--output", "-o", help="file", required=True)
parser.add_argument('files', metavar='FILE', nargs='*', help='files to read, if empty, stdin is used')
args = parser.parse_args()

f = open(args.output,'wb')

for line in fileinput.input(files=args.files if len(args.files) > 0 else ('-', )):
    try:
        if len(line.split()) == 4:
            [data, sig, pubkey_hex, txid] = line.split()
        else:
            [reason, data, sig, pubkey_hex, txid, privkey_hex, nonce] = line.split()
    except ValueError as e:
        sys.stderr.write("Could not unpack line\n%s%s\n" % (line, e))
    else:
        sigb = bytes.fromhex(sig)
        l = sigb[3]
        r = int.from_bytes(sigb[4:4+l],'big')
        f.write(r.to_bytes(32,'big'))
