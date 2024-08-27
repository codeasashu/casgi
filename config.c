#include "asgi.h"

asgi_config *read_config(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    printf("Could not open config file: %s\n", filename);
    return NULL;
  }

  // Determine the file size
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Read the file contents into a buffer
  char *buffer = (char *)malloc(file_size + 1);
  if (!buffer) {
    printf("Memory allocation error\n");
    fclose(file);
    return NULL;
  }
  fread(buffer, 1, file_size, file);
  buffer[file_size] = '\0'; // Null-terminate the string

  fclose(file);

  // Parse the JSON
  cJSON *json = cJSON_Parse(buffer);
  if (!json) {
    printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
    free(buffer);
    return NULL;
  }

  // Create and populate the asgi_config struct
  asgi_config *config = (asgi_config *)malloc(sizeof(asgi_config));
  if (!config) {
    printf("Memory allocation error\n");
    cJSON_Delete(json);
    free(buffer);
    return NULL;
  }

  // Extract "app_path" from the JSON
  cJSON *apppath_json = cJSON_GetObjectItem(json, "app_path");
  if (cJSON_IsString(apppath_json) && (apppath_json->valuestring != NULL)) {
    strncpy(config->app_path, apppath_json->valuestring,
            sizeof(config->app_path) - 1);
    config->app_path[sizeof(config->app_path) - 1] =
        '\0'; // Ensure null termination
  } else {
    printf("Invalid or missing 'apppath' in the configuration file.\n");
    free(config);
    config = NULL;
  }

  cJSON *module = cJSON_GetObjectItem(json, "module");
  if (cJSON_IsString(module) && (module->valuestring != NULL)) {
    strncpy(config->module, module->valuestring, sizeof(config->module) - 1);
    config->module[sizeof(config->module) - 1] =
        '\0'; // Ensure null termination
  } else {
    printf("Invalid or missing 'module' in the configuration file.\n");
    free(config);
    config = NULL;
  }

  cJSON *pyhome = cJSON_GetObjectItem(json, "home");
  if (cJSON_IsString(pyhome) && (pyhome->valuestring != NULL)) {
    printf("PYHOME: %s.\n", pyhome->valuestring);
    // config->pyhome = malloc(strlen(pyhome->valuestring) + 1);
    // memset(config->pyhome, '\0', sizeof(pyhome->valuestring) + 1);
    strncpy(config->pyhome, pyhome->valuestring, sizeof(config->pyhome) - 1);
    config->pyhome[sizeof(config->pyhome) - 1] =
        '\0'; // Ensure null termination
    printf("PYHOME2: %s.\n", config->pyhome);
  }

  cJSON *socketname = cJSON_GetObjectItem(json, "socket_name");
  if (cJSON_IsString(socketname) && (socketname->valuestring != NULL)) {
    config->socket_name = malloc(strlen(socketname->valuestring) + 1);
    if (config->socket_name == NULL) {
      printf("[ERROR] Memory allocation failed for socket_name.\n");
      exit(1); // Handle error appropriately in your program
    }
    strncpy(config->socket_name, socketname->valuestring,
            sizeof(config->socket_name) - 1);
    config->socket_name[sizeof(config->socket_name) - 1] =
        '\0'; // Ensure null termination
  } else {
    printf(
        "[WARN] Invalid or missing 'socket_name' in the configuration file.\n");
  }

  // Clean up
  cJSON_Delete(json);
  free(buffer);

  return config;
}
