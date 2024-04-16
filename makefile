.PHONY: all clean

all: TCP_Receiver TCP_Sender RUDP_Receiver RUDP_Sender

TCP_Receiver: TCP_Receiver.o
	@gcc -o TCP_Receiver TCP_Receiver.o

TCP_Sender: TCP_Sender.o
	@gcc -o TCP_Sender TCP_Sender.o

TCP_Receiver.o: TCP_Receiver.c
	@gcc -c TCP_Receiver.c

TCP_Sender.o: TCP_Sender.c
	@gcc -c TCP_Sender.c

RUDP_Receiver: RUDP_Receiver.o RUDP_API.o
	@gcc -o RUDP_Receiver RUDP_Receiver.o RUDP_API.o

RUDP_Sender: RUDP_Sender.o RUDP_API.o
	@gcc -o RUDP_Sender RUDP_Sender.o RUDP_API.o

RUDP_Receiver.o: RUDP_Receiver.c
	@gcc -c RUDP_Receiver.c

RUDP_Sender.o: RUDP_Sender.c 
	@gcc -c RUDP_Sender.c

RUDP_API.o: RUDP_API.c RUDP_API.h
	@gcc -c RUDP_API.c


clean:
	@rm -f *.o TCP_Receiver TCP_Sender RUDP_Receiver RUDP_Sender