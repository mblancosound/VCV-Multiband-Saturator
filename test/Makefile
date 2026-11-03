# Arnes offline de Multiband Saturator - v2.6.0
# Especificacion: specs/v2.6_test_harness_spec.md
# Sin -lRack y sin <rack.hpp>: solo headers header-only del SDK.

RACK_DIR ?= /c/Rack-SDK2.6.6
CXX ?= g++
CXXSTD := -std=c++11 -D_USE_MATH_DEFINES -march=nehalem -Wall -Wextra
INCLUDES := -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include -I../src -Isupport

# Gate de release: flags identicos al modulo (fidelidad de alias y latencia).
RELEASE_FLAGS := -O3 -funsafe-math-optimizations
# Gate de finitud: sin fast-math, para que isfinite/fpclassify sean fiables.
STRICT_FLAGS := -O2 -fno-fast-math -ffp-contract=off

DEPS := harness.cpp support/metrics.hpp support/offline_chain.hpp \
        support/oversample_local.hpp ../src/Curves.hpp ../src/AutoMakeup.hpp

.PHONY: test test-strict freeze clean

test: harness_release.exe
	./harness_release.exe

test-strict: harness_strict.exe
	./harness_strict.exe

freeze: harness_release.exe
	@mkdir -p baseline
	./harness_release.exe --freeze

harness_release.exe: $(DEPS)
	$(CXX) $(CXXSTD) $(INCLUDES) $(RELEASE_FLAGS) harness.cpp -o $@

harness_strict.exe: $(DEPS)
	$(CXX) $(CXXSTD) $(INCLUDES) $(STRICT_FLAGS) harness.cpp -o $@

clean:
	rm -f harness_release.exe harness_strict.exe
