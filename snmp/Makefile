CC=gcc
CXX=g++
CFLAGS=
CXXFLAGS=
DBG=-g -Wall -Wextra -DDEBUG

driver: driver.cpp snmp.o deepgreen-snmp.o
	$(CXX) $(CXXFLAGS) -o $@ $^

debug: driver.cpp snmp-dbg.o deepgreen-snmp-dbg.o
	$(CXX) $(CXXFLAGS) $(DBG) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%-dbg.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DBG) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%-dbg.o: %.c
	$(CC) $(CFLAGS) $(DBG) -c -o $@ $<

.PHONY=clean
clean:
	rm -rf *.o driver debug
