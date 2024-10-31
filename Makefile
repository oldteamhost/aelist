TARGET=sapps
CXX=g++
CXXFLAGS=-Wall -O3
SRCS=sapps.cc
OBJS=$(SRCS:.cc=.o)

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

