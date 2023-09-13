all: final

final:
	gcc -c BibakBOXServer.c
	gcc -c BibakBOXClient.c
	gcc BibakBOXClient.o -o BibakBOXClient -lrt -lm -lpthread -Wall -std=c99 -pedantic 
	gcc BibakBOXServer.o -o BibakBOXServer -lrt -lm -lpthread -Wall -std=c99 -pedantic 

clean:
	rm *.o BibakBOXClient BibakBOXServer
