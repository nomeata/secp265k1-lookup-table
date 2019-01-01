#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <stdio.h>
#include <endian.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

int main (int argc, char* argv[]) {
  if (argc < 4) {
    printf("Usage: %s [output%%02hhx-%d.bin] bits start \n", argv[0]);
    exit(1);
  }

  char *filename_pat = argv[1];
  unsigned long bits = atoi(argv[2]);
  unsigned long start = atoi(argv[3]);

  unsigned long start_factor = start << bits;
  unsigned long end = start_factor + (1l<<bits);
  // if (start == 0) start = 1;

  char *filename[265];
  FILE *output[256];
  for (int i=0; i<256; i++){
    asprintf(&filename[i], filename_pat, i, start);
    output[i] = fopen(filename[i], "w");
    if( output[i] == NULL ) { perror(filename[i]); return EXIT_FAILURE; }
  }

  // endianness
  unsigned char start_b[32];
  for (int i = 0; i<32; i++) {start_b[i] = 0; }
  // needs to be big endian (intel is little endian)
  *(uint64_t*)(&start_b[(256 - 64)/8]) = htobe64(start_factor);

  secp256k1_scalar start_s;
  secp256k1_scalar_set_b32(&start_s, start_b, NULL);

  secp256k1_scalar null_s;
  secp256k1_scalar_set_int(&null_s, 0);

  secp256k1_gej g;
  secp256k1_gej_set_ge(&g, &secp256k1_ge_const_g);

  unsigned char pub[32];

  secp256k1_ecmult_context ctx;
  secp256k1_ecmult_context_init(&ctx);

  secp256k1_gej xj;
  secp256k1_ge x;
  // set x to G*start_s
  secp256k1_ecmult(&ctx, &xj, &g, &start_s, &null_s);
  for (unsigned long i = start_factor; i< end; i++){
    secp256k1_ge_set_gej(&x, &xj);
    secp256k1_fe_normalize(&(x.x));
    secp256k1_fe_get_b32(pub, &(x.x));

    // print_hex(pub);
    if (!fwrite(&pub[(256-64)/8], 64/8, 1, output[pub[(256-64)/8]])) break;

    secp256k1_gej_add_var(&xj, &xj, &g, NULL);
  }

  for (int i=0; i<256; i++){
    if (ferror(output[i])) { perror(filename[i]); return EXIT_FAILURE; }
    fclose(output[i]);
  }
}
