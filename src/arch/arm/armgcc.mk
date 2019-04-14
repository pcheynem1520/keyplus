# Copyright 2019 jem@seethis.link
# Licensed under the MIT license (http://opensource.org/licenses/MIT)

# Toolchain commands
CC      := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-gcc
CXX     := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-c++
AS      := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-as
AR      := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-ar -r
LD      := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-ld
NM      := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-nm
OBJDUMP := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-objdump
OBJCOPY := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-objcopy
SIZE    := $(GNU_INSTALL_ROOT)$(GNU_PREFIX)-size

LD_INPUT               = $^ $(LIB_FILES)

#######################################################################
#                define object files from source files                #
#######################################################################

include $(KEYPLUS_PATH)/obj_file.mk

C_OBJ_FILES = $(call obj_file_list, $(SRC_FILES),o)
DEP_FILES = $(call obj_file_list, $(SRC_FILES),d)

ASM_OBJ_FILES = $(call obj_file_list, $(ASM_FILES),o)

OBJ_FILES = $(C_OBJ_FILES) $(ASM_OBJ_FILES)

TARGET_OUT = $(BUILD_TARGET_DIR)/$(TARGET).out
TARGET_BIN = $(BUILD_TARGET_DIR)/$(TARGET).bin

INC_PATHS = $(addprefix -I,$(INC_FOLDERS))

#######################################################################
#                        extra compiler flags                         #
#######################################################################

# Compiler flags to generate dependency files.
GENDEPFLAGS = -MMD -MP

# Combine all necessary flags and optional flags.
# Add target processor to flags.
CFLAGS += -std=c99 $(GENDEPFLAGS)
ASMFLAGS += $(ASFLAGS)

#######################################################################
#                         object file recipes                         #
#######################################################################

define c_file_recipe
	@echo "compiling: $$<"
	@$(CC) $$(CFLAGS) $$(INC_PATHS) -o $$@ -c $$<
endef

define asm_file_recipe
	@echo "assembling: $$<"
	@$(CC) -x assembler-with-cpp $$(ASMFLAGS) $$(INC_PATHS) -o $$@ -c $$<
endef
# dd@$(AS) $$(ASFLAGS) $$@ $$<


# Create the recipes for the object files
$(call create_recipes, $(SRC_FILES),c_file_recipe,o)
$(call create_recipes, $(ASM_FILES),asm_file_recipe,o)

# Link object files
$(TARGET_OUT): $(OBJ_FILES)
	@echo
	@echo Linking target: $(TARGET_OUT)
	$(eval LD_INPUT := $(@:.out=.in))
	@echo $(OBJ_FILES) $(LIB_FILES) > $(LD_INPUT)
	$(CC) $(LDFLAGS) @$(LD_INPUT) -Wl,-Map=$(@:.out=.map) -o $@
	@echo
	@echo '### Size Info ###'
	$(SIZE) $@
	@echo


# Create binary .bin file from the .out file
$(TARGET_BIN): $(TARGET_OUT)
	@$(NO_ECHO)$(OBJCOPY) -O binary $< $@

# Create binary .hex file from the .out file
$(TARGET_HEX): $(TARGET_OUT)
	@$(NO_ECHO)$(OBJCOPY) -O ihex $< $@

.PHONY: clean
clean:
	rm -r $(BUILD_TARGET_DIR)

# Don't need to do anything for dep files since they are generated by the compiler
%.d: