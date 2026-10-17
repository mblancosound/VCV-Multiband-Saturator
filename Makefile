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

include $(RACK_DIR)/plugin.mk