PROJECT_NAME := cloud

JS_BOARD := board_esp32_devkitc.h
EXTRA_COMPONENT_DIRS += $(PROJECT_PATH)/../components

include $(IDF_PATH)/make/project.mk
CPPFLAGS += -DJS_BOARD=\"$(JS_BOARD)\"
