# Variables for compiler and flags
CXX = g++
IMGUI_DIR = ../imgui

CXXFLAGS = -std=c++20 -Wall -Wextra -fopenmp -Ofast -march=native -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
LIBS = -lglfw -lvulkan -fopenmp -Ofast -march=native -lm

TARGET = exec

OBJS = main.o \
       imgui.o \
       imgui_draw.o \
       imgui_widgets.o \
       imgui_tables.o \
       imgui_impl_glfw.o \
       imgui_impl_vulkan.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

imgui.o: $(IMGUI_DIR)/imgui.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

imgui_draw.o: $(IMGUI_DIR)/imgui_draw.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

imgui_widgets.o: $(IMGUI_DIR)/imgui_widgets.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

imgui_tables.o: $(IMGUI_DIR)/imgui_tables.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

imgui_impl_glfw.o: $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

imgui_impl_vulkan.o: $(IMGUI_DIR)/backends/imgui_impl_vulkan.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
