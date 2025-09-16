
#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>



// // created for each client connecting to server
struct player {
    struct lws *wsi;
    void *move;                             //has to be callocd with LWS_PRE + 1 size
};

// created for each game session
struct per_game_session {

    struct player *pl1;
    struct player *pl2;
    int guesser;
    int pl1WinCount;
    int pl2WinCount;
    int rounds;
    int winner;
    int game_started;

    //round specific fields
    char pl1_move;
    char pl2_move;
    int current_guesser;
    int waiting_for_moves;
};

// also created for each game session to allow communinication to users
struct per_connection_data {
    struct per_game_session *game_session;
    int player_number;
    int is_host;
    int room_number;
};

// created for each vhost the protocol is used on (servicing multiple domains)
struct vhd_game_server {
    struct lws_context *context;
    struct lws_vhost *vhost;
    struct per_game_session *game_rooms[10000]; 
};

static void send_game_state_update(struct per_game_session *game) {
    if (!game || !game->pl1 || !game->pl2) return;
    
    unsigned char buf[LWS_PRE + 128];
    char message[64];
    
    // Send role information to both players
    snprintf(message, sizeof(message), "ROUND_START:GUESSER:%d", game->current_guesser);
    strcpy((char*)buf + LWS_PRE, message);
    
    // Send to player 1
    lws_write(game->pl1->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
    
    // Send to player 2  
    lws_write(game->pl2->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
    
    // Send score update
    snprintf(message, sizeof(message), "SCORE:%d:%d", game->pl1WinCount, game->pl2WinCount);
    strcpy((char*)buf + LWS_PRE, message);
    
    lws_write(game->pl1->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
    lws_write(game->pl2->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
}

static void send_round_result(struct per_game_session *game, const char* result) {
    if (!game || !game->pl1 || !game->pl2) return;
    
    unsigned char buf[LWS_PRE + 128];
    char message[64];
    
    snprintf(message, sizeof(message), "ROUND_RESULT:%s:SCORE:%d:%d", 
             result, game->pl1WinCount, game->pl2WinCount);
    strcpy((char*)buf + LWS_PRE, message);
    
    lws_write(game->pl1->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
    lws_write(game->pl2->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
}

static void send_game_over(struct per_game_session *game) {
    if (!game || !game->pl1 || !game->pl2) return;
    
    unsigned char buf[LWS_PRE + 128];
    char message[64];
    
    // Determine winner
    const char* winner_msg;
    if (game->pl1WinCount > game->pl2WinCount) {
        winner_msg = "GAME_OVER:P1_WINS";
    } else if (game->pl2WinCount > game->pl1WinCount) {
        winner_msg = "GAME_OVER:P2_WINS";
    } else {
        winner_msg = "GAME_OVER:TIE";
    }
    
    snprintf(message, sizeof(message), "%s:SCORE:%d:%d", 
             winner_msg, game->pl1WinCount, game->pl2WinCount);
    strcpy((char*)buf + LWS_PRE, message);
    
    lws_write(game->pl1->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
    lws_write(game->pl2->wsi, buf + LWS_PRE, strlen(message), LWS_WRITE_TEXT);
}

static void process_round(struct per_game_session *game, struct per_connection_data *pcd ,struct vhd_game_server *vhd) {
    char pl1_move = game->pl1_move;
    char pl2_move = game->pl2_move;
    int guesser = game->current_guesser;
    
    lwsl_user("Processing round: P1=%c, P2=%c, Guesser=P%d\n", 
              pl1_move, pl2_move, guesser);
    
    // Check if moves match
    if (pl1_move == pl2_move) {
        // Moves match - guesser gets point
        if (guesser == 1) {
            game->pl1WinCount++;
            send_round_result(game, "P1_GUESSER_WIN");
            lwsl_user("P1 (guesser) gets point! Score: P1=%d, P2=%d\n", 
                     game->pl1WinCount, game->pl2WinCount);
        } else {
            game->pl2WinCount++;
            send_round_result(game, "P2_GUESSER_WIN");
            lwsl_user("P2 (guesser) gets point! Score: P1=%d, P2=%d\n", 
                     game->pl1WinCount, game->pl2WinCount);
        }
        // Guesser stays the same for next round
    } else {
        // Moves don't match - switch guesser
        if (guesser == 1) {
            game->current_guesser = 2;
            send_round_result(game, "GUESSER_SWITCH_TO_P2");
            lwsl_user("Guesser switches to P2. Score: P1=%d, P2=%d\n", 
                     game->pl1WinCount, game->pl2WinCount);
        } else {
            game->current_guesser = 1;
            send_round_result(game, "GUESSER_SWITCH_TO_P1");
            lwsl_user("Guesser switches to P1. Score: P1=%d, P2=%d\n", 
                     game->pl1WinCount, game->pl2WinCount);
        }
    } 
    game->rounds++;
    
    if (game->rounds >= 5) {
        // Game is over, send final results
        send_game_over(game);
        
        // Mark game as finished
        game->game_started = 3; 
        
        // Clean up the room
        if (game->pl1 && game->pl1->wsi) {
            vhd->game_rooms[pcd->room_number] = NULL;
        }
        
        lwsl_user("Game over after 5 rounds! Final score: P1=%d, P2=%d\n", 
                 game->pl1WinCount, game->pl2WinCount);
    } else {
        // Continue game - send updated state for next round
        send_game_state_update(game);
    }
}

static int
callback_gotcha_server(struct lws *wsi, enum lws_callback_reasons reason,
            void *user, void *in, size_t len)
{
    struct per_connection_data *pcd = (struct per_connection_data *)user;
    struct vhd_game_server *vhd = 
            (struct vhd_game_server *)
            lws_protocol_vh_priv_get(lws_get_vhost(wsi),
                lws_get_protocol(wsi));
    int m;
    int isConnected = 0;

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                    lws_get_protocol(wsi),
                    sizeof(struct vhd_game_server));
            if (!vhd) return -1;

            vhd->context = lws_get_context(wsi);
            vhd->vhost = lws_get_vhost(wsi);
            break;
        
        case LWS_CALLBACK_ESTABLISHED:
            lwsl_warn("LWS_CALLBACK_ESTABLISHED - USER JOINING\n");
            
            
            break;
        
        case LWS_CALLBACK_CLOSED:
            //free memory and remove all connections
            if(!pcd->game_session) break;

            if(pcd->player_number==1){
                pcd->game_session->pl1 = NULL;

            }
            else if(pcd->player_number==2){
                pcd->game_session->pl2 = NULL;

            }

            if(!pcd->game_session->pl1 && !pcd->game_session->pl2){
                free(pcd->game_session);
            }

            pcd->game_session = NULL;
            pcd->player_number = -1;
            break;
        
        case LWS_CALLBACK_SERVER_WRITEABLE:
            lwsl_warn("WRITING TO CLIENTS\n");
            if(!pcd->game_session) break;

            unsigned char buf[LWS_PRE + 64];

            //writing to player 1
            if(pcd->player_number == 1){
                //pre pl2 joined message
                if(!pcd->game_session->game_started){
                    const char *msg = "waiting for player 2\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing game waiting messsage to pl1\n");
                        return -1;
                    }
                }
                else if(pcd->game_session->game_started == 1){
                    const char *msg = "Game Started";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing game waiting messsage to pl1\n");
                        return -1;
                    }
                }
                //other player abandoned mid game signal 2
                else if(pcd->game_session->game_started == 2){
                    const char *msg = "Player 2 abandoned game\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing player abandoned messsage to pl1\n");
                        return -1;
                    }
                }
            }
            else{
                if(pcd->game_session->game_started == 1){
                    const char *msg = "Game Started";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl2->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing game waiting messsage to pl2\n");
                        return -1;
                    }
                }
                else if(pcd->game_session->game_started == 2){
                    const char *msg = "Player 2 abandoned game\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing player abandoned messsage to pl1\n");
                        return -1;
                    }
                }
            }
            break;
        
        case LWS_CALLBACK_RECEIVE: 
            lwsl_warn("RECEIVING FROM CLIENT\n");
            
            if(len==0 || len>=256){
                lwsl_err("Invalid Message length: %d\n", (int)len);
                break;
            }

            char msg[256];
            memcpy(msg, in, len);
            msg[len] = '\0';

            lwsl_user("Player %d sent: %s\n", pcd->player_number, msg);

            //parsing message
            if(strcmp(msg,"HOST") == 0){
                lwsl_user("Player %d wants to host a game", pcd->player_number);
            

                int room_code;
                do {
                    room_code = rand() % 9000 + 1000;
                }while (vhd->game_rooms[room_code]!=NULL);

                //create new game session
                struct per_game_session *new_game = calloc(1, sizeof(struct per_game_session));
                if (!new_game){
                    lwsl_err("out of memory creating game\n");
                    break;
                }

                //create player 1 (host)
                new_game->pl1 = calloc(1, sizeof(struct player));
                if (!new_game->pl1) {
                    free(new_game);
                    lwsl_err("Out of memory creating player\n");
                    break;
                }

                new_game->pl1->wsi = wsi;
                new_game->game_started = 0;

                vhd->game_rooms[room_code] = new_game;

                pcd->game_session = new_game;
                pcd->player_number = 1;
                pcd->is_host = 1;
                pcd->room_number = room_code;

                //semd room code back to client
                unsigned char buf[LWS_PRE + 64];
                char response[32];
                snprintf(response, sizeof(response), "ROOM:%d", room_code);
                strcpy((char*)buf+LWS_PRE, response);

                m = lws_write(wsi,buf + LWS_PRE, strlen(response), LWS_WRITE_TEXT);
                if (m<strlen(response)){
                    lwsl_err("Error sending room code\n");
                }
                lwsl_user("Created room %d for player %d\n", room_code, pcd->player_number);
            }
            else if(strncmp(msg, "JOIN:", 5) == 0){
                int room_code = atoi(msg+5);
                lwsl_user("Player %d wants to join room %d\n", pcd->player_number, room_code);
                
                // Check if room exists and has space
                if (room_code < 1000 || room_code > 9999 || !vhd->game_rooms[room_code]) {
                    // Room doesn't exist
                    unsigned char buf[LWS_PRE + 64];
                    const char *response = "ERROR:Room not found";
                    strcpy((char*)buf + LWS_PRE, response);
                    lws_write(wsi, buf + LWS_PRE, strlen(response), LWS_WRITE_TEXT);
                    break;
                }

                struct per_game_session *game = vhd->game_rooms[room_code];
                if (game->pl2 != NULL) {
                    // Room is full
                    unsigned char buf[LWS_PRE + 64];
                    const char *response = "ERROR:Room full";
                    strcpy((char*)buf + LWS_PRE, response);
                    lws_write(wsi, buf + LWS_PRE, strlen(response), LWS_WRITE_TEXT);
                    break;
                }

                //add player 2 to game
                game->pl2 = calloc(1, sizeof(struct player));
                if(!game->pl2) {
                    lwsl_err("out of memory\n");
                    break;
                }

                game->pl2->wsi = wsi;
                game->game_started = 1;                     //game can now start
                game->current_guesser = 1;
                game->pl1_move = '\0';  
                game->pl2_move = '\0';  
                game->waiting_for_moves = 1;  
                
                //update connection data
                pcd->game_session = game;
                pcd->player_number = 2 ;
                pcd->is_host = 0;
                pcd->room_number = room_code;

                //notify both players
                send_game_state_update(game);
                
                lwsl_user("Player 2 joined room %d, game starting\n", room_code);
            }
            else if (strncmp(msg, "MOVE:", 5) == 0) {
                // Handle game moves like "MOVE:U", "MOVE:D", etc.
                char move = msg[5];
                lwsl_user("Player %d made move: %c\n", pcd->player_number, move);
                
                // Validate move
                if (move != 'U' && move != 'D' && move != 'L' && move != 'R') {
                    // Send error message
                    lwsl_err("INVALID MOVE by: %d\n",pcd->player_number);
                    break;
                }
                
                // Store the move
                if (pcd->player_number == 1) {
                    pcd->game_session->pl1_move = move;
                } else {
                    pcd->game_session->pl2_move = move;
                }
                
                // Check if both players have submitted moves
                if (pcd->game_session->pl1_move != '\0' && pcd->game_session->pl2_move != '\0') {
                    // Both moves received - process the round
                    process_round(pcd->game_session, pcd, vhd);
                    
                    // Reset moves for next round
                    pcd->game_session->pl1_move = '\0';
                    pcd->game_session->pl2_move = '\0';
                    
                    // Notify both players of results and request next moves
                    lws_callback_on_writable(pcd->game_session->pl1->wsi);
                    lws_callback_on_writable(pcd->game_session->pl2->wsi);
                } else {
                    // Still waiting for other player's move
                    unsigned char buf[LWS_PRE + 64];
                    const char *response = "MOVE_RECEIVED_WAITING";
                    strcpy((char*)buf + LWS_PRE, response);
                    lws_write(wsi, buf + LWS_PRE, strlen(response), LWS_WRITE_TEXT);
                }
            }
            else {
                lwsl_user("Unknown message from player %d: %s\n", pcd->player_number, msg);
            }   


            break;
        
        default:
            break; 
    }

    return 0;
}


