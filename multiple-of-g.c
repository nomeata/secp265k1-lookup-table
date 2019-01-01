#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#define HAVE_CONFIG_H 1
#include "secp256k1.h"
#include "scalar_impl.h"
#include "num_impl.h"
#include "field_impl.h"
#include "group_impl.h"
#include "ecmult_impl.h"
#include "scratch_impl.h"
#pragma GCC diagnostic pop

void print_hex(unsigned char buf[]){
  int i;
  for (i = 0; i < 32; i++)
  {
      printf("%02x", buf[i]);
  }
  printf("\n");
}

char *map;

int main (int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: %s --binary n1 n2...\n", argv[0]);
    exit(1);
  }

  bool binary = !strcmp(argv[1],"--binary");
  int i = binary ? 2 : 1;

  for (; i < argc; i++) {
    unsigned long n = atol(argv[i]);

    // endianness
    unsigned char n_b[32];
    for (int i = 0; i<32; i++) {n_b[i] = 0; }
    // needs to be big endian (intel is little endian)
    *(uint64_t*)(&n_b[(256 - 64)/8]) = htobe64(n);

    secp256k1_scalar n_s;
    secp256k1_scalar_set_b32(&n_s, n_b, NULL);

    secp256k1_scalar null_s;
    secp256k1_scalar_set_int(&null_s, 0);

    secp256k1_gej g;
    secp256k1_gej_set_ge(&g, &secp256k1_ge_const_g);

    unsigned char pub[32];

    secp256k1_ecmult_context ctx;
    secp256k1_ecmult_context_init(&ctx);

    secp256k1_gej xj;
    secp256k1_ge x;
    // set x to G*n
    secp256k1_ecmult(&ctx, &xj, &g, &n_s, &null_s);
    secp256k1_ge_set_gej(&x, &xj);
    secp256k1_fe_normalize(&(x.x));
    secp256k1_fe_get_b32(pub, &(x.x));
    if (binary) {
      fwrite(&pub[0],1, 32,stdout);
    } else {
      print_hex(pub);
    }
  }
}
