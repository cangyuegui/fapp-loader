#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include "cJSON.h"

int is_directory(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        perror("stat");
        return 0;
    }
    return S_ISDIR(path_stat.st_mode);
}

char* read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char*)malloc(file_size + 1);
    if (!content) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    fread(content, 1, file_size, file);
    content[file_size] = '\0';  // Null-terminate the string

    fclose(file);
    return content;
}

// Function to extract the "exe" section from the JSON file content
char* get_exe_section(const char *json_content, const char* section) {
    cJSON *json = cJSON_Parse(json_content);
    if (!json) {
        fprintf(stderr, "Error parsing JSON\n");
        return NULL;
    }

    cJSON *exe = cJSON_GetObjectItem(json, section);
    if (!exe || !cJSON_IsString(exe)) {
        fprintf(stderr, "\"%s\" section not found or is not a string\n", section);
        cJSON_Delete(json);
        return NULL;
    }

    char *exe_content = strdup(exe->valuestring);
    cJSON_Delete(json);
    return exe_content;
}

// Function to count the number of environment variables
size_t count_envp(char *const envp[]) {
    size_t count = 0;
    while (envp[count] != NULL) {
        count++;
    }
    return count;
}

// Function to combine custom environment variables with the existing environment
char **combine_envp(char *const custom_envp[]) {
    // Get the existing environment variables
    extern char **environ;
    size_t existing_count = count_envp(environ);
    size_t custom_count = count_envp(custom_envp);
    size_t total_count = existing_count + custom_count + 1; // +1 for NULL terminator

    // Allocate memory for the new environment array
    char **new_envp = (char**)malloc(total_count * sizeof(char *));
    if (new_envp == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Copy existing environment variables
    size_t i;
    for (i = 0; i < existing_count; i++) {
        new_envp[i] = strdup(environ[i]);
        if (new_envp[i] == NULL) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
    }

    // Add custom environment variables
    for (size_t j = 0; j < custom_count; j++) {
        new_envp[i++] = strdup(custom_envp[j]);
        if (new_envp[i - 1] == NULL) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
    }

    // Null-terminate the new environment array
    new_envp[i] = NULL;

    return new_envp;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "no app dir input";
        return 0;
    }

    char* fapp_path = argv[1];
    if (is_directory(fapp_path)) {
        // Check if it ends with ".fapp"
        size_t len = strlen(fapp_path);
        if (len > 5 && strcmp(fapp_path + len - 5, ".fapp") == 0) {
            printf("The path is a directory and ends with '.fapp'.\n");
        } else {
            printf("The path is a directory but does not end with '.fapp'.\n");
            return 0;
        }
    } else {
        printf("The path is not a directory.\n");
        return 0;
    }

    char *lib_path;
    char *ld_library_path;
    char *new_ld_library_path;
    size_t len_fapp = strlen(fapp_path);
    size_t len_lib = strlen("/lib");
    size_t len_ld_library_path;
    size_t total_len;

    // Combine fapp_path with "/lib"
    lib_path = (char*)malloc(len_fapp + len_lib + 1);
    if (lib_path == NULL) {
        perror("malloc");
        return 1;
    }
    strcpy(lib_path, fapp_path);
    strcat(lib_path, "/lib");

    // Retrieve the current LD_LIBRARY_PATH
    ld_library_path = getenv("LD_LIBRARY_PATH");
    if (ld_library_path != NULL) {
        len_ld_library_path = strlen(ld_library_path);
        total_len = len_fapp + len_lib + 1 + len_ld_library_path + 1;
        new_ld_library_path = (char*)malloc(total_len);
        if (new_ld_library_path == NULL) {
            perror("malloc");
            free(lib_path);
            return 1;
        }
        snprintf(new_ld_library_path, total_len, "%s:%s", lib_path, ld_library_path);
    } else {
        // If LD_LIBRARY_PATH is not set, just use lib_path
        new_ld_library_path = strdup(lib_path);
        if (new_ld_library_path == NULL) {
            perror("strdup");
            free(lib_path);
            return 1;
        }
    }

    // Print the combined path
    printf("Combined path: %s\n", new_ld_library_path);


    char json_path[len_fapp + strlen("/main.json") + 1];
    snprintf(json_path, sizeof(json_path), "%s/main.json", fapp_path);

    // Read the JSON file
    char *json_content = read_file(json_path);
    if (!json_content) {
        return 1;
    }

    // Get the "exe" section
    char *exe_content = get_exe_section(json_content, "exe");
    if (!exe_content) {
        return 0;
    }

    size_t exe_path_len = len_fapp + strlen("/bin/") + strlen(exe_content) + 1;
    char exe_path[exe_path_len];
    snprintf(exe_path, exe_path_len, "%s/bin/%s", fapp_path, exe_content);
    free(exe_content);

    printf("Exe section content: %s\n", exe_content);

    char *custom_envp[] = {
            NULL
        };
    char **new_envp = combine_envp(custom_envp);
    size_t count_envp_s = count_envp(new_envp);

    // Print the new environment for demonstration purposes
    for (int i = 0; i < count_envp_s; ++i) {
        const char* ld_library_path_rep = "LD_LIBRARY_PATH=";
        size_t ld_library_path_rep_len = strlen(ld_library_path_rep);
        if (strlen(new_envp[i]) >= ld_library_path_rep_len && strncmp(new_envp[i], ld_library_path_rep, ld_library_path_rep_len) == 0)
        {
            free(new_envp[i]);
            new_envp[i] = new char[ld_library_path_rep_len + strlen(new_ld_library_path)];
            snprintf(new_envp[i], ld_library_path_rep_len + strlen(new_ld_library_path), "%s%s", ld_library_path_rep, new_ld_library_path);
        }
    }

    for (int i = 0; i < count_envp_s; ++i) {
        printf("%s\n", new_envp[i]);
    }

    char** nargv = new char*[argc - 1];
    nargv[0] = exe_path;
    for (int i = 2; i < argc; ++i)
        nargv[i] = argv[i - 1];

    argv[0] = exe_path;
    execve(exe_path, nargv, new_envp);

    // Clean up allocated memory
    for (char **env = new_envp; *env != NULL; env++) {
        free(*env);
    }

    free(nargv);
    free(lib_path);
    free(new_ld_library_path);
    return 0;
}
