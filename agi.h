#define MAX_AGI_LINES 20
#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 128

struct agi_pair {
  char key[MAX_KEY_LEN];
  char value[MAX_VALUE_LEN];
};

struct agi_header {
  struct agi_pair *env;
};

int parse_agi_data(char *, struct agi_header *);
