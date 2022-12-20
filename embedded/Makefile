# Makefile arguments
#
#
# Delete the default suffix rules.
.SUFFIXES:
.SECONDARY:

GTEST_DIR   = ../third_party/gtest/googletest/
GMOCK_DIR   = ../third_party/gtest/googlemock/
MBEDTLS_DIR = ../third_party/mbedtls/


ARCH        ?= host
OUT_DIR_NAME ?= $(ARCH)
OUT_DIR     = out/$(OUT_DIR_NAME)
DIST_DIR    = dist/$(OUT_DIR_NAME)

CFLAGS := -Wall
COMMON_INCLUDE_DIRS :=
CLIENT_INCLUDES :=
# For ARCH variants like gLinux_hw set to gLinux.
ARCH_COMMON_NAME := $(ARCH)
ARCH_COMMON_DIR = common/source/$(ARCH_COMMON_NAME)
CLIENT_DIR = client/source
CLIENT_SRCS :=

NEARBY_LIB_NAME = libnearby.a

NAME = $(OUT_DIR)/$(NEARBY_LIB_NAME)

all : $(NAME)

COMMON_DIR  = common/source
COMMON_C_FILES :=

# All output objects for the build system should be added to ALL_OBJS.
ALL_OBJS :=
# Extra dependencies of the dist target.
DIST_LIST :=

# Name of the libnearby static library that we distribute.
DIST_NEARBY_LIB_NAME ?= $(DIST_DIR)/$(NEARBY_LIB_NAME)

# Kokoro on Windows is wonky. We can't shell out to git because we are in
# cygwin, so this is pre-computed by continuous.bat
ifneq ($(FIRMWARE_VERSION_FILENAME),)
FIRMWARE_VERSION := $(shell cat $(FIRMWARE_VERSION_FILENAME))
else
FIRMWARE_VERSION_FILENAME = $(OUT_DIR)/FIRMWARE_VERSION
PHONY_VERSION_FILENAME = $(OUT_DIR)/PHONY_FIRMWARE_VERSION
FIRMWARE_VERSION := $(shell git -c core.fileMode=false describe --always --dirty --exclude="*")
FIRMWARE_VERSION_FILENAME: $(PHONY_VERSION_FILENAME)

ifneq ($(KOKORO_RELEASE_BUILD),1)
FIRMWARE_VERSION := $(FIRMWARE_VERSION)ENG
endif

# Update the phony version on every build.
.PHONY: $(PHONY_VERSION_FILENAME)
$(PHONY_VERSION_FILENAME):
	@mkdir -p $(dir $@) ;\
	echo $(FIRMWARE_VERSION) > $@ ;\

# Only update this version if necessary (if it's different from the phony). This
# tricks make into not updating dependencies of FIRMWARE_VERSION_FILENAME unless
# the file is updated. .PHONY is not transitive.
$(FIRMWARE_VERSION_FILENAME): $(PHONY_VERSION_FILENAME)
	@cmp $@ $< 2>> /dev/null || cp -v $< $@

endif

# Note $(notdir) below converts a path to just the base file name
define compile_c
	mkdir -p $(dir $@)
	$(CC) -c $(1) -o $@ -D__NEARBY_SHORT_FILE__='"$(notdir $<)"' $<
endef

# Include all of the target specific variables and rules.
# The $(ARCH).mk files should only depend on the variables listed above.
include target/$(ARCH)/config.mk

# Set trace verbosity from target specific variable
CFLAGS += -DNEARBY_TRACE_LEVEL=NEARBY_TRACE_LEVEL_$(NEARBY_TRACE_LEVEL)
# if directory doesn't exist, raise an error so we don't fail later with more cryptic warnings
ifeq ($(wildcard common/target/$(ARCH_COMMON_NAME)),)
$(error need to create directory common/target/$(ARCH_COMMON_NAME) )
endif

ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
CFLAGS += -DNEARBY_FP_ENABLE_BATTERY_NOTIFICATION=$(NEARBY_FP_ENABLE_BATTERY_NOTIFICATION)
endif

ifdef NEARBY_FP_ENABLE_ADDITIONAL_DATA
CFLAGS += -DNEARBY_FP_ENABLE_ADDITIONAL_DATA=$(NEARBY_FP_ENABLE_ADDITIONAL_DATA)
endif

ifdef NEARBY_FP_MESSAGE_STREAM
CFLAGS += -DNEARBY_FP_MESSAGE_STREAM=$(NEARBY_FP_MESSAGE_STREAM)
endif

ifdef NEARBY_FP_RETROACTIVE_PAIRING
CFLAGS += -DNEARBY_FP_RETROACTIVE_PAIRING=$(NEARBY_FP_RETROACTIVE_PAIRING)
endif

ifdef NEARBY_FP_BLE_ONLY
CFLAGS += -DNEARBY_FP_BLE_ONLY=$(NEARBY_FP_BLE_ONLY)
endif

ifdef NEARBY_FP_PREFER_BLE_BONDING
CFLAGS += -DNEARBY_FP_PREFER_BLE_BONDING=$(NEARBY_FP_PREFER_BLE_BONDING)
endif

