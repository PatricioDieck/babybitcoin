CXX ?= ccache clang++
CXXFLAGS ?= -std=c++20 -O0 -g \
  -Isrc -Ithird_party -I/opt/homebrew/opt/openssl@3/include
LDFLAGS ?= -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -pthread
MAKEFLAGS += -s --no-builtin-rules
.SUFFIXES:

TARGET := babycoin

SRCS := \
  src/utils/common.cpp \
  src/utils/crypto.cpp \
  src/utils/serialize.cpp \
  src/utils/storage.cpp \
  src/script_min.cpp \
  src/tx.cpp \
  src/block.cpp \
  src/chain.cpp \
  src/p2p_minimal.cpp \
  src/miner.cpp \
  src/wallet.cpp \
  src/cli.cpp \
  src/main.cpp

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/dep

OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(SRCS:%.cpp=$(DEP_DIR)/%.d)

.PHONY: all clean run install-deps

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# Compile .cpp -> build/obj/...o and write deps to build/dep/...d
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@) $(dir $(DEP_DIR)/$*.d)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(DEP_DIR)/$*.d -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

install-deps:
	brew install openssl ccache