all: scan rfcomm-server btput btget

scan: scan.cpp
	g++ scan.cpp -o bin/scan -lbluetooth -std=c++17 -Wall

rfcomm-server: rfcomm-server.cpp
	g++ rfcomm-server.cpp -o bin/rfcomm-server -lbluetooth -std=c++17 -Wall

btput: btput.cpp
	g++ btput.cpp -o bin/btput -lbluetooth -std=c++17 -Wall

btget: btget.cpp
	g++ btget.cpp -o bin/btget -lbluetooth -std=c++17 -Wall

clean:
	rm -f bin/*
