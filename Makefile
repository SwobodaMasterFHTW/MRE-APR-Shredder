CXX = g++
CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -pthread
all: control test_tools
control: control.cpp shared_data.h
	$(CXX) $(CXXFLAGS) control.cpp -o control
test_tools: test_tools.cpp shared_data.h
	$(CXX) $(CXXFLAGS) test_tools.cpp -o test_tools
clean:
	rm -f control test_tools
