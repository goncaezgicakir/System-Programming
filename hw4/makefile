all: midterm

midterm:
	gcc -c biboClient.c
	gcc -c biboServer.c
	gcc biboClient.o -o biboClient -lrt -lm -lpthread -Wall -std=c99 -pedantic 
	gcc biboServer.o -o biboServer -lrt -lm -lpthread -Wall -std=c99 -pedantic 

clean:
	rm *.o biboClient biboServer
