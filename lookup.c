#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <stdio.h>
#include <endian.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

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

void print_hex(uint8_t buf[]){
  int i;
  for (i = 0; i < 32; i++)
  {
      printf("%02x", buf[i]);
  }
  printf("\n");
}

void unsigned_long_to_b32_be(uint8_t *b, unsigned long n) {
  for (int i = 0; i<32; i++) {b[i] = 0; }
  // needs to be big endian (intel is little endian)
  *(uint64_t*)(&b[(256 - 64)/8]) = htobe64(n);
}

void secp256k1_scalar_set_long(secp256k1_scalar *s, unsigned long n) {
  uint8_t b[32];
  unsigned_long_to_b32_be(b, n);
  secp256k1_scalar_set_b32(s, b, NULL);
}

unsigned char first_char = 0x00;
unsigned char last_char = 0xff;
uint32_t *lsb_map[256];
unsigned long bits_idx;
unsigned long *lsb_idx_map[256];
void *lookup_map;
unsigned long lookup_map_size;

bool is_in_char_range(uint64_t lsb) {
  uint8_t msb_byte = (uint8_t)lsb; // This abuses that we are little endian
  return first_char <= msb_byte && msb_byte <= last_char;
}

bool is_in_lsb_map(uint64_t lsb) {
  if (!is_in_char_range(lsb)) return false;

  uint8_t msb_byte = (uint8_t)lsb; // This abuses that we are little endian
  // get the lo and hi from the index
  uint64_t index_i = (be64toh(lsb) >> (64-8-bits_idx)) % (1l<<bits_idx);
  unsigned long lo = lsb_idx_map[msb_byte][index_i];
  unsigned long hi = lsb_idx_map[msb_byte][index_i+1];

  uint32_t ls32 = (uint32_t)(lsb >> 32); // This abuses that we are little endian
  uint32_t *a = lsb_map[msb_byte];
  while (lo < hi) {
    unsigned long mid = (lo + hi) / 2;
    if (be32toh(a[mid]) < be32toh(ls32))
      lo = mid + 1;
    else
      hi = mid;
  }
  return (ls32 == a[lo]);
}

bool is_lt_b32(uint8_t b1[], uint8_t b2[]) {
  for (int i = 0; i<32; i++) {
    if (b1[i] < b2[i]) return true;
    if (b1[i] > b2[i]) return false;
  }
  return false;
}
bool is_eq_b32(uint8_t b1[], uint8_t b2[]) {
  for (int i = 0; i<32; i++) {
    if (b1[i] != b2[i]) return false;
  }
  return true;
}
bool is_zero_b32(uint8_t b1[]) {
  for (int i = 0; i<32; i++) {
    if (b1[i] != 0) return false;
  }
  return true;
}

struct lookup_entry {
   uint8_t     nonce[32];
   uint32_t k;
};

bool is_in_lookup_map(uint8_t b[], unsigned long *r) {
  if (is_zero_b32(b)) {
    *r = 0;
    return true;
  }

  unsigned long lo = 0;
  unsigned long hi = (lookup_map_size / sizeof(struct lookup_entry))+1;
  struct lookup_entry *a = (struct lookup_entry*)lookup_map;

  while (lo < hi) {
    unsigned long mid = (lo + hi) / 2;
    if (is_lt_b32(a[mid].nonce, b))
      lo = mid + 1;
    else
      hi = mid;
  }
  if (is_eq_b32(a[lo].nonce, b)) {
    *r = be32toh(a[lo].k);
    return true;
  } else {
    return false;
  }
}

bool is_in_lookup_map_gej(secp256k1_gej *xj, unsigned long *r) {
  uint8_t b[32];
  if (secp256k1_gej_is_infinity(xj)) { *r = 0; return true; }

  secp256k1_ge x;
  secp256k1_ge_set_gej(&x, xj);
  secp256k1_fe_normalize(&(x.x));
  secp256k1_fe_get_b32(b, &(x.x));

  return is_in_lookup_map(b, r);
}

