liblssrv: src/*.c
	gcc -g -Wall -fPIC -shared -I../lsnet/include -Iinclude -llsnet -L../lsnet src/*.c -o liblssrv.so -lpthread
server: test/main.c
	gcc -g -Wall -I../lsnet/include -Iinclude test/main.c ./liblssrv.so ../lsnet/liblsnet.so -o server -lpthread
#	gcc -g -Wall -I../lsnet/include -Iinclude -L../lsnet -L. -llsnet -llssrv test/*.c -o server -lpthread
client: test/client.c
	gcc -g -Wall test/client.c -o client -lpthread
clean:
	rm liblssrv.so
	rm server
