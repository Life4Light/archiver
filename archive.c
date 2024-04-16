#include "archive.h"

int remove_directory(const char *path)
{
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;

    if (d)
    {
        struct dirent *p;

        r = 0;
        while (!r && (p = readdir(d)))
        {
            int r2 = -1;
            char *buf;
            size_t len;

            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
                continue;

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if (buf)
            {
                struct stat statbuf;

                snprintf(buf, len, "%s/%s", path, p->d_name);
                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))
                        r2 = remove_directory(buf);
                    else
                        r2 = unlink(buf);
                }
                free(buf);
            }
            r = r2;
        }
        closedir(d);
    }

    if (!r)
        r = rmdir(path);

    return r;
}

void archive_directory(size_t orig_len, const char *src_dir, FILE *archive)
{
    DIR *dir = opendir(src_dir);
    if (dir == NULL)
    {
        fprintf(stderr, "Failed to open directory %s: %s\n", src_dir, strerror(errno));
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char src_path[MAX_FILENAME_SIZE];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);

        struct stat path_stat;
        if (stat(src_path, &path_stat) == -1)
        {
            fprintf(stderr, "Failed to get stat for %s: %s\n", src_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(path_stat.st_mode))
        {
            archive_directory(orig_len, src_path, archive);
        }
        else if (S_ISREG(path_stat.st_mode))
        {
            FILE *src = fopen(src_path, "rb");
            if (src == NULL)
            {
                fprintf(stderr, "Failed to open file %s: %s\n", src_path, strerror(errno));
                continue;
            }

            fwrite(src_path + orig_len + 1, sizeof(char), strlen(src_path) - orig_len, archive);

            fseek(src, 0, SEEK_END);
            long file_size = ftell(src);
            rewind(src);
            fwrite(&file_size, sizeof(long), 1, archive);

            char buffer[MAX_BUFFER_SIZE];
            size_t bytes;

            while ((bytes = fread(buffer, 1, sizeof(buffer), src)) != 0)
            {
                fwrite(buffer, 1, bytes, archive);
            }

            fclose(src);
        }
    }

    closedir(dir);
}