void sanity_checks(char *lsb_file, char *lookup_file){
  bool fail = false;
  // Some sanity checks
  if (is_in_char_range(0) && !is_in_lsb_map(0)) {
    fprintf(stderr, "Could not find 0x0 in %s\n", lsb_file);
    fail = true;
  }
  if (is_in_char_range(htobe64(0x000009e9bfe2cf10)) && !is_in_lsb_map(htobe64(0x000009e9bfe2cf10))) {
    fprintf(stderr, "Could not find 0x000009e9bfe2cf10 in %s\n", lsb_file);
    fail = true;
  }
  if (is_in_char_range(htobe64(0xfffff21b299fa7d6)) && !is_in_lsb_map(htobe64(0xfffff21b299fa7d6))) {
    fprintf(stderr, "Could not find 0xfffff21b299fa7d6 in %s\n", lsb_file);
    fail = true;
  }
  // Some sanity checks
  unsigned long tmp;

  uint8_t b1[32] = "\x00\x13\x69\x33\x17\x4b\xc3\x88\xa7\x4e\xbd\x67\x46\xe1\x3a\xfe\x0e\xef\x5d\x66\x58\x0c\x8e\x23\xd3\x34\x64\xc3\x42\xdc\x00\x80";
  if (!(is_in_lookup_map(b1,&tmp) && tmp == 0xf6)) {
    fprintf(stderr, "Sanity check 1 for %s failed (r = %d)\n", lookup_file, tmp);
    fail = true;
  }
  uint8_t b2[32] = "\xff\xf9\x7b\xd5\x75\x5e\xee\xa4\x20\x45\x3a\x14\x35\x52\x35\xd3\x82\xf6\x47\x2f\x85\x68\xa1\x8b\x2f\x05\x7a\x14\x60\x29\x75\x56";
  if (!(is_in_lookup_map(b2,&tmp) && tmp == 6)) {
    fprintf(stderr, "Sanity check 2 for %s failed (r = %d)\n", lookup_file, tmp);
    fail = true;
  }

  if (fail) exit(1);
}

