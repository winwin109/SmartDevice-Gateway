all: server client

server : server.cpp message_parser.cpp protocol.cpp device_manager.cpp
	g++ -o server server.cpp message_parser.cpp protocol.cpp device_manager.cpp -lpthread -ljsoncpp -lmysqlclient

client : client.cpp protocol.cpp
	g++ -o client client.cpp protocol.cpp -lpthread -ljsoncpp

clean:
	rm -rf server client *.o