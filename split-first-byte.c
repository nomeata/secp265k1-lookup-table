#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

int main (int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: %s [input] [output%%02hhx.bin] [0..255] [max_elements]\n", argv[0]);
    exit(1);
  }

  const char *input_filename = argv[1];
  const char *output_filename_pat = argv[2];
  const int long_needle = atoi(argv[3]);
  if (long_needle < 0 || long_needle>255) {
    printf("%d not in range 0..255\n", long_needle);
    exit(1);
  }
  const char needle = (char)long_needle;

  unsigned long max_elems = atoi(argv[4]);

  char *output_filename;
  asprintf(&output_filename, output_filename_pat, needle);
  FILE *input = fopen(input_filename, "r");
  if( input == NULL ) { perror(input_filename); return EXIT_FAILURE; }
  FILE *output = fopen(output_filename, "w");
  if( output == NULL ) { perror(output_filename); return EXIT_FAILURE; }

  char buf[sizeof(uint64_t)];
  while (fread(buf, sizeof(uint64_t), 1, input)) {
    if (buf[0] == needle) {
      if (!fwrite(buf, sizeof(uint64_t), 1, output)) break;
    }
    max_elems--;
    if (max_elems == 0) {
      break;
    }
  }
  if (ferror(input)) { perror(input_filename); return EXIT_FAILURE; }
  if (ferror(output)) { perror(output_filename); return EXIT_FAILURE; }
  fclose(output);
  fclose(input);
}
