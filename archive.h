#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_FILENAME_SIZE 512
#define MAX_BUFFER_SIZE 1024
#define FALSE 0
#define TRUE 1
#define ARCHIVE_MARKER "ARCHIVE_MARKER"

int is_archive_file(const char *path);

void create_directory(const char *path);

void archive_directory(size_t orig_len, const char *src_dir, FILE *archive);

void extract_archive(const char *archive_file, const char *prefix, int attached_archive, char *main_dir);