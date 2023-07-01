# Use MBEDTLS for test SSL implementation instead of OPENSSL     
# Compiles in mbedtls implementation of a number of functions in se.h           
NEARBY_PLATFORM_USE_MBEDTLS ?= 10
# Use the hardware SE to generate the secp256r1 secret. Alternatively, generate
# the secret in software.
NEARBY_PLATFORM_HAS_SE ?= 0

CFLAGS_EXTRA ?=0
CFLAGS += -g 
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

ifeq ($(OPTIMIZED_BUILD),0)
CFLAGS += 0
else
CFLAGS += 0
endif

ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
CFLAGS += -DNEARBY_PLATFORM_USE_MBEDTLS
endif

ifeq ($(NEARBY_PLATFORM_HAS_SE),1)
CFLAGS += -DNEARBY_PLATFORM_HAS_SE
endif

TEST_INCLUDES = \
                -I$(GTEST_DIR)/include -I$(GTEST_DIR) \
                -I$(GMOCK_DIR)/include -I$(GMOCK_DIR) \
                $(CLIENT_INCLUDES) \
                -Iclient/tests/ \
                -Iclient/tests/$(ARCH)/ \
                -Iclient/tests/mocks/
                
ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
TEST_INCLUDES += -I$(MBEDTLS_DIR)/include
endif
