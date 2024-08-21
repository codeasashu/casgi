
output: main.c python.c config.c
	gcc -o output main.c cJSON.c config.c python.c -I/usr/local/include/python3.10 -lpython3.10
	# gcc -o output main.c cJSON.c -I/usr/include/python3.12 -lpython3.12

clean:
	rm -rf output

