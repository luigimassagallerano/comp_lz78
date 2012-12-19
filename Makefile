CFLAGS = -Wall -Werror -O2 
CC = cc 

lz78.o: lz78.h bit.h

bit.o: bit.h

main.o: lz78.h bit.h	
	$(CC) -c $(CFLAGS) main.c

lz78: main.o lz78.o bit.o
	$(CC) -lm -o lz78 $^ $(LDFLAGS)
	
clean: 
	rm *.o lz78 compresso

