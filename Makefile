CXX=g++
CXXFLAGS=-std=c++17 -Wall -Werror
LDLIBS=-lbluetooth
SRCDIR=src
BINDIR=bin
TARGETS=$(BINDIR)/scan $(BINDIR)/rfcomm-server $(BINDIR)/btput $(BINDIR)/btget

all: $(TARGETS)

$(BINDIR)/scan: $(SRCDIR)/scan.cpp
	@mkdir -p $(@D)
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDLIBS)

$(BINDIR)/rfcomm-server: $(SRCDIR)/rfcomm-server.cpp $(SRCDIR)/common.cpp $(SRCDIR)/common.h
	@mkdir -p $(@D)
	$(CXX) $< $(SRCDIR)/common.cpp -o $@ $(CXXFLAGS) $(LDLIBS)

$(BINDIR)/btput: $(SRCDIR)/btput.cpp $(SRCDIR)/common.cpp $(SRCDIR)/common.h
	@mkdir -p $(@D)
	$(CXX) $< $(SRCDIR)/common.cpp -o $@ $(CXXFLAGS) $(LDLIBS)

$(BINDIR)/btget: $(SRCDIR)/btget.cpp $(SRCDIR)/common.cpp $(SRCDIR)/common.h
	@mkdir -p $(@D)
	$(CXX) $< $(SRCDIR)/common.cpp -o $@ $(CXXFLAGS) $(LDLIBS)

clean:
	rm -rf $(BINDIR)
