liblssrv: src/*.c src/*.cpp
	g++ -g -Wall -fPIC -shared -D__CPLUSPLUS -I../lsnet/include -Iinclude -llsnet -L../lsnet src/*.c src/*.cpp -o liblssrv.so -lpthread
server: test/main.c
	g++ -g -Wall -D__CPLUSPLUS -I../lsnet/include -Iinclude test/main.c ../lsnet/liblsnet.so ./liblssrv.so -o server -lpthread
client: test/client.c
	g++ -g -Wall test/client.c -o client -lpthread
clean:
	rm liblssrv.so
	rm server
	rm client
