all: bin/scan bin/rfcomm-server bin/btput bin/btget

bin/scan: scan.cpp
	g++ scan.cpp -o bin/scan -lbluetooth -std=c++17 -Wall

bin/rfcomm-server: rfcomm-server.cpp
	g++ rfcomm-server.cpp -o bin/rfcomm-server -lbluetooth -std=c++17 -Wall

bin/btput: btput.cpp
	g++ btput.cpp -o bin/btput -lbluetooth -std=c++17 -Wall

bin/btget: btget.cpp
	g++ btget.cpp -o bin/btget -lbluetooth -std=c++17 -Wall

clean:
	rm -f bin/scan bin/rfcomm-server bin/btput bin/btget
