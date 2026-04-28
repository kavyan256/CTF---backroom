#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define MAX_PLAYERS_CONST 6
#define MAX_HOSTNAME_LEN 64
#define NETWORK_TIMEOUT_SEC 5

/* Error codes */
#define NET_ERR_TIMEOUT -1
#define NET_ERR_CLOSED -2
#define NET_ERR_INVALID -3
#define NET_ERR_DISCONNECT -4

typedef struct {
    int player_id;
    char ip[64];
    int port;
    char name[32];
} PlayerInfo;

typedef struct {
    int assigned_id;
    PlayerInfo players[MAX_PLAYERS_CONST];
} JoinResponse;

typedef struct {
    int listen_port;
    char name[32];
} JoinRequest;

typedef struct {
    int player_id;
} PeerHello;

typedef struct {
    int player_id;
    float x, y;
    float angle;
} PlayerUpdate;

enum {
    LOBBY_MSG_READY_TOGGLE = 1,
    LOBBY_MSG_STATE = 2,
    LOBBY_MSG_START = 3,
    LOBBY_MSG_CHARACTER_SELECT = 4
};

typedef struct {
    int msg_type;
    int player_id;
    int is_ready;
    int character_index;
    int connected[MAX_PLAYERS_CONST];
    int ready[MAX_PLAYERS_CONST];
    int selected_character[MAX_PLAYERS_CONST];
} LobbyPacket;

#ifdef __cplusplus
}
#endif

#endif