all:client6
client1:client1.cpp
	g++ -o $@ $^ -std=c++11 -lpthread

client2:client2.cpp
	g++ -o $@ $^ -std=c++11 -lpthread

client3:client3.cpp
	g++ -o $@ $^ -std=c++11 -lpthread

client4:client4.cpp
	g++ -o $@ $^ -std=c++11 -lpthread

client5:client5.cpp
	g++ -o $@ $^ -std=c++11 -lpthread

client6:client6.cpp
	g++ -o $@ $^ -std=c++11 -lpthread
	
tcpsever:Tcpserver.cc
	g++ -o $@ $^ -std=c++11 -lpthread
client:test_tcpcli.cc
	g++ -o $@ $^ -std=c++11 -lpthread
server:test_tcpsrv.cc
	g++ -o $@ $^ -std=c++11 -lpthread