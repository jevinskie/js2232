TARGETS := loopback_test_async
all: $(TARGETS)

CXXFLAGS += -Wall -Wextra -g -std=c++20 -I ../3rdparty/scope-guard/include
LDFLAGS +=
LDLIBS +=  -lusb-1.0 -lfmt

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^

%: %.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

all: $(TARGETS)
.SUFFIXES:
.PHONY: clean
clean:
	rm -f $(TARGETS)
