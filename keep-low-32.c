#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

int main (int argc, char* argv[]) {
  if (argc < 1) {
    printf("Usage: %s [input.bin]\n", argv[0]);
    exit(1);
  }

  const char *input_filename = argv[1];
  char *output_filename;
  asprintf(&output_filename, "%s.low32", input_filename);

  FILE *input = fopen(input_filename, "r");
  if( input == NULL ) { perror(input_filename); return EXIT_FAILURE; }
  FILE *output = fopen(output_filename, "w");
  if( output == NULL ) { perror(output_filename); return EXIT_FAILURE; }

  char buf[sizeof(uint64_t)];
  while (fread(buf, sizeof(uint64_t), 1, input)) {
    if (!fwrite(&(buf[(64-32)/8]), sizeof(uint32_t), 1, output)) break;
  }
  if (ferror(input)) { perror(input_filename); return EXIT_FAILURE; }
  if (ferror(output)) { perror(output_filename); return EXIT_FAILURE; }
  fclose(output);
  fclose(input);
}
