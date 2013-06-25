liblssrv: src/*.c src/*.cpp
	g++ -g -Wall -fPIC -shared -D__CPLUSPLUS -I../lsnet/include -Iinclude -llsnet -L../lsnet src/*.c src/*.cpp -o liblssrv.so -lpthread
server: test/main.c
	g++ -g -Wall -D__CPLUSPLUS -I../lsnet/include -Iinclude test/main.c ../lsnet/liblsnet.so ./liblssrv.so -o server -lpthread
server2: test/main2.cpp
	g++ -g -Wall -D__CPLUSPLUS -DDEBUG_EPEX -I../lsnet/include -Iinclude test/main2.cpp src/*.c src/*.cpp ../lsnet/src/*.c -o server2 -lpthread
client: test/client.c
	g++ -g -Wall test/client.c -o client -lpthread
client2: test/client2.cpp
	g++ -g -Wall test/client2.cpp -o client2 -lpthread
clean:
	rm liblssrv.so
	rm server
	rm client
