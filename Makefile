# Makefile for macOS (OpenGL + GLUT)
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
FRAMEWORKS = -framework OpenGL -framework GLUT

SRC = main.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = Assignment3

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(FRAMEWORKS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) models/bound-lo-sphere.smf

clean:
	rm -f $(OBJ) $(TARGET)
