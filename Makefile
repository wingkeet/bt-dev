all: bin/scan bin/rfcomm-server bin/btput bin/btget

bin/scan: src/scan.cpp
	g++ src/scan.cpp -o bin/scan -lbluetooth -std=c++17 -Wall

bin/rfcomm-server: src/rfcomm-server.cpp src/common.cpp src/common.h
	g++ src/rfcomm-server.cpp src/common.cpp -o bin/rfcomm-server -lbluetooth -std=c++17 -Wall

bin/btput: src/btput.cpp src/common.cpp src/common.h
	g++ src/btput.cpp src/common.cpp -o bin/btput -lbluetooth -std=c++17 -Wall

bin/btget: src/btget.cpp src/common.cpp src/common.h
	g++ src/btget.cpp src/common.cpp -o bin/btget -lbluetooth -std=c++17 -Wall

clean:
	rm -f bin/scan bin/rfcomm-server bin/btput bin/btget
