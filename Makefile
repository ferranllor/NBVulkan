# Variables for compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -fopenmp -Ofast -march=native -lm
LIBS = -lglfw -lvulkan -fopenmp -fopenmp -Ofast -march=native -lm

# Name of the final executable
TARGET = exec

# The default rule that runs when you just type 'make'
all: $(TARGET)

# Rule to link the object files and create the executable
$(TARGET): main.o
	$(CXX) main.o -o $(TARGET) $(LIBS)

# Rule to compile main.cpp into an object file
main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp

# Clean rule to remove compiled files and start fresh
clean:
	rm -f *.o $(TARGET)