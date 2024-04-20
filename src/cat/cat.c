#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NAME "cat (canoutils)"
#define VERSION "1.0.0"
#define AUTHOR "Akos Szijgyarto (SzAkos04)"

#define print_version()                                                        \
  do {                                                                         \
    printf("%s\nversion: %s\nby: %s\n", NAME, VERSION, AUTHOR);                \
  } while (0)

#define print_help()                                                           \
  do {                                                                         \
    printf("Usage: cat [OPTION]... [FILE]...\n");                              \
    printf("Concatenate FILE(s) to standard output.\n");                       \
  } while (0)

#define print_incorrect_args()                                                 \
  do {                                                                         \
    printf("incorrect arguments\n");                                           \
    printf("see `cat --help`\n");                                              \
  } while (0)

#define BUF_MAX_LEN 4096 // max length of a buffer in bytes
#define PATH_LEN 256     // max length of a path in bytes
#define ARGS_MAX 16      // number of the max arguments
#define ARGS_LEN 32      // max length of the arguments in bytes

bool number_nonblank = false;  // number nonempty output lines, overrides -n
bool show_ends = false;        // display $ at end of each line
bool number = false;           // number all output lines
bool squeeze_blank = false;    // suppress repeated empty output lines
bool show_tabs = false;        // display TAB characters as ^I
bool show_nonprinting = false; // use ^ and M- notation, except for LFD and TAB

int cat(int filec, char **paths);
int print_file(char *buf);
int print_stdin(void);

