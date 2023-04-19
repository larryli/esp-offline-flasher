#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "esp_log.h"
#include "flash_args.h"

static const char *TAG = "flash_args";

static flash_args_t *get_from_json(const char *json, size_t length,
                                   const char *base_path)
{
    flash_args_t *args = NULL;
    size_t base_path_len = strlen(base_path) + 2; // "{$base_path}/{$path}\0"
    cJSON *root = cJSON_ParseWithLength(json, length);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Read json error: %s\n", error_ptr);
        }
        goto failed;
    }

    const cJSON *flash_files = cJSON_GetObjectItem(root, "flash_files");
    if (flash_files == NULL) {
        ESP_LOGE(TAG, "Cannot find \"flash_files\" arg\n");
        goto failed;
    }
    int size = cJSON_GetArraySize(flash_files);
    if (size <= 0) {
        ESP_LOGE(TAG, "Invalid \"flash_files\" size\n");
        goto failed;
    }
    size_t s = sizeof(flash_args_t) + sizeof(flash_file_t) * size;
    args = malloc(s);
    if (args == NULL) {
        ESP_LOGE(TAG, "Malloc flash_args %d bytes failed\n", s);
        goto failed;
    }
    memset(args, 0, s);
    args->flash_files_size = size;
    int n = 0;
    for (const cJSON *i = flash_files->child; i != NULL; i = i->next) {
        size_t s = base_path_len + strlen(i->valuestring);
        char *path = malloc(s);
        if (path == NULL) {
            ESP_LOGE(TAG,
                     "Malloc flash_args.flash_files[%d].path %d bytes failed\n",
                     n, s);
            goto failed;
        }
        strcpy(path, base_path);
        strcat(path, "/");
        strcat(path, i->valuestring);
        struct stat st;
        if (stat(path, &st) == 0) {
            args->flash_files[n].size = st.st_size;
        } else {
            ESP_LOGE(TAG, "Flash file \"%s\" is not exists\n", path);
            goto failed;
        }
        args->flash_files[n].addr = strtoul(i->string, NULL, 0);
        args->flash_files[n].path = path;
        n++;
    }

    cJSON_Delete(root);
    return args;

failed:
    flash_args_free(args);
    cJSON_Delete(root);
    return NULL;
}

char *find_json_file(char *dir, size_t *size, char **find_dir, int indent)
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
                    find_json_file(path, NULL, find_dir,
                                   indent + 1); // donot save
                } else {
                    find = find_json_file(path, size, find_dir, indent + 1);
                }
            } else if (d->d_type == DT_REG) {
                struct stat st;
                if (stat(path, &st) == 0) {
                    if (size != NULL && find == NULL &&
                        strcmp(d->d_name, "flasher_args.json") == 0) {
                        printf("\033[1;32m%s\t\033[1;36m\%ld\033[0m\n",
                               d->d_name, st.st_size);
                        find = path; // first only
                        *size = st.st_size;
                        *find_dir = dir;
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

flash_args_t *flash_args_find(char *dir)
{
    flash_args_t *args = NULL;
    size_t size;
    char *find_dir = NULL;
    ESP_LOGI(TAG, "List file(s):");
    char *json_file = find_json_file(dir, &size, &find_dir, 0);
    if (json_file != NULL) {
        ESP_LOGI(TAG, "Found \"%s\" %d bytes", json_file, size);
        char *buf = malloc(size + 1);
        if (buf != NULL) {
            FILE *fp = fopen(json_file, "r");
            if (fp != NULL) {
                size_t read = fread(buf, 1, size, fp);
                if (!ferror(fp)) {
                    buf[read] = '\0';
                    args = get_from_json(buf, read + 1, find_dir);
                }
                fclose(fp);
            }
            free(buf);
        }
        free(json_file);
        if (find_dir != dir) {
            free(find_dir);
        }
    }
    return args;
}

void flash_args_free(flash_args_t *args)
{
    if (args == NULL) {
        return;
    }
    for (int i = 0; i < args->flash_files_size; i++) {
        if (args->flash_files[i].path != NULL) {
            free(args->flash_files[i].path);
        }
    }
    free(args);
}

void flash_args_dump(flash_args_t *args)
{
    if (args == NULL) {
        return;
    }
    ESP_LOGI(TAG, "Flash %d file(s):", args->flash_files_size);
    for (int i = 0; i < args->flash_files_size; i++) {
        printf("  - \033[1;37maddr\033[0m: \033[1;36m0x%lx\033[0m\n"
               "    \033[1;37msize\033[0m: \033[1;36m%ld\033[0m\n"
               "    \033[1;37mpath\033[0m: \033[1;32m%s\033[0m\n",
               args->flash_files[i].addr, args->flash_files[i].size,
               args->flash_files[i].path);
    }
}
