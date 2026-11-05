CXX ?= g++
CPPFLAGS := -Iinclude -Iapp
CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDLIBS :=
CORE := src/validation.cpp src/planner.cpp src/verify.cpp src/summary.cpp
WIRE := app/json_io.cpp
TESTS := tests/test_main.cpp tests/test_geometry.cpp tests/test_adversarial.cpp tests/test_json.cpp
HEADERS := $(wildcard include/guillotine/*.hpp src/*.hpp app/*.hpp tests/*.hpp)
SANFLAGS := -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

.PHONY: all test sanitize-test demo demo-json
all: build/cutplan

build:
	mkdir -p build

build/cutplan: $(CORE) $(WIRE) app/main.cpp app/demo.cpp $(HEADERS) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE) $(WIRE) app/main.cpp app/demo.cpp $(LDLIBS) -o $@

build/tests: $(CORE) $(WIRE) $(TESTS) $(HEADERS) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE) $(WIRE) $(TESTS) $(LDLIBS) -o $@

build/tests-sanitize: $(CORE) $(WIRE) $(TESTS) $(HEADERS) | build
	$(CXX) $(CPPFLAGS) $(SANFLAGS) $(CORE) $(WIRE) $(TESTS) $(LDLIBS) -o $@

build/cutplan-sanitize: $(CORE) $(WIRE) app/main.cpp app/demo.cpp $(HEADERS) | build
	$(CXX) $(CPPFLAGS) $(SANFLAGS) $(CORE) $(WIRE) app/main.cpp app/demo.cpp $(LDLIBS) -o $@

test: build/tests build/cutplan
	./build/tests
	bash tests/cli.sh ./build/cutplan

sanitize-test: build/tests-sanitize build/cutplan-sanitize
	./build/tests-sanitize
	bash tests/cli.sh ./build/cutplan-sanitize

demo: build/cutplan
	./build/cutplan demo

demo-json: build/cutplan
	./build/cutplan demo --json
