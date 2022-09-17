BUILD_DIR := build
TARGET := $(BUILD_DIR)/controlService

CFLAGS := -c -g -O0 -march=native -MD -I $(PWD)
LDFLAGS := -lcrypto

VPATH := $(shell find -type d)
SOURCES := $(foreach dir, $(VPATH), $(wildcard $(dir)/*.c))
OBJECTS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SOURCES)))

UI_SOURCES := $(foreach dir, $(VPATH), $(wildcard $(dir)/*.html) $(wildcard $(dir)/*.js))
UI_OBJECTS := $(patsubst %, $(BUILD_DIR)/%.ui.o, $(notdir $(UI_SOURCES)))

include $(wildcard $(BUILD_DIR)/*.d)

all: $(TARGET)
$(TARGET): $(OBJECTS) $(UI_OBJECTS)
	@ mkdir -p $(@D)
	gcc $(OBJECTS) $(UI_OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c
	@ mkdir -p $(@D)
	gcc $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.ui.o: $(BUILD_DIR)/%.ui.c
	@ mkdir -p $(@D)
	gcc $(CFLAGS) $< -o $@


$(BUILD_DIR)/%.ui.c: $(BUILD_DIR)/%.ui.min
	@ mkdir -p $(@D)
	xxd -i $< > $@

$(BUILD_DIR)/%.ui.min: %
	@ mkdir -p $(@D)
	minify $< -o $@

clean:
	rm -rf $(BUILD_DIR)
