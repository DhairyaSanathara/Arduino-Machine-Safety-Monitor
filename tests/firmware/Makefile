# Host-side firmware tests: compile the real firmware against mocks and run it on a PC.
#   make test          build and run every test
#   make test T=stall  run only tests whose name contains "stall"
FW      := ../../firmware/MachineSafetyMonitor
CXX     ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-misleading-indentation -Wno-format-truncation -Wno-stringop-truncation
INCLUDES := -Imocks -I$(FW) -I$(FW)/src
SOURCES  := test_safety.cpp sim.cpp $(wildcard $(FW)/src/*.cpp) build/main_ino.cpp

.PHONY: test clean
test: build/run_tests
	./build/run_tests $(T)

build/main_ino.cpp: $(FW)/MachineSafetyMonitor.ino
	@mkdir -p build
	@{ echo '#include <Arduino.h>'; cat $<; } > $@

build/run_tests: $(SOURCES) $(wildcard mocks/*.h) $(wildcard $(FW)/src/*.h) $(FW)/config.h sim.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $@

clean:
	rm -rf build
