# Variables for compiler and flags
CXX = g++
IMGUI_DIR = imgui

CXXFLAGS = -std=c++20 -Wall -Wextra -fopenmp -Ofast -march=native -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
LIBS = -lglfw -lvulkan -fopenmp -Ofast -march=native -lm

OBJ_DIR = obj
BIN_DIR = bin

TARGET = $(BIN_DIR)/exec

OBJS = $(addprefix $(OBJ_DIR)/, \
       main.o \
       imgui.o \
       imgui_draw.o \
       imgui_widgets.o \
       imgui_tables.o \
       imgui_impl_glfw.o \
       imgui_impl_vulkan.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

$(OBJ_DIR)/main.o: main.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(IMGUI_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(IMGUI_DIR)/backends/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)