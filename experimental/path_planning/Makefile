# Name of target output
TARGET = main

# Setting up the directories
CD = $(CURDIR)
SD = $(CD)/src
SDP = $(SD)/path_planning
TD = $(CD)/tests
BD = $(CD)/build

# Setting the compiler
CC = g++

# Compiler flags
# -g: Turns on debugging with gdb
# -Wall: Turns all warnings on
# -I: Specifies the search directory for g++
FLAGS = -g -Wall -I$(CD)

# Linker flags
LFLAGS =


# Main rule that runs by default
.PHONY: all
all: setup $(TARGET)


# General compiling for a single file
$(BD)/%.o: $(SD)/%.cpp
	$(CC) $(FLAGS) -c $< -o $@

$(BD)/%.o: $(SDP)/%.cpp
	$(CC) $(FLAGS) -c $< -o $@

$(BD)/%.o: $(TD)/%.cpp
	$(CC) $(FLAGS) -c $< -o $@


# Getting all the source files to compile
SRCS := $(wildcard $(SD)/*.cpp)
SRCS += $(wildcard $(SDP)/*.cpp)
SRCS += $(wildcard $(TD)/*.cpp)

# Getting respective build file names, with .o
OBJS = $(patsubst $(SD)/%.cpp, $(BD)/%.o, $(patsubst $(SDP)/%.cpp, $(BD)/%.o, $(patsubst $(TD)/%.cpp, $(BD)/%.o, $(SRCS))))

# Linking everything together
$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $^


# Sets up the directory structure
.PHONY: setup
setup:
	mkdir -p build/
	mkdir -p tests/mazes/

# Removes any previous build/test files from prior run
.PHONY: clean
clean:
	rm -f *.exe
	rm -rf tests/mazes/*
	rm -rf $(BD)/*


# Function to help debug
print_objs:
	@echo $(OBJS)
