all: bin/scan bin/rfcomm-server bin/btput bin/btget

bin/scan: src/scan.cpp
	g++ src/scan.cpp -o bin/scan -lbluetooth -std=c++17 -Wall

bin/common.o: src/common.cpp src/common.h
	g++ src/common.cpp -c -o bin/common.o -std=c++17 -Wall

bin/rfcomm-server: src/rfcomm-server.cpp bin/common.o
	g++ src/rfcomm-server.cpp bin/common.o -o bin/rfcomm-server -lbluetooth -std=c++17 -Wall

bin/btput: src/btput.cpp src/common.cpp  bin/common.o
	g++ src/btput.cpp bin/common.o -o bin/btput -lbluetooth -std=c++17 -Wall

bin/btget: src/btget.cpp src/common.cpp bin/common.o
	g++ src/btget.cpp bin/common.o -o bin/btget -lbluetooth -std=c++17 -Wall

clean:
	rm -f bin/scan bin/common.o bin/rfcomm-server bin/btput bin/btget