#define LWS_PLUGIN_PROTOCOL_GOTCHA_SERVER \
    { \
        "gotcha-server",\
        callback_gotcha_server, \
        sizeof(struct per_connection_data), \
        1024, \
        0, NULL, 0 \
    }




/* BACKUP

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>



// // created for each client connecting to server
struct player {
    struct lws *wsi;
    void *move;                             //has to be callocd with LWS_PRE + 1 size
};

// created for each game session
struct per_game_session {

    struct player *pl1;
    struct player *pl2;
    int pl1WinCount;
    int pl2WinCount;
    int rounds;
    int winner;
    int game_started;
};

// also created for each game session to allow communinication to users
struct per_connection_data {
    struct per_game_session *game_session;
    int player_number;
    int is_host;
    int room_number;
};

// created for each vhost the protocol is used on (servicing multiple domains)
struct vhd_game_server {
    struct lws_context *context;
    struct lws_vhost *vhost;
    struct per_game_session *waiting_session;   //check if 1 player is already there
    struct per_game_session *game_rooms[10000]; 
};



static int
callback_gotcha_server(struct lws *wsi, enum lws_callback_reasons reason,
            void *user, void *in, size_t len)
{
    struct per_connection_data *pcd = (struct per_connection_data *)user;
    struct vhd_game_server *vhd = 
            (struct vhd_game_server *)
            lws_protocol_vh_priv_get(lws_get_vhost(wsi),
                lws_get_protocol(wsi));
    int m;
    int isConnected = 0;

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                    lws_get_protocol(wsi),
                    sizeof(struct vhd_game_server));
            if (!vhd) return -1;

            vhd->context = lws_get_context(wsi);
            vhd->vhost = lws_get_vhost(wsi);
            break;
        
        case LWS_CALLBACK_ESTABLISHED:
            //EDGE CASES: NOT ACCOUNTED FOR 3 PLAYERS LOL. HAVE TO CHANGE TO HOST PORT SETUP IF THAT IS POSSIBLE.


            lwsl_warn("LWS_CALLBACK_ESTABLISHED - USER JOINING\n");
            
            //check if a session is already established/waiting
            if(vhd->waiting_session == NULL){

                //creating new session 
                vhd->waiting_session = calloc(1,sizeof(struct per_game_session));

                //checking if memory was allocated 
                if(!vhd->waiting_session){
                    lwsl_err("Out of Memory \n");
                    return -1;
                }
                
                //creating player 1
                vhd->waiting_session->pl1 = calloc(1, sizeof(struct player));
                if(!vhd->waiting_session->pl1){
                    free(vhd->waiting_session);
                    vhd->waiting_session = NULL;
                    lwsl_err("Out of Memory \n");
                    return -1;
                }
                
                vhd->waiting_session->pl1->wsi = wsi;
                vhd->waiting_session->pl1WinCount = 0;
                vhd->waiting_session->pl2WinCount = 0;
                vhd->waiting_session->rounds = 1;
                vhd->waiting_session->game_started = 0;

                pcd->game_session = vhd->waiting_session;
                pcd->player_number = 1;

                lws_callback_on_writable(pcd->game_session->pl1->wsi);
                lwsl_user("Player 1 joined, waiting for player 2\n");
            }
            else {
                //game already active, adding player 2
                vhd->waiting_session->pl2 = calloc(1, sizeof(struct player));
                if(!vhd->waiting_session->pl2){
                    lwsl_err("Out of Memory \n");
                    return -1;
                }

                vhd->waiting_session->pl2->wsi = wsi;

                lwsl_user("Player 2 joined, game can start\n");
                lwsl_warn("GAME SESSION FULLY ESTABLISHED\n");

                pcd->game_session = vhd->waiting_session;
                pcd->player_number = 2;
                pcd->game_session->game_started = 1;
                vhd->waiting_session = NULL;


                //notify both players that game is starting
                lws_callback_on_writable(pcd->game_session->pl1->wsi);
                lws_callback_on_writable(pcd->game_session->pl2->wsi);

            }
            break;
        
        case LWS_CALLBACK_CLOSED:
            //free memory and remove all connections
            if(!pcd->game_session) break;

            if(pcd->player_number==1){
                pcd->game_session->pl1 = NULL;

            }
            else if(pcd->player_number==2){
                pcd->game_session->pl2 = NULL;

            }

            if(!pcd->game_session->pl1 && !pcd->game_session->pl2){
                free(pcd->game_session);
            }

            pcd->game_session = NULL;
            pcd->player_number = -1;
            break;
        
        case LWS_CALLBACK_SERVER_WRITEABLE:
            lwsl_warn("WRITING TO CLIENTS\n");
            if(!pcd->game_session) break;

            unsigned char buf[LWS_PRE + 64];

            //writing to player 1
            if(pcd->player_number == 1){
                //pre pl2 joined message
                if(!pcd->game_session->game_started){
                    const char *msg = "waiting for player 2\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing game waiting messsage to pl1\n");
                        return -1;
                    }
                }
                else if(pcd->game_session->game_started == 1){
                    const char *msg = "Game Started\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing game waiting messsage to pl1\n");
                        return -1;
                    }
                }
                //other player abandoned mid game signal 2
                else if(pcd->game_session->game_started == 2){
                    const char *msg = "Player 2 abandoned game\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing player abandoned messsage to pl1\n");
                        return -1;
                    }
                }
            }
            else{
                if(pcd->game_session->game_started == 1){
                    const char *msg = "Game Started\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl2->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing game waiting messsage to pl2\n");
                        return -1;
                    }
                }
                else if(pcd->game_session->game_started == 2){
                    const char *msg = "Player 2 abandoned game\n";
                    strcpy((char*)buf + LWS_PRE, msg);
                    m = lws_write(pcd->game_session->pl1->wsi, buf+LWS_PRE,
                        strlen(msg), LWS_WRITE_TEXT);
                    if(m<strlen(msg)){
                        lwsl_err("ERROR writing player abandoned messsage to pl1\n");
                        return -1;
                    }
                }
            }
            break;
        
        case LWS_CALLBACK_RECEIVE: 
            lwsl_warn("RECEIVING FROM CLIENT\n");
            break;
        
        default:
            break;   
    }

    return 0;
}

#define LWS_PLUGIN_PROTOCOL_GOTCHA_SERVER \
    { \
        "gotcha-server",\
        callback_gotcha_server, \
        sizeof(struct per_connection_data), \
        1024, \
        0, NULL, 0 \
    }

*/