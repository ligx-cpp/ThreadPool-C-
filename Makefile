CC=g++
CC_FLAG= -std=c++11
INCLUDE=-I.
LIB=-pthread


SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp,%.o,$(SRCS))

target=main
$(target):$(OBJS)
	$(CC) $(LIB) $^ -o $@
%.o:%.cpp
	$(CC) -c $^ $(INCLUDE) $(CC_FLAG)  -o $@
.PRONY:clean
clean:	
	@echo "Removing linked and compiled files......"	
	rm -f *.o
