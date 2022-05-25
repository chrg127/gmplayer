CXX := g++
CXXFLAGS := -g -std=c++20 -Wall -Wextra -pedantic
CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -pedantic
LDLIBS := -lgme -lSDL2 -lfmt

all: main