int main (int argc, char* argv[]) {
  if (argc < 9) {
    printf("Usage: %s <bits0> <lsb-64%%02hxx.bin> <bits1> <bits_idx> <lookup> <bits2> <input> <output> [<half>]\n", argv[0]);
    printf("<bits0>:  Find r values up to 2^bits0.\n");
    printf("<lsb-64%%02hxx>: The least significant 64 bits of g*n, n<2^bits1, packed,\n");
    printf("          split into one file per significant byte, sorted.");
    printf("          (actually, the .low32 file is used).");
    printf("<bits_idx>: number of bits in the corresponding .idx file.");
    printf("<lookup>: Packed pairs of (g*n,n), n<2^bits2, sorted.\n");
    printf("          (256 bits for g*n, 32bits for n).\n");
    printf("<input>:  Binary file packed with r-values, big endian, 256 bits each.\n");
    printf("<output>: Packed pairs of (g*k,k), unsorted.\n");
    printf("          (256 bits for g*k, 256bits for k).\n");
    printf("[<half>]: absent, 0 or 1. Only use lower/upper half of the lookup map.\n");
    printf("Typically bits0=64 bits1=40 bits_idx=24 bits2=32\n");
    exit(1);
  }

  unsigned long bits0 = atoi(argv[1]);
  char *lsb_file = argv[2];
  unsigned long bits1 = atoi(argv[3]);
  bits_idx = atoi(argv[4]);
  char *lookup_file = argv[5];
  unsigned long bits2 = atoi(argv[6]);
  char *input_file = argv[7];
  char *output_file = argv[8];

  if (argc >= 10) {
    if (atoi(argv[9]) == 0) {
      first_char = 0x00;
      last_char = 0x7f;
    } else if (atoi(argv[9]) == 1) {
      first_char = 0x70;
      last_char = 0xff;
    } else {
      fprintf(stderr, "<half> must be 0 or 1\n");
      exit(1);
    }
  }

  fprintf(stderr, "Parameters: bits0=%d bits1=%d bits_idx=%d bits2=%d chars=%02hhx-%02hhx\n", bits0, bits1, bits_idx, bits2, first_char, last_char);

  struct stat sb;

  for (int i=first_char; i <= last_char; i++) {
    char *filename;
    asprintf(&filename, lsb_file, i);

    char *filename_low32;
    asprintf(&filename_low32, "%s.low32", filename);
    int lsb_fd = open(filename_low32, O_RDONLY);
    if (lsb_fd < 0){
      printf("Failed to open %s.\n", filename_low32);
      handle_error("open");
    }
    fstat(lsb_fd, &sb);
    lsb_map[i] = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, lsb_fd, 0);
    if (lsb_map[i] == MAP_FAILED) handle_error("mmap lsb_map");

    char *filename_idx;
    asprintf(&filename_idx, "%s.idx", filename);
    int lsb_idx_fd = open(filename_idx, O_RDONLY);
    if (lsb_idx_fd < 0){
      printf("Failed to open %s.\n", filename_idx);
      handle_error("open");
    }
    fstat(lsb_idx_fd, &sb);
    lsb_idx_map[i] = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, lsb_idx_fd, 0);
    if (lsb_idx_map[i] == MAP_FAILED) handle_error("mmap lsb_idx");
  }

  int lookup_fd = open(lookup_file, O_RDONLY);
  if (lookup_fd < 0){
    printf("Failed to open %s.\n", lookup_file);
    handle_error("open");
  }
  fstat(lookup_fd, &sb);
  lookup_map_size = sb.st_size;
  lookup_map = mmap(NULL, lookup_map_size, PROT_READ, MAP_SHARED, lookup_fd, 0);
  if (lookup_map == MAP_FAILED) handle_error("mmap lookup_map");

  sanity_checks(lsb_file, lookup_file);

  // Open input and output
  int input_fd = open(input_file, O_RDONLY);
  if (input_fd < 0){
    printf("Failed to open %s.\n", input_file);
    handle_error("open");
  }
  int output_fd = open(output_file, O_CREAT|O_WRONLY|O_APPEND|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (output_file < 0){
    printf("Failed to open %s.\n", output_file);
    handle_error("open");
  }


  // Some constants
  secp256k1_ecmult_context ctx;
  secp256k1_ecmult_context_init(&ctx);
  secp256k1_ecmult_context_build(&ctx,NULL);

  secp256k1_scalar null_s;
  secp256k1_scalar_set_int(&null_s, 0);

  secp256k1_gej g;
  secp256k1_gej_set_ge(&g, &secp256k1_ge_const_g);

  secp256k1_scalar bits1_s;
  secp256k1_scalar_set_long(&bits1_s, 2l<<bits1);
  secp256k1_gej neg_g_bits1_dbl;
  secp256k1_ecmult(&ctx, &neg_g_bits1_dbl, &g, &null_s, &bits1_s);
  secp256k1_gej_neg(&neg_g_bits1_dbl, &neg_g_bits1_dbl);

  secp256k1_scalar bits2_s;
  secp256k1_scalar_set_long(&bits2_s, 2l<<bits2);
  secp256k1_gej g_bits2_dbl;
  secp256k1_ecmult(&ctx, &g_bits2_dbl, &g, &null_s, &bits2_s);
  secp256k1_gej neg_g_bits2_dbl;
  secp256k1_gej_neg(&neg_g_bits2_dbl, &g_bits2_dbl);

  unsigned long tried = 0;
  unsigned long lsb_hits = 0;
  unsigned long hits = 0;
  unsigned long failed_probe = 0;


  uint8_t b1[32];
  uint8_t b2[32];
  uint8_t b3[32];
  uint8_t b4[32];
  while (read(input_fd, b1, 32) == 32) {
    tried++;
    fprintf(stderr, "Lookup up entry %d\n", tried);
    unsigned long this_lsb_hit = 0;
    unsigned long this_hit = 0;
    // read r
    secp256k1_fe r;
    secp256k1_fe_set_b32(&r, b1);

    // try both signs for y
    for (int flip = 0; flip < 2; flip++) {
      // turn r into a point
      secp256k1_gej xj;
      secp256k1_ge x;
      secp256k1_ge_set_xquad(&x, &r);
      if (flip) secp256k1_ge_neg(&x,&x);
      secp256k1_gej_set_ge(&xj, &x);

      // look at x-i*(g*2^(bits1+1)) for i < 2^(bits0-(bits1+1)).
      for (unsigned long int i=0; i < (1l<<(bits0-(bits1+1))); i++) {
        // fprintf(stderr, "Trying sign=%d i=%d\n", flip, i);
        secp256k1_ge_set_gej(&x, &xj);
        secp256k1_fe_normalize(&(x.x));
        secp256k1_fe_get_b32(b2, &(x.x));
        // get 64 least significant bits
        uint64_t x_lsb = *(uint64_t*)(&b2[(256 - 64)/8]);

        if (is_in_lsb_map(x_lsb)) {
          this_lsb_hit++;
          fprintf(stderr, "LSB lookup table match! sign=%d i=%ld x_lsb=0x%08lx\n", flip, i, be64toh(x_lsb));

          // We got a match! Might still be a false positive...
          // So copy it and search in smaller steps in the lookup table
          // We search both directions
          for (long sign_j = 1; sign_j>-2; sign_j-=2) {
            secp256k1_gej x2j = xj;

            for (unsigned long int j=0; j < (1l<<((bits1+1)-(bits2+1))); j++) {
              // fprintf(stderr, "Trying sign=%d i=%d j=%d\n", flip, i, j);

              unsigned long l;
              if (is_in_lookup_map_gej(&x2j, &l)){
                for (long sign_l = 1; sign_l>-2; sign_l-=2) {
                  // We have found a match!
                  unsigned long k = i * (1l<<(bits1+1)) + sign_j * j * (1l<<(bits2+1)) + sign_l * l;

                  // Lets test if this is any good...
                  secp256k1_scalar k_s;
                  secp256k1_scalar_set_long(&k_s, k);

                  secp256k1_gej probej;
                  secp256k1_ge probe;
                  // set probe to G*k
                  secp256k1_ecmult(&ctx, &probej, &g, &k_s, &null_s);
                  secp256k1_ge_set_gej(&probe, &probej);
                  secp256k1_fe_normalize(&(probe.x));
                  secp256k1_fe_get_b32(b4, &(probe.x));

                  if (is_eq_b32(b1, b4)){
                    this_hit++;
                    fprintf(stderr, "Success (sign=%d i=%ld j=%ld l=%ld k=%ld)!\n", flip, i, sign_j * j, sign_l * l, k);
                    write(output_fd, b1, 32);
                    unsigned_long_to_b32_be(b3, k);
                    write(output_fd, b3, 32);

                    goto found;
                  }
                }
                failed_probe++;
                fprintf(stderr, "Probe failed (sign=%ld i=%ld j=%ld l=%ld)\n", flip, i, j, l);
                found: 0;
              }
              // subtract or add g*2^(bits2+1)
              if (sign_j > 0) {
                secp256k1_gej_add_var(&x2j, &x2j, &neg_g_bits2_dbl, NULL);
              } else {
                secp256k1_gej_add_var(&x2j, &x2j, &g_bits2_dbl, NULL);
              }
            }
          }
        }

        // subtract g*2^(bits1+1) from x
        secp256k1_gej_add_var(&xj, &xj, &neg_g_bits1_dbl, NULL);
      }
    }
    if (this_lsb_hit) { lsb_hits++; }
    if (this_hit)     { hits++; }
  }

  fprintf(stderr, "Tried: %ld LSB hits: %ld Found: %ld\n", tried, lsb_hits, hits);
}
