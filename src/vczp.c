#include "vczp.h"
#include "b64.h"
#include <stdio.h>
#include <stdlib.h>
#include <cJSON.h>
#include <zlib.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <errno.h> 


#define CHECK_COMPRESSION(result, data, str) \
    if ((result) != Z_OK) { \
        fprintf(stderr, "Error compressing %s\n", (str)); \
        free(data); \
        exit(1); \
    }

#define CHECK_DECOMPRESSION(result, data, str) \
    if ((result) != Z_OK) { \
        fprintf(stderr, "Error decompressing %s\n", (str)); \
        free(data); \
        exit(1); \
    }

static void executar_comandos(char* comando, char* mensagem) {
    printf("%s: %s\n", mensagem, comando);
    char* sistema = "Linux"; // Assuming Linux as default system
    if (strcmp(sistema, "Windows") == 0) {
        // Adjust the command for Windows
        // Replace '/' with '\\'
        for (int i = 0; comando[i] != '\0'; i++) {
            if (comando[i] == '/') {
                comando[i] = '\\';
            }
        }
    }
    int status = system(comando);
    if (status != 0) {
        printf("Erro ao executar o comando: %s\n", comando);
        exit(1);
    }
}

static void compress_data(const char *data_str, Bytef **compressed_data, uLong *compressed_length) {
    uLong data_length = strlen(data_str) + 1;
    *compressed_length = compressBound(data_length);
    *compressed_data = (Bytef *)malloc(*compressed_length);
    
    if (*compressed_data == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(1);
    }

    int result = compress(*compressed_data, compressed_length, (const Bytef *)data_str, data_length);
    CHECK_COMPRESSION(result, *compressed_data, "data");
}

static void free_resources(Bytef *compressed_data1, Bytef *compressed_data2, char *str1, char *str2) {
    free(compressed_data1);
    free(compressed_data2);
    free(str1);
    free(str2);
}

static void decompress_data(Bytef *compressed_data, uLong compressed_length, Bytef **decompressed_data, uLong *decompressed_length) {
    *decompressed_length = compressed_length * 10; // Estimate decompressed size
    *decompressed_data = (Bytef *)malloc(*decompressed_length);

    if (*decompressed_data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    int result = uncompress(*decompressed_data, decompressed_length, compressed_data, compressed_length);
    CHECK_DECOMPRESSION(result, *decompressed_data, "data");
}

static void read_compressed_data(FILE *file, Bytef **compressed_data, uLong *compressed_length) {
    fread(compressed_length, sizeof(uLong), 1, file);

    *compressed_data = (Bytef *)malloc(*compressed_length);
    if (*compressed_data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        exit(1);
    }

    fread(*compressed_data, 1, *compressed_length, file);
}

static unsigned char* read_file(const char* filename, size_t* out_size) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = malloc(file_size);
    if (buffer == NULL) {
        perror("Erro ao alocar memória para leitura do arquivo");
        fclose(file);
        exit(1);
    }

    size_t read_size = fread(buffer, 1, file_size, file);
    if (read_size != file_size) {
        perror("Erro ao ler o arquivo");
        free(buffer);
        fclose(file);
        exit(1);
    }

    fclose(file);
    *out_size = file_size;
    return buffer;
}


