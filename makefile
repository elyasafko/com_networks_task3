.PHONY: all clean

all: TCP_Receiver TCP_Sender

TCP_Receiver: TCP_Receiver.o
	@gcc -o TCP_Receiver TCP_Receiver.o

TCP_Sender: TCP_Sender.o
	@gcc -o TCP_Sender TCP_Sender.o

TCP_Receiver.o: TCP_Receiver.c
	@gcc -c TCP_Receiver.c

TCP_Sender.o: TCP_Sender.c
	@gcc -c TCP_Sender.c


clean:
	@rm -f *.o TCP_Receiver TCP_Sender