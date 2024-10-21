SRC = global.cpp util.cpp rsmt.cpp arch.cpp lib.cpp object.cpp netlist.cpp legal.cpp wirelength.cpp pindensity.cpp main.cpp
OBJ = $(SRC:.cpp=.o)
DEP = $(SRC:.cpp=.d)
CC = g++

CFLAGS = -Wall -Wextra -std=c++11
CFLAGS += -g
CFLAGS += -o3

all: checker

checker: $(OBJ)
	$(CC) $(CFLAGS) -o checker $(OBJ)

# 包含自动生成的依赖文件 (.d)
-include $(DEP)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f checker $(OBJ)