void pack_vczp(const char *path_vczp, void* files_arr, void* commands_arr, void* pkg_info, const void* base_dir, const int is_debug) {
    cJSON *package_info = (cJSON*)pkg_info;
    cJSON *commands = (cJSON*)commands_arr;
    cJSON *files = (cJSON*)files_arr;

    printf("Criando: %s\n", path_vczp);
    printf("Name: %s\n", cJSON_GetObjectItem(package_info, "Name")->valuestring);
    printf("Version: %s\n", cJSON_GetObjectItem(package_info, "Version")->valuestring);
    printf("Description: %s\n", cJSON_GetObjectItem(package_info, "Description")->valuestring);
    printf("Author: %s\n", cJSON_GetObjectItem(package_info, "Author")->valuestring);
    printf("Architecture: %s\n", cJSON_GetObjectItem(package_info, "Architecture")->valuestring);
    printf("Files_path_info: %s\n", cJSON_GetObjectItem(package_info, "files_path")->valuestring);

    char *package_info_str = cJSON_PrintUnformatted(package_info);
    char *commands_str = cJSON_PrintUnformatted(commands);

    Bytef *compressed_data_package;
    uLong compressedLength_package;
    compress_data(package_info_str, &compressed_data_package, &compressedLength_package);

    Bytef *compressed_data_commands;
    uLong compressedLength_commands;
    compress_data(commands_str, &compressed_data_commands, &compressedLength_commands);

    

    cJSON *file = NULL;
    cJSON *files_obj_arr = cJSON_CreateArray();
    cJSON_ArrayForEach(file, files) {
        // Cria uma cópia do objeto 'file' para evitar sobrescrever o mesmo item
        cJSON *file_copy = cJSON_Duplicate(file, cJSON_True);

        if (file_copy == NULL) {
            fprintf(stderr, "Error: Failed to create a copy of the file object.\n");
            exit(1);
        }

        char full_path[PATH_MAX];
        char *path = cJSON_GetObjectItem(file_copy, "path")->valuestring;
        sprintf(full_path, "%s/%s", (const char *)base_dir, path);
        char *type = cJSON_GetObjectItem(file_copy, "type")->valuestring;

        if (strcmp(type, "file") == 0) {
            size_t file_size;
            unsigned char *file_content = read_file(full_path, &file_size);

            // Codifica o conteúdo do arquivo em Base64
            size_t encoded_length;
            char *encoded = base64_encode(file_content, file_size, &encoded_length);
            cJSON_AddStringToObject(file_copy, "bytes", encoded);
            printf("File: %s\n", full_path);

            free(file_content);
            free(encoded);
        } else if (strcmp(type, "folder") == 0) {
            cJSON_AddStringToObject(file_copy, "bytes", "");
            printf("Folder: %s\n", full_path);
        } else if (strcmp(type, "root_folder") == 0) {
            cJSON_AddStringToObject(file_copy, "bytes", "");
            printf("Root Folder: %s\n", full_path);
        } else {
            fprintf(stderr, "Error: Unknown type '%s' encountered.\n", type);
            cJSON_Delete(file_copy);  // Limpa a memória antes de sair
            exit(1);
        }

        // Adiciona a cópia ao array
        cJSON_AddItemToArray(files_obj_arr, file_copy);
    }

    char *files_obj_arr_str = cJSON_PrintUnformatted(files_obj_arr);

    Bytef *compressed_data_files;
    uLong compressedLength_files;
    compress_data(files_obj_arr_str, &compressed_data_files, &compressedLength_files);
    
    if (is_debug == 1) {
        printf("Package compressed: ");
        for (uLong i = 0; i < compressedLength_package; i++) {
            printf("%02x ", compressed_data_package[i]);
        }
        printf("\n\n");

        printf("Commands compressed: ");
        for (uLong i = 0; i < compressedLength_commands; i++) {
            printf("%02x ", compressed_data_commands[i]);
        }
        printf("\n\n");

        printf("Files compressed: ");
        for (uLong i = 0; i < compressedLength_files; i++) {
            printf("%02x ", compressed_data_files[i]);
        }
        printf("\n");
    }



    FILE *archive = fopen(path_vczp, "wb");
    if (archive == NULL) {
        perror("Erro ao abrir o arquivo");
        free_resources(compressed_data_package, compressed_data_commands, package_info_str, commands_str);
        exit(1);
    }

    const char header[] = "VCZP\x01";
    fwrite(header, sizeof(char), sizeof(header) - 1, archive);
    fwrite(&compressedLength_package, sizeof(uLong), 1, archive);
    fwrite(compressed_data_package, 1, compressedLength_package, archive);
    fwrite(&compressedLength_commands, sizeof(uLong), 1, archive);
    fwrite(compressed_data_commands, 1, compressedLength_commands, archive);
    fwrite(&compressedLength_files, sizeof(uLong), 1, archive);
    fwrite(compressed_data_files, 1, compressedLength_files, archive);
    
    
    fclose(archive);
    free_resources(compressed_data_package, compressed_data_commands, package_info_str, commands_str);
}


