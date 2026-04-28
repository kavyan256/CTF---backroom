CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -O2 -DLOG_LEVEL=LOG_LEVEL_INFO
CFLAGS += -Icommon -Iclient -Iclient/core -Iclient/networking -Iclient/simulation
CFLAGS += -Iclient/input -Iclient/render

LDFLAGS_CLIENT = -lGL -lGLU -lglut -lpthread -lm
LDFLAGS_SERVER = -lpthread

SERVER_SRCS = \
	server/server.c

CLIENT_SRCS = \
	client/client.c \
	client/core/game.c \
	client/networking/net.c \
	client/networking/recv_thread.c \
	client/networking/peer_manager.c \
	client/simulation/level.c \
	client/input/input.c \
	client/render/render.c \
	client/render/raycaster.c \
	client/render/texture.c \
	client/render/sprite.c

SERVER_BIN = build/server_app
CLIENT_BIN = build/client_app

all: $(SERVER_BIN) $(CLIENT_BIN)

build:
	mkdir -p build

$(SERVER_BIN): build $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o $(SERVER_BIN) $(LDFLAGS_SERVER)

$(CLIENT_BIN): build $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o $(CLIENT_BIN) $(LDFLAGS_CLIENT)

clean:
	rm -rf build

debug: CFLAGS += -g -DLOG_LEVEL=LOG_LEVEL_DEBUG
debug: clean all

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	@echo Usage: ./build/client_app SERVER_IP

run-localhost:
	./run_localhost_4players.sh

help:
	@echo "Available targets:"
	@echo "  make              - Build server and client"
	@echo "  make debug        - Build with debug symbols and verbose logging"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make run-server   - Run server"
	@echo "  make run-localhost- Run 4-player local test"
	@echo "  make help         - Show this help"

.PHONY: all clean debug run-server run-client run-localhost help