int main(int argc, char **argv) {
  if (argc < 2) {
    // print stdin
    if (print_stdin() != 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(argv[1], "--version") == 0) {
    if (argc != 2) {
      print_incorrect_args();
      return 1;
    }

    print_version();
    return 0;
  }

  if (strcmp(argv[1], "--help") == 0) {
    if (argc != 2) {
      print_incorrect_args();
      return 1;
    }

    print_help();
    return 0;
  }

  int filec = 0; // file count
  char **paths = (char **)malloc(sizeof(char) * PATH_LEN * ARGS_MAX);
  if (!paths) {
    perror("could not allocate memory");
    return 1;
  }

  // parse arguments
  for (int i = 1; i < argc; ++i) {
    int len = strlen(argv[i]);
    if (len > 1 && argv[i][0] == '-') {
      for (int j = 1; j < len; ++j) {
        switch (argv[i][j]) {
        case 'b':
          number_nonblank = true;
          number = false;
          continue;
        case 'E':
          show_ends = true;
          continue;
        case 'n':
          number = true;
          number_nonblank = false;
          continue;
        case 's':
          squeeze_blank = true;
          continue;
        case 'T':
          show_tabs = true;
          continue;
        case 'v':
          show_nonprinting = true;
          continue;
        case '-':
          if (strcmp(argv[i], "--number-nonblank") == 0) {
            number_nonblank = true;
            number = false;
          } else if (strcmp(argv[i], "--show-ends") == 0) {
            show_ends = true;
          } else if (strcmp(argv[i], "--number") == 0) {
            number = true;
            number_nonblank = false;
          } else if (strcmp(argv[i], "--squeeze-blank") == 0) {
            squeeze_blank = true;
          } else if (strcmp(argv[i], "--show-tabs") == 0) {
            show_tabs = true;
            show_nonprinting = false;
          } else if (strcmp(argv[i], "--show-nonprinting") == 0) {
            show_nonprinting = true;
            show_tabs = false;
          }
          break;
        default:
          fprintf(stderr, "unknown argument `-%c`", argv[i][j]);
          free(paths);
          return 1;
        }
      }
    } else {
      // check if the file is accessible
      if (access(argv[i], F_OK | R_OK) != 0 && strcmp(argv[i], "-") != 0) {
        fprintf(stderr, "file `%s` not found\n", argv[i]);
        free(paths);
        return 1;
      }

      paths[filec++] = argv[i];
    }
  }

  if (cat(filec, paths) != 0) {
    free(paths);
    return 1;
  }

  free(paths);

  return 0;
}

int cat(int filec, char **paths) {
  // print stdin
  if (filec == 0 || !paths) {
    char *buf = (char *)malloc(sizeof(char) * BUF_MAX_LEN);
    if (!buf) {
      perror("could not allocate memory");
      return 1;
    }

    if (print_stdin() != 0) {
      return 1;
    }

    free(buf);

    return 0;
  }

  for (int i = 0; i < filec; ++i) {
    // print stdin
    if (strcmp(paths[i], "-") == 0) {
      char *buf = (char *)malloc(sizeof(char) * BUF_MAX_LEN);
      if (!buf) {
        perror("could not allocate memory");
        return 1;
      }

      if (print_stdin() != 0) {
        return 1;
      }

      free(buf);

      continue;
    }

    // read file
    FILE *infile = fopen(paths[i], "r");
    if (!infile) {
      perror("could not open file");
      return 1;
    }

    // get the size of the file
    fseek(infile, 0, SEEK_END);
    long file_size = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    char *buf = (char *)malloc(sizeof(char) * (file_size + 1));
    if (!buf) {
      perror("could not allocate memory");
      fclose(infile);
      return 1;
    }

    // read the file into the buffer
    size_t files_read = fread(buf, sizeof(char), file_size, infile);
    if (files_read != (size_t)file_size) {
      fprintf(stderr, "could not read file\n");
      fclose(infile);
      free(buf);
      return 1;
    }
    buf[file_size] = '\0'; // make sure the string is null terminated

    fclose(infile);

    if (print_file(buf) != 0) {
      free(buf);
      return 1;
    }

    free(buf);
  }
  return 0;
}

#define NUMBER_BEFORE 6 // number of spaces before line numbers

int print_file(char *buf) {
  int lines = 1;
  if (number) {
    // print number before the first line
    printf("%*d  ", NUMBER_BEFORE, lines);
  }
  if (number_nonblank) {
    if (buf[0] != '\n' && buf[1] != '\0') {
      // print number before the first line
      printf("%*d  ", NUMBER_BEFORE, lines);
    }
  }
  int len = strlen(buf);
  for (int i = 0; i < len; ++i) {
    // higher priority
    // NOTE: not the prettiest code, but it works, so don't touch it
    if (squeeze_blank && buf[i] == '\n') {
      // skip over consecutive '\n' characters
      if (i + 1 < len && buf[i + 1] == '\n') {
        // if the consecutive '\n' characters are over
        if (i + 2 < len && buf[i + 2] != '\n') {
          if (number) {
            printf("\n%*d  ", NUMBER_BEFORE, ++lines);
          } else {
            putchar('\n');
          }
        }
        continue;
      }
    }

    if (number_nonblank && buf[i] == '\n' && buf[i + 1] != '\n' &&
        buf[i + 1] != '\0') {
      printf("\n%*d  ", NUMBER_BEFORE, ++lines);
      continue;
    }
    if (show_ends && buf[i] == '\n') {
      putchar('$');
    }
    if (number && buf[i] == '\n' && buf[i + 1] != '\0') {
      printf("\n%*d  ", NUMBER_BEFORE, ++lines);
      continue;
    }
    if (show_tabs && buf[i] == '\t') {
      printf("^I");
      continue;
    }
    if (show_nonprinting && !isprint(buf[i]) && buf[i] != 9 && buf[i] != 10) {
      if (buf[i] & 0x80) {
        // meta (M-) notation for characters with the eighth bit set
        printf("M-");
        char printable_char = buf[i] & 0x7F; // clear the eighth bit
        printf("^%c", '@' + printable_char);
      } else {
        // regular non-printable character notation
        printf("^%c", '@' + buf[i]);
      }
      continue;
    }

    putchar(buf[i]);
  }

  return 0;
}

int print_stdin(void) {
  char buf[BUF_MAX_LEN];
  while (fgets(buf, BUF_MAX_LEN, stdin)) {
    if (print_file(buf) != 0) {
      return 1;
    }
  }

  return 0;
}