void debug(const char *path_vczp, const char *current_env) {
    FILE *archive_vczp = fopen(path_vczp, "rb");
    if (archive_vczp == NULL) {
        perror("Erro ao abrir o arquivo");
        return;
    }

    unsigned char header_package[5];
    size_t bytesRead_package = fread(header_package, 1, 5, archive_vczp);

    if (bytesRead_package < 5) {
        perror("Error reading file");
        fclose(archive_vczp);
        return;
    }

    if (memcmp(header_package, "VCZP\x01", 5) != 0) {
        fprintf(stderr, "Invalid file format\n");
        fclose(archive_vczp);
        return;
    }

    uLong compressedLength_package;
    Bytef *compressed_data_package;
    read_compressed_data(archive_vczp, &compressed_data_package, &compressedLength_package);

    Bytef *decompressed_data_package_str;
    uLong dataLength_package;
    decompress_data(compressed_data_package, compressedLength_package, &decompressed_data_package_str, &dataLength_package);

    cJSON *decompressed_data_package = cJSON_Parse(decompressed_data_package_str);
    if (decompressed_data_package == NULL) {
        fprintf(stderr, "Error parsing JSON data\n");
        free(compressed_data_package);
        free(decompressed_data_package_str);
        fclose(archive_vczp);
        return;
    }

    printf("Criando: %s\n", path_vczp);
    printf("Name: %s\n", cJSON_GetObjectItem(decompressed_data_package, "Name")->valuestring);
    printf("Version: %s\n", cJSON_GetObjectItem(decompressed_data_package, "Version")->valuestring);
    printf("Description: %s\n", cJSON_GetObjectItem(decompressed_data_package, "Description")->valuestring);
    printf("Author: %s\n", cJSON_GetObjectItem(decompressed_data_package, "Author")->valuestring);
    printf("Architecture: %s\n", cJSON_GetObjectItem(decompressed_data_package, "Architecture")->valuestring);

    // Descompactação dos comandos
    uLong compressedLength_commands;
    Bytef *compressed_data_commands;
    read_compressed_data(archive_vczp, &compressed_data_commands, &compressedLength_commands);

    Bytef *decompressed_data_commands_str;
    uLong dataLength_commands;
    decompress_data(compressed_data_commands, compressedLength_commands, &decompressed_data_commands_str, &dataLength_commands);

    cJSON *decompressed_data_commands = cJSON_Parse(decompressed_data_commands_str);
    if (decompressed_data_commands == NULL) {
        fprintf(stderr, "Error parsing JSON commands\n");
        free(compressed_data_package);
        free(decompressed_data_package_str);
        free(compressed_data_commands);
        free(decompressed_data_commands_str);
        fclose(archive_vczp);
        return;
    }

    //printf("Commands:\n%s\n", cJSON_Print(decompressed_data_commands));

    //Descompactando Arquivos

    uLong compressedLength_files;
    Bytef *compressed_data_files;
    read_compressed_data(archive_vczp, &compressed_data_files, &compressedLength_files);
    
    Bytef *decompressed_data_files_str;
    uLong dataLength_files;
    decompress_data(compressed_data_files, compressedLength_files, &decompressed_data_files_str, &dataLength_files);

    cJSON *decompressed_data_files = cJSON_Parse(decompressed_data_files_str);

    if (decompressed_data_files == NULL) {
        fprintf(stderr, "Error parsing JSON commands\n");
        free(compressed_data_package);
        free(decompressed_data_package_str);
        free(compressed_data_commands);
        free(decompressed_data_commands_str);
        free(decompressed_data_files_str);
        fclose(archive_vczp);
        return;
    }

    cJSON *file = NULL;

    char *output_path = cJSON_GetObjectItem(decompressed_data_package, "installDebugDir")->valuestring;
    

    cJSON_ArrayForEach(file, decompressed_data_files){
        char *file_name = cJSON_GetObjectItem(file, "name")->valuestring;
        char *file_path = cJSON_GetObjectItem(file, "path")->valuestring;
        size_t file_size = (size_t)cJSON_GetObjectItem(file, "size")->valuedouble;
        long file_mtime = (long)cJSON_GetObjectItem(file, "mtime")->valuedouble;
        int permissions = cJSON_GetObjectItem(file, "permissions")->valueint;
        char *file_type = cJSON_GetObjectItem(file, "type")->valuestring;
        char *bytes_b64 = cJSON_GetObjectItem(file, "bytes")->valuestring;

        
        

        char full_path[PATH_MAX];

        sprintf(full_path, "%s/%s", (const char *)output_path, file_path);
        if(strcmp(file_type, "file") == 0){
            printf("file: %s, Type: %s, Path: %s, Size: %ld, Mtime: %ld Permissions: %d\n", file_name, file_type, file_path, file_size, file_mtime, permissions);
            printf("FILE_PATH:> %s\n", full_path);
            
            size_t decoded_size;
            unsigned char *decoded_bytes = base64_decode(bytes_b64, strlen(bytes_b64), &decoded_size);

            if (decoded_bytes == NULL) {
                perror("Erro ao decodificar os bytes base64");
                exit(EXIT_FAILURE);
            }

            
            FILE *f = fopen(full_path, "wb");

            if (f == NULL) {
                perror("Erro ao abrir o arquivo para escrita");
                free(decoded_bytes);
                exit(EXIT_FAILURE);
            }

            size_t written_size = fwrite(decoded_bytes, 1, decoded_size, f);

            if (written_size != decoded_size) {
                perror("Erro ao escrever no arquivo");
                free(decoded_bytes);
                fclose(f);
                exit(EXIT_FAILURE);
            }

            fclose(f);
            free(decoded_bytes);

            // Atualize o tempo de modificação do diretório
            struct utimbuf new_times;
            new_times.actime = file_mtime;  // Hora de último acesso
            new_times.modtime = file_mtime; // Hora de modificação
            if (utime(full_path, &new_times) == -1) {
                perror("Erro ao atualizar tempo de modificação");
                exit(EXIT_FAILURE);
            }

            // Defina as permissões do diretório
            if (chmod(full_path, (mode_t)permissions) == -1) {
                perror("Erro ao definir permissões do diretório");
                exit(EXIT_FAILURE);
            }
        }else if (strcmp(file_type, "folder") == 0 || strcmp(file_type, "root_folder") == 0){
            printf("folder: %s, Type: %s, Path: %s, Size: %ld, Mtime: %ld Permissions: %d\n", file_name, file_type, file_path, file_size, file_mtime, permissions);
            printf("FOLDER_PATH:> %s\n", full_path);
            if (mkdir(full_path, 0777) == -1 && errno != EEXIST) {
                perror("Erro ao criar diretório");
                exit(EXIT_FAILURE);
            }

            // Atualize o tempo de modificação do diretório
            struct utimbuf new_times;
            new_times.actime = file_mtime;  // Hora de último acesso
            new_times.modtime = file_mtime; // Hora de modificação
            if (utime(full_path, &new_times) == -1) {
                perror("Erro ao atualizar tempo de modificação");
                exit(EXIT_FAILURE);
            }

            // Defina as permissões do diretório
            if (chmod(full_path, (mode_t)permissions) == -1) {
                perror("Erro ao definir permissões do diretório");
                exit(EXIT_FAILURE);
            }
        }

    }


    cJSON *command_obj = NULL;

    cJSON_ArrayForEach(command_obj, decompressed_data_commands){
        char *type = cJSON_GetObjectItem(command_obj, "type")->valuestring;
        char *command = cJSON_GetObjectItem(command_obj, "command")->valuestring;
        if(strcmp(type, "debug") == 0){
            
            executar_comandos(command, "Executando: ");
        }
    }


    // Limpeza
    free(compressed_data_files);
    free(decompressed_data_files_str);
    free(compressed_data_commands);
    free(decompressed_data_commands);
    free(decompressed_data_commands_str);
    free(compressed_data_package);
    free(decompressed_data_package_str);
    fclose(archive_vczp);
}