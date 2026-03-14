CXX = g++
CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -fsanitize=address $(shell sdl2-config --cflags)
LDFLAGS = -fsanitize=address $(shell sdl2-config --libs)

SRC = main.cpp memory.cpp cartridge.cpp cpu.cpp ppu.cpp joypad.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = gbemu

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
