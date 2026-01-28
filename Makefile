# Makefile for Nabreterm

CXX = g++
CXXFLAGS = -Wall -std=c++17
LDFLAGS = -lreadline -lhistory

SRC = main.cpp
TARGET = nabreterm

all: $(TARGET)

$(TARGET): $(SRC)
    $(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
    rm -f $(TARGET) *.o
