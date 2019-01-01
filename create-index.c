#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

int main (int argc, char* argv[]) {
  if (argc < 3) {
    printf("Usage: %s [input] [bits]\n", argv[0]);
    printf("Assumes input is a binary file with 64 bit records, with the\n");
    printf("first byte constant.\n");
    printf("Creates index.idx with 2^bits+1 entries corresponding to the [bits]\n");
    printf("the first bytes. Each entry is a 64 bit number in machine order;\n");
    printf("the record index into the input file.\n");
    exit(1);
  }

  const char *input_filename = argv[1];
  char *output_filename;
  asprintf(&output_filename, "%s.idx", input_filename);

  unsigned long bits = atoi(argv[2]);

  FILE *input = fopen(input_filename, "r");
  if( input == NULL ) { perror(input_filename); return EXIT_FAILURE; }
  FILE *output = fopen(output_filename, "w");
  if( output == NULL ) { perror(output_filename); return EXIT_FAILURE; }

  u_int64_t idx_i = 0;
  u_int64_t data_i = 0;
  u_int64_t x = 0;
  while (fread(&x, sizeof(uint64_t), 1, input)) {
    x = be64toh(x);
    // get the right 2^24 bytes
    x = (x >> (64-8-bits)) % (1<<bits);
    // write all index entries in between
    while (idx_i <= x) {
      if (!fwrite(&data_i, sizeof(uint64_t), 1, output)) break;
      idx_i ++;
    }
    data_i++;
  };
  // reached the end of file, write the remaining entries.
  while (idx_i <= (1<<bits)) {
    if (!fwrite(&data_i, sizeof(uint64_t), 1, output)) break;
    idx_i ++;
  }

  if (ferror(input)) { perror(input_filename); return EXIT_FAILURE; }
  if (ferror(output)) { perror(output_filename); return EXIT_FAILURE; }
  fclose(output);
  fclose(input);
  return EXIT_SUCCESS;
}