void create_directory(const char *path)
{
    char *dir_path_copy = strdup(path);
    char *dir_path = dirname(dir_path_copy);

    char *p = strtok(dir_path, "/");
    char current_path[MAX_FILENAME_SIZE] = "";

    while (p != NULL)
    {
        strcat(current_path, "/");
        strcat(current_path, p);

        mkdir(current_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        p = strtok(NULL, "/");
    }

    free(dir_path_copy);
}

void extract_archive(const char *archive_file, const char *prefix, int attached_archive, char *main_dir)
{
    FILE *archive = fopen(archive_file, "rb");
    if (archive == NULL)
    {
        printf("Failed to open archive\n");
        return;
    }

    char marker[sizeof(ARCHIVE_MARKER)];
    fread(marker, sizeof(char), sizeof(ARCHIVE_MARKER) - 1, archive);
    marker[sizeof(ARCHIVE_MARKER) - 1] = '\0';
    if (strcmp(marker, ARCHIVE_MARKER) != 0)
    {
        printf("File is not a valid archive\n");
        fclose(archive);
        return;
    }
    char *archive_name_copy = strdup(archive_file);
    char *archive_name = basename(archive_name_copy);

    while (!feof(archive))
    {
        char filename[MAX_FILENAME_SIZE];
        int i = 0;
        do
        {
            if (fread(&filename[i], sizeof(char), 1, archive) == 0)
            {
                fclose(archive);
                free(archive_name_copy);
                return;
            }
        } while (filename[i++] != '\0');

        if (strlen(filename) == 0)
        {
            break;
        }

        long file_size = 0;
        fread(&file_size, sizeof(long), 1, archive);
        char absolute_prefix[MAX_FILENAME_SIZE];
        realpath(prefix, absolute_prefix);
        prefix = absolute_prefix;
        char path[MAX_FILENAME_SIZE * 2 + 1];
        if (prefix != NULL)
        {
            sprintf(path, "%s/%s/%s", prefix, archive_name, filename);
        }

        create_directory(path);

        FILE *dst = fopen(path, "wb");
        if (dst == NULL)
        {
            fprintf(stderr, "Failed to open destination file %s: %s\n", path, strerror(errno));
            fclose(archive);
            free(archive_name_copy);
            return;
        }

        char buffer[MAX_BUFFER_SIZE];
        size_t bytes;
        while (file_size > 0 && (bytes = fread(buffer, 1, MIN(sizeof(buffer), file_size), archive)) != 0)
        {
            fwrite(buffer, 1, bytes, dst);
            file_size -= bytes;
        }
        fclose(dst);
        if (attached_archive)
        {
            if (is_archive_file(path))
            {
                extract_archive(path, prefix, attached_archive, main_dir);
            }
        }
    }
    free(archive_name_copy);
    fclose(archive);
}

int is_archive_file(const char *path)
{
    FILE *archive = fopen(path, "rb");
    if (archive == NULL)
    {
        return 0;
    }
    int is_archive = 1;
    char marker[sizeof(ARCHIVE_MARKER)];
    fread(marker, sizeof(char), sizeof(ARCHIVE_MARKER) - 1, archive);
    marker[sizeof(ARCHIVE_MARKER) - 1] = '\0';
    if (strcmp(marker, ARCHIVE_MARKER) != 0)
    {
        is_archive = 0;
    }
    fclose(archive);

    return is_archive;
}

int main(int argc, char *argv[])
{
    int opt;
    char *mode = NULL;
    char *src_path = NULL;
    char *dst_path = NULL;
    int only_attached = FALSE;
    char *main_dir = NULL;
    while ((opt = getopt(argc, argv, "m:s:d:a")) != -1)
    {
        switch (opt)
        {
        case 'm':
            mode = optarg;
            break;
        case 's':
            main_dir = src_path;
            src_path = optarg;
            break;
        case 'd':
            dst_path = optarg;
            break;
        case 'a':
            only_attached = TRUE;
            break;
        default:
            fprintf(stderr, "Usage: %s -m <mode> -s <source_path> -d <destination_path>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (mode == NULL || src_path == NULL || dst_path == NULL)
    {
        fprintf(stderr, "Usage: %s -m <mode> -s <source_path> -d <destination_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(mode, "archive") == 0)
    {
        char *dir_name = basename(src_path);
        char absolute_dst[MAX_FILENAME_SIZE];
        realpath(dst_path, absolute_dst);
        char path[MAX_FILENAME_SIZE * 2 + 1];
        sprintf(path, "%s/%s", absolute_dst, dir_name);
        FILE *archive = fopen(path, "wb");
        if (archive == NULL)
        {
            fprintf(stderr, "Failed to open archive file %s: %s\n", path, strerror(errno));
            exit(EXIT_FAILURE);
        }
        fwrite(ARCHIVE_MARKER, sizeof(char), strlen(ARCHIVE_MARKER), archive);
        archive_directory(strlen(src_path), src_path, archive);
        fclose(archive);
    }
    else if (strcmp(mode, "extract") == 0)
    {
        extract_archive(src_path, dst_path, only_attached, main_dir);
    }
    else
    {
        fprintf(stderr, "Invalid mode. Use 'archive' or 'extract'.\n");
        exit(EXIT_FAILURE);
    }

    if (only_attached)
    {
        char *dir_name = basename(src_path);
        char *last_token = strrchr(dir_name, '/');
        if (last_token != NULL)
        {
            dir_name = last_token + 1;
        }
        char absolute_dst[MAX_FILENAME_SIZE];
        realpath(dst_path, absolute_dst);
        char path[MAX_FILENAME_SIZE * 2 + 1];
        sprintf(path, "%s/%s", absolute_dst, dir_name);
        remove_directory(path);
    }

    return 0;
}