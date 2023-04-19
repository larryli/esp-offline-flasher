#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "findfile.h"

static const char *TAG = "findfile";

static char *_findfile(const char *dir, const char *fname, size_t *size,
                       char **find_dir, int indent)
{
    char *find = NULL;
    struct dirent *d;
    DIR *dh = opendir(dir);
    if (!dh) {
        if (errno == ENOENT) {
            ESP_LOGE(TAG, "Directory doesn't exist %s",
                     CONFIG_TINYUSB_MSC_MOUNT_PATH);
        } else {
            ESP_LOGE(TAG, "Unable to read directory %s",
                     CONFIG_TINYUSB_MSC_MOUNT_PATH);
        }
        return NULL;
    }
    while ((d = readdir(dh)) != NULL) {
        char *path = malloc(strlen(dir) + strlen(d->d_name) + 2);
        if (path != NULL) {
            strcpy(path, dir);
            strcat(path, "/");
            strcat(path, d->d_name);
            for (int i = 0; i < indent; i++) {
                printf("  ");
            }
            if (d->d_type == DT_DIR) {
                printf("\033[1;34m%s\033[0m\n", d->d_name);
                if (find != NULL) {
                    // donot save
                    _findfile(path, fname, NULL, find_dir, indent + 1);
                } else {
                    find = _findfile(path, fname, size, find_dir, indent + 1);
                }
            } else if (d->d_type == DT_REG) {
                struct stat st;
                if (stat(path, &st) == 0) {
                    if (size != NULL && find == NULL &&
                        strcmp(d->d_name, fname) == 0) {
                        printf("\033[1;32m%s\t\033[1;36m\%ld\033[0m\n",
                               d->d_name, st.st_size);
                        find = path; // first only
                        *size = st.st_size;
                        *find_dir = (char *)dir;
                    } else {
                        printf("%s\t\033[0;36m%ld\033[0m\n", d->d_name,
                               st.st_size);
                    }
                } else {
                    printf("\033[1;31m%s\033[0m\n", d->d_name);
                }
            }
            if (path != find && path != *find_dir) {
                free(path);
            }
        }
    }
    closedir(dh);
    return find;
}

findfile_t *findfile(const char *dir, const char *fname)
{
    size_t size;
    char *find_dir = NULL;
    ESP_LOGI(TAG, "List file(s):");
    char *find = _findfile(dir, fname, &size, &find_dir, 0);
    if (find == NULL) {
        return NULL;
    }
    ESP_LOGI(TAG, "Found \"%s\" %d bytes", find, size);
    bool failed = true;
    findfile_t *ff = malloc(sizeof(findfile_t));
    if (ff != NULL) {
        ff->dir = find_dir;
        ff->size = size + 1;
        ff->buf = malloc(ff->size);
        if (ff->buf != NULL) {
            FILE *fp = fopen(find, "r");
            if (fp != NULL) {
                size_t read = fread(ff->buf, 1, size, fp);
                if (!ferror(fp)) {
                    ff->buf[read] = '\0';
                    ff->size = read + 1;
                    failed = false;
                }
                fclose(fp);
            }
        }
    }
    free(find);
    if (failed) { // clean
        if (ff != NULL) {
            if (ff->buf != NULL) {
                free(ff->buf);
            }
            free(ff);
            ff = NULL;
        }
        if (find_dir != dir) {
            free(find_dir);
        }
    }
    return ff;
}
