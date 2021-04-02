CXX = g++
CFLAGS = -std=c++14 -O -Wall -g

TARGET = server
OBJS = ./log/*.cc ./mysql/*.cc ./timer/*.cc ./epoller/*.cc  ./http/*.cc  \
	   ./webserver/*.cc ./buffer/*.cc ./config/*.hpp ./threadpool/*.hpp ./main.cc

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET) -pthread -lmysqlclient

clean:
	rm $(TARGET)

# g++ -std=c++14 -O2 -Wall -g -MM main.cc -o server -pthread -lmysqlclient