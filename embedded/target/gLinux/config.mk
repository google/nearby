CFLAGS_EXTRA ?=
CFLAGS += -g \
          -MD \
          -Werror \
          -Wno-deprecated-declarations \
          -DARCH_GLINUX \
          -fsanitize=address \
          -fno-omit-frame-pointer \
          -DARCH_GLINUX \
          -DNEARBY_ALL_MODULE_DEBUG \
          -DNEARBY_UNIT_TEST_ENABLED \
          $(CFLAGS_EXTRA)

NEARBY_TRACE_LEVEL = VERBOSE

ifeq ($(OPTIMIZED_BUILD),1)
CFLAGS += -O2
else
CFLAGS += -O0
endif

TEST_INCLUDES = \
                -I$(GTEST_DIR)/include -I$(GTEST_DIR) \
                -I$(GMOCK_DIR)/include -I$(GMOCK_DIR) \
                $(CLIENT_INCLUDES) \
                -Iclient/tests/ \
                -Iclient/tests/$(ARCH)/ \
                -Iclient/tests/mocks/