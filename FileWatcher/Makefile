CXX = g++

OBJS = FileWatcher.o MonitoredFile.o

all: $(OBJS)

clean:
	rm -rf testApp *.o test/*.o

tests: all
	$(CXX) -c -o test/Main.o test/Main.cpp
	$(CXX) -c -o test/TestObserver.o test/TestObserver.cpp
	$(CXX) -o testApp test/Main.o test/TestObserver.o $(OBJS)

%.o: %.cpp
	$(CXX) -c -o $@ $<
