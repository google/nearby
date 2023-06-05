# Use MBEDTLS for test SSL i mplementation instead of OPENSSL     
# Compiles in mbedtls implementation of a number of functions in se.h           
NEARBY_PLATFORM_USE_MBEDTLS ?= 0

# Use the hardware SE to generate the secp256r1 secret. Alternatively, generate
# the secret in software.
NEARBY_PLATFORM_HAS_SE ?= 1

# Use Micro-ecc for test nearby_platform_GetSecp160r1PublicKey implementation 
# instead of OPENSSL. 
NEARBY_PLATFORM_USE_MICROECC ?= 1

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

ifeq ($(NEARBY_PLATFORM_USE_MBEDTLS),1)
CFLAGS += -DNEARBY_PLATFORM_USE_MBEDTLS
endif

ifeq ($(NEARBY_PLATFORM_USE_MICROECC),1)
CFLAGS += -DNEARBY_PLATFORM_USE_MICROECC
CFLAGS += -DuECC_ENABLE_VLI_API=1 
CFLAGS += -DuECC_VLI_NATIVE_LITTLE_ENDIAN=1
CFLAGS += -DuECC_SUPPORTS_secp160r1=1
endif

ifeq ($(NEARBY_PLATFORM_HAS_SE),1)
CFLAGS += -DNEARBY_PLATFORM_HAS_SE
endif

ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
CFLAGS += -DNEARBY_FP_ENABLE_BATTERY_NOTIFICATION=$(NEARBY_FP_ENABLE_BATTERY_NOTIFICATION)
endif

ifdef NEARBY_FP_ENABLE_ADDITIONAL_DATA
CFLAGS += -DNEARBY_FP_ENABLE_ADDITIONAL_DATA=$(NEARBY_FP_ENABLE_ADDITIONAL_DATA)
endif

ifdef NEARBY_FP_ENABLE_SPOT
CFLAGS += -DNEARBY_FP_ENABLE_SPOT=$(NEARBY_FP_ENABLE_SPOT)
endif

ifdef NEARBY_FP_MESSAGE_STREAM
CFLAGS += -DNEARBY_FP_MESSAGE_STREAM=$(NEARBY_FP_MESSAGE_STREAM)
endif

ifdef NEARBY_FP_RETROACTIVE_PAIRING
CFLAGS += -DNEARBY_FP_RETROACTIVE_PAIRING=$(NEARBY_FP_RETROACTIVE_PAIRING)
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

ifeq ($(NEARBY_PLATFORM_USE_MICROECC),1)
TEST_INCLUDES += -I$(MICROECC_DIR)
endif