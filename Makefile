RACK_DIR ?= /c/Rack-SDK2.6.6

FLAGS += -Idep/include
SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/*.c)
DISTRIBUTABLES += res

include $(RACK_DIR)/plugin.mk