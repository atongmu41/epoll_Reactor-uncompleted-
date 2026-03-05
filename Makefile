## 简单 Makefile：编译 epoll_server / epoll_client（项目要求 C++11）

CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -pedantic

BUILD_DIR := build

SERVER_BIN := epoll_server
CLIENT_BIN := epoll_client

SERVER_SRC := tcpepoll.cpp InetAddress.cpp Socket.cpp Epoll.cpp Channel.cpp
CLIENT_SRC := client.cpp InetAddress.cpp

SERVER_OBJ := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SERVER_SRC))
CLIENT_OBJ := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CLIENT_SRC))

DEPS := $(SERVER_OBJ:.o=.d) $(CLIENT_OBJ:.o=.d)

.PHONY: all server client clean

all: $(SERVER_BIN) $(CLIENT_BIN)

server: $(SERVER_BIN)

client: $(CLIENT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SERVER_BIN): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(SERVER_BIN) $(CLIENT_BIN)
