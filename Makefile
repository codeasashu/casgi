PYTHON_CONFIG = python3.12-config
CFLAGS = $(shell $(PYTHON_CONFIG) --cflags)
LDFLAGS = $(shell $(PYTHON_CONFIG) --ldflags)

output: main.c python.c config.c socket.c agi.c
	gcc -o output main.c cJSON.c config.c python.c socket.c agi.c $(CFLAGS) $(LDFLAGS) -lpython3.12

clean:
	rm -rf output

