#include "asgi.h"

int parse_agi_line(const char *line, struct agi_pair *pair) {
  // Find the location of the ':'
  char *colon = strchr(line, ':');
  if (colon == NULL) {
    return -1; // Invalid line format
  }

  // Copy the key part (before the colon)
  size_t key_len = colon - line;
  strncpy(pair->key, line, key_len);
  pair->key[key_len] = '\0'; // Null-terminate the key

  // Copy the value part (after the colon and spaces)
  char *value_start = colon + 1;
  while (*value_start == ' ')
    value_start++; // Skip spaces

  strncpy(pair->value, value_start, MAX_VALUE_LEN - 1);
  pair->value[MAX_VALUE_LEN - 1] = '\0'; // Null-terminate the value

  return 0;
}

struct agi_pair *parse_agi_line_b(const char *line) {
  struct agi_pair *pair;
  pair = malloc(sizeof(struct agi_pair) + 2);
  memset(pair, 0, sizeof(struct agi_pair) + 2);
  // Find the location of the ':'
  char *colon = strchr(line, ':');
  if (colon == NULL) {
    return NULL;
  }

  // Copy the key part (before the colon)
  size_t key_len = colon - line;
  strncpy(pair->key, line, key_len);
  pair->key[key_len] = '\0'; // Null-terminate the key

  // Copy the value part (after the colon and spaces)
  char *value_start = colon + 1;
  while (*value_start == ' ')
    value_start++;

  strncpy(pair->value, value_start, MAX_VALUE_LEN - 1);
  pair->value[MAX_VALUE_LEN - 1] = '\0'; // Null-terminate the value
  return pair;
}

int parse_agi_data(char *data, struct agi_header *header) {
  int count = 0;
  struct agi_pair *pair;
  char *line = strtok(data, "\n");

  while (line != NULL) {
    if ((pair = parse_agi_line_b(line)) != NULL) {
      header->env[count] = *pair;
      count++;
    }
    line = strtok(NULL, "\n");
  }
  header->env_lines = count;
  return count;
}
