
output: main.c
	gcc -o output main.c cJSON.c -I/usr/include/python3.12 -lpython3.12

clean:
	rm -rf output

