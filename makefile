CXX = g++
CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -fsanitize=address
LDFLAGS = -fsanitize=address

SRC = main.cpp memory.cpp cartridge.cpp cpu.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = gbemu

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)