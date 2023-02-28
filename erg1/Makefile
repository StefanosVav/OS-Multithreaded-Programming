all: parent child

parent: parent.c
	gcc -o parent parent.c -lpthread

child: child.c
	gcc -o child child.c -lpthread

clean:
	rm -f child parent test.txt
