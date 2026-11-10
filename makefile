all: server client

server : server.cpp
	g++ -o server server.cpp -lpthread -ljsoncpp -lmysqlclient

client : client.cpp
	g++ -o client client.cpp -lpthread -ljsoncpp

clean:
	rm -rf server client *.o