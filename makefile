OBJS = gui.o parser.o program.o sim8085.o utility.o opcodecheck.o 8255ppi.o
 
CC = gcc
sim8085: $(OBJS)
	$(CC) -o sim8085 $(OBJS) `pkg-config gtk+-3.0 --libs` -pthread

gui.o: gui.c parser.h sim8085.h 8255ppi.h
	$(CC) gui.c -c -std=c99 `pkg-config gtk+-3.0 --cflags`

opcodecheck.o: opcodecheck.c opcodecheck.h
	$(CC) opcodecheck.c -c -std=c99

parser.o: parser.c parser.h program.h
	$(CC) parser.c -c -std=c99

program.o: program.c program.h
	$(CC) program.c -c -std=c99

sim8085.o: sim8085.c sim8085.h
	$(CC) sim8085.c -c -std=c99

utility.o: utility.c utility.h
	$(CC) utility.c -c -std=c99

8255ppi.o: 8255ppi.c 8255ppi.h
	$(CC) 8255ppi.c -c -std=c99
