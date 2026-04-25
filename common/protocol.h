#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define MAX_PLAYERS_CONST 4
#define MAX_HOSTNAME_LEN 64
#define NETWORK_TIMEOUT_SEC 5

/* Error codes */
#define NET_ERR_TIMEOUT -1
#define NET_ERR_CLOSED -2
#define NET_ERR_INVALID -3
#define NET_ERR_DISCONNECT -4

typedef struct {
    int ball_id;
    int owner_id;
    float x, y;
    float vx, vy;
    float ax, ay;
} Ball;

typedef struct {
    int player_id;
    float mouse_x;
    float mouse_y;
} InputState;

typedef struct {
    int ball_id;
    float x, y;
    float vx, vy;
    float ax, ay;
} BallTransferPacket;

typedef struct {
    int player_id;
    char ip[64];
    int port;
} PlayerInfo;

typedef struct {
    int assigned_id;
    PlayerInfo players[4];
} JoinResponse;

typedef struct {
    int listen_port;
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
    LOBBY_MSG_START = 3
};

typedef struct {
    int msg_type;
    int player_id;
    int is_ready;
    int connected[MAX_PLAYERS_CONST];
    int ready[MAX_PLAYERS_CONST];
} LobbyPacket;

#ifdef __cplusplus
}
#endif

#endif