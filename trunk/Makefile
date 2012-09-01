liblssrv: src/*.c
	gcc -g -Wall -fPIC -shared -I../lsnet/include -Iinclude -llsnet -L../lsnet -lpthread src/*.c -o liblssrv.so
clean:
	rm liblssrv.so
