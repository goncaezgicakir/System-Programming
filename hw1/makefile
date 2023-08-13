all: hw1

hw1:
	gcc -c appendMeMore.c part2.c part3.c
	gcc appendMeMore.o -o appendMeMore -Wall -std=c99 -pedantic 
	gcc part2.o -o part2 -Wall -std=c99 -pedantic 
	gcc part3.o -o part3 -Wall -std=c99 -pedantic 
clean:
	rm *.o  appendMeMore part2 part3
