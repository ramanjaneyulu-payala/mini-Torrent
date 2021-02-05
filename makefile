fileshare: client.cpp
	g++ client.cpp -o client -w -lpthread -lssl -lcrypto
	g++ tracker.cpp -o tracker -lpthread
