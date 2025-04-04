all:
	g++ -std=c++20 -c lsm.cpp -o lsm.o -pthread
	gcc -c server.c -pthread
	g++ -std=c++20 server.o lsm.o -o server -pthread
	g++ client.c -o client
	
clean:
	rm -f *.o
	rm -rf SSTable_*
	rm server