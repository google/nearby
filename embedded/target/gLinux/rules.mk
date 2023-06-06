ifneq ($(EXCLUDE_COMMON_TEST_SRCS),1)
TEST_SRCS += $(wildcard $(COMMON_DIR)/tests/*.cc)
endif

TEST_SRCS += $(wildcard client/tests/*.cc)

TEST_SRCS := $(filter-out $(TEST_SRCS_EXCLUDE), $(TEST_SRCS))

TEST_OBJS += $(patsubst %.cc,$(OUT_DIR)/%.o,$(TEST_SRCS))
GLINUX_TARGET_SRCS := $(wildcard client/tests/glinux/*.cc)
MBEDTLS_TARGET_SRCS := $(wildcard common/source/mbedtls/*.c)
GTEST_SRCS = $(GTEST_DIR)/src/gtest-all.cc $(GMOCK_DIR)/src/gmock-all.cc
EMPTY_TARGET_SRCS := $(wildcard client/tests/empty_target/*.c)

GTEST_OBJS := $(patsubst %.cc,$(OUT_DIR)/%.o,$(GTEST_SRCS))

GLINUX_TARGET_OBJS := $(patsubst %.cc,$(OUT_DIR)/%.o,$(GLINUX_TARGET_SRCS))
MBEDTLS_TARGET_OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(MBEDTLS_TARGET_SRCS))
EMPTY_TARGET_OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(EMPTY_TARGET_SRCS))

ALL_TEST_OBJS += $(GLINUX_TARGET_OBJS)

ALL_TEST_OBJS += $(TEST_OBJS)
ALL_TEST_OBJS += $(GTEST_OBJS)
ALL_TEST_OBJS += $(EMPTY_TARGET_OBJS)
ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
ALL_TEST_OBJS += $(MBEDTLS_TARGET_OBJS)
endif

# The glinux target layer
$(GLINUX_TARGET_OBJS) : $(OUT_DIR)/%.o: %.cc
	$(call compile_c,$(TEST_INCLUDES) -I. -std=c++14 -c $(CFLAGS))
	
ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
# Mbed TLS interface files
$(MBEDTLS_TARGET_OBJS) : $(OUT_DIR)/%.o: %.c
	$(call compile_c,$(TEST_INCLUDES) -I. -std=c99 -c $(CFLAGS))
endif

$(EMPTY_TARGET_OBJS) : $(OUT_DIR)/%.o: %.c
	$(info "compiling empty target object $<")
	$(call compile_c,-I. $(EMPTY_TARGET_INC) -std=c99 -c $(CFLAGS))

$(TEST_OBJS) : $(FIRMWARE_VERSION_FILENAME)
$(TEST_OBJS) : $(OUT_DIR)/%.o: %.cc
	$(call compile_c,$(TEST_INCLUDES) -I. -std=c++14 $(CFLAGS))

$(GTEST_OBJS) : $(OUT_DIR)/%.o : %.cc
	$(call compile_c,$(TEST_INCLUDES) -std=c++14 $(CFLAGS))

TESTS_TO_RUN = $(patsubst %.cc,$(OUT_DIR)/%_run,$(TEST_SRCS))
.PHONY: $(TESTS_TO_RUN)

# Static pattern rule to execute the test binary.
$(TESTS_TO_RUN) : %_run : %
	mkdir -p $(OUT_DIR)/test_logs/$(notdir $<)_logs/
	./$< --gtest_output=xml:$(OUT_DIR)/test_logs/$(notdir $<)_logs/sponge_log.xml

TARGET_OBJS ?= $(GLINUX_TARGET_OBJS)
ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
TARGET_OBJS += $(MBEDTLS_TARGET_OBJS)
endif

TARGET_OS_SRCS := $(wildcard client/tests/gLinux/*.cc)
TARGET_MBEDTLS_SRCS += $(wildcard common/source/mbedtls/*.c)
TARGET_OS_OBJS ?= $(patsubst %.cc,$(OUT_DIR)/%.o,$(TARGET_OS_SRCS))
TARGET_MBEDTLS_OBJS ?= $(patsubst %.c,$(OUT_DIR)/%.o,$(TARGET_MBEDTLS_SRCS))
$(TARGET_OS_OBJS) : $(OUT_DIR)/%.o: %.cc
	$(call compile_c,$(TEST_INCLUDES) -I. -std=c++14 $(CFLAGS))
$(TARGET_MBED_OBJS) : $(OUT_DIR)/%.o: %.c
	$(call compile_c,$(TEST_INCLUDES) -I. -std=c++14 $(CFLAGS))

ALL_TEST_OBJS += $(TEST_OBJS)
ALL_TEST_OBJS += $(TARGET_OS_OBJS)
ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
ALL_TEST_OBJS += $(TARGET_MBEDTLS_OBJS)
endif

ALL_OBJS += $(ALL_TEST_OBJS)
# Must include the .d files for test sources here since gLinux.rules is included
# by makefile after after it includes $(DEPFILES).
-include $(ALL_TEST_OBJS:.o=.d)

EMPTY_TARGET_TEST = $(OUT_DIR)/client/tests/empty_target_test
$(EMPTY_TARGET_TEST) : TARGET_OBJS = $(EMPTY_TARGET_OBJS)
$(EMPTY_TARGET_TEST) : $(EMPTY_TARGET_OBJS)
TEST_BINARIES = $(patsubst %.cc,$(OUT_DIR)/%,$(TEST_SRCS))
ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
$(TEST_BINARIES) : $(NAME) $(GTEST_OBJS) $(GLINUX_TARGET_OBJS) $(MBEDTLS_TARGET_OBJS)
else
$(TEST_BINARIES) : $(NAME) $(GTEST_OBJS) $(GLINUX_TARGET_OBJS)
endif
$(TEST_BINARIES) : $(TARGET_OS_OBJS)

LIBS ?= -L $(OUT_DIR) -lnearby -lcrypto -lssl -lpthread -lstdc++

ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
MBEDTLS_LIBS = $(MBEDTLS_DIR)/library/libmbedtls.a $(MBEDTLS_DIR)/library/libmbedcrypto.a
$(MBEDTLS_LIBS) &:
	cd $(MBEDTLS_DIR) && $(MAKE)
$(TEST_BINARIES) : $(MBEDTLS_LIBS)
LIBS += -L $(MBEDTLS_DIR)/library -lmbedtls -lmbedcrypto
endif

$(TEST_BINARIES) : % : %.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $< \
		$(CFLAGS) \
		$(TEST_INCLUDES) \
		$(GTEST_OBJS) \
		$(TARGET_OBJS) \
		$(TARGET_OS_OBJS) \
		-L /usr/local/lib \
		-std=c++14 \
		$(LIBS)

run_tests: $(TESTS_TO_RUN)

tests: $(TEST_BINARIES)

run_tests : tests
.PHONY : tests run_tests
