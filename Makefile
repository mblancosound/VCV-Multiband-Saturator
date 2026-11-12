# B1: plugin.mk requiere `jq` (MSYS2: /mingw64/bin/jq). Asegura PATH + mensaje claro.
export PATH := /mingw64/bin:$(PATH)
ifeq (,$(shell which jq 2>/dev/null))
$(error falta 'jq' en PATH - instala con: pacman -S mingw-w64-x86_64-jq (MSYS2 mingw64))
endif
RACK_DIR ?= /c/Rack-SDK2.6.6

# dep/include no existe (auditor verificado) - bandera redundante desactivada.
# FLAGS += -Idep/include
SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/*.c)
DISTRIBUTABLES += res
DISTRIBUTABLES += manual

# Arnes offline v2.6 (specs/v2.6_test_harness_spec.md seccion 10).
# Solo delega en test/Makefile: no toca plugin.mk ni requiere jq.
.PHONY: test test-strict
test:
	$(MAKE) -C test test

test-strict:
	$(MAKE) -C test test-strict

include $(RACK_DIR)/plugin.mk