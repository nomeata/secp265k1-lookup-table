all: enumerate-and-write multiple-of-g lookup split-first-byte create-index keep-low-32

%: %.c secp256k1/src/libsecp256k1-config.h
	gcc -Wconversion -Wno-sign-conversion $(CFLAGS) -O3 -lrt -g -lgmp  -I. -Isecp256k1/src $< -o $@

secp256k1/src/libsecp256k1-config.h:
	cd secp256k1; ./autogen.sh
	cd secp256k1; ./configure