ifdef NEARBY_FP_PREFER_LE_TRANSPORT
CFLAGS += -DNEARBY_FP_PREFER_LE_TRANSPORT=$(NEARBY_FP_PREFER_LE_TRANSPORT)
endif
COMMON_INCLUDE_DIRS += \
    -I. \
    -I$(ARCH_COMMON_DIR) \
    -Icommon/target \
    -Icommon/target/$(ARCH_COMMON_NAME) \
    -I$(COMMON_DIR)
ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
COMMON_INCLUDE_DIRS += -I$(MBEDTLS_DIR)/include
endif

CLIENT_INCLUDES += \
    -I$(CLIENT_DIR) \
    -Iclient/target

COMMON_C_FILES += \
    $(wildcard $(COMMON_DIR)/*.c)

CLIENT_SRCS += \
    $(wildcard $(CLIENT_DIR)/*.c)

CFLAGS += $(COMMON_INCLUDE_DIRS)

CROSS_COMPILE ?= arm-none-eabi-
CC     ?= $(CROSS_COMPILE)gcc
AR     ?= $(CROSS_COMPILE)ar
ALL_C_FILES = $(COMMON_C_FILES)
COMMON_OBJS = $(patsubst %.c,$(OUT_DIR)/%.o,$(ALL_C_FILES))
CLIENT_OBJS = $(patsubst %.c,$(OUT_DIR)/%.o,$(CLIENT_SRCS))
NEARBY_LIB_OBJS = $(COMMON_OBJS) $(CLIENT_OBJS)

NEARBY_HEADERS += $(patsubst common/target/%.h,$(DIST_DIR)/%.h,$(wildcard common/target/*.h))
NEARBY_HEADERS += $(patsubst common/target/$(ARCH_COMMON_NAME)/%.h,$(DIST_DIR)/%.h,$(wildcard common/target/$(ARCH_COMMON_NAME)/*.h))
NEARBY_HEADERS += $(patsubst client/target/%,$(DIST_DIR)/%,$(wildcard client/target/*.h))

NEARBY_DIST_HEADERS := $(NEARBY_HEADERS)

DIST_LIST += $(NEARBY_DIST_HEADERS)
DIST_LIST += $(DIST_NEARBY_LIB_NAME)

$(DIST_DIR)/nearby.h: $(FIRMWARE_VERSION_FILENAME)
$(DIST_DIR)/nearby.h: common/target/nearby.h
	@mkdir -p $(dir $@)
	$(call copy_dist_header)
	sed -i -e "s/\(define\s*NEARBY_BUILD_ID\).*/\1 \"$(FIRMWARE_VERSION)\"/" $@

define copy_dist_header
	@mkdir -p $(dir $@)
	@test "$$(head -n 2 $< | grep 'Copyright [0-9]\{4\} Google LLC.')" != "" || \
		(echo ; echo  "=====ERROR=====" ; echo "Missing copyright in $<" ; exit 1)
	unifdef -t -b -x2 $(UNIFDEF_ARG) -o - $<  | cat - >$@
	@# (mkartoz) Should be able to specify $@ as the output directly from
	@# unifdef but, if you do that, the file never shows up on kokoro. Piping to
	@# cat is a workaround.
endef

# Do not bypass the whole build process if the dist directory exists
.PHONY: dist
dist : all \
     $(DIST_LIST)

$(DIST_DIR)/$(NEARBY_LIB_NAME) : $(NAME)
	mkdir -p $(dir $@)
	cp -uv $< $@

$(DIST_DIR)/%.h : common/target/%.h
	$(call copy_dist_header)

$(DIST_DIR)/%.h : common/target/$(ARCH_COMMON_NAME)/%.h
	$(call copy_dist_header)

$(NAME): $(NEARBY_LIB_OBJS)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $(NEARBY_LIB_OBJS)

# If the hash has changed then we need to rebuild nearby.o and nearby_client.o
$(filter %/nearby_client.o, $(NEARBY_LIB_OBJS)): $(FIRMWARE_VERSION_FILENAME)
$(filter %/nearby.o, $(NEARBY_LIB_OBJS)): $(FIRMWARE_VERSION_FILENAME)

$(CLIENT_OBJS) : $(OUT_DIR)/%.o : %.c
	$(call compile_c, $(CFLAGS) -Icommon/target $(CLIENT_INCLUDES))

$(COMMON_OBJS) : $(OUT_DIR)/%.o: %.c
	$(call compile_c,$(CFLAGS))

ALL_OBJS += $(NEARBY_LIB_OBJS) $(CLIENT_OBJS)

DEPFILES = $(ALL_OBJS:.o=.d)

-include $(DEPFILES)

.PHONY: clean
clean:
	rm -f $(NAME)
	rm -f $(ALL_OBJS) $(DEPFILES)
	rm -f $(FIRMWARE_VERSION_FILENAME) $(PHONY_VERSION_FILENAME)
	rm -f $(DIST_LIST) $(TEST_BINARIES)

-include target/$(ARCH)/rules.mk
