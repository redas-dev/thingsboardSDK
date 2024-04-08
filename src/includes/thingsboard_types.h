#include <stdbool.h>

#ifndef _THINGSBOARD_TYPES_H_
#define _THINGSBOARD_TYPES_H_
    #define LOGGING_ENABLED

    typedef struct thingsboard_ctx {
        int API;
        void* mqtt;
        void* http;
        char* host;
        int port;
        char* token;
        bool attributes_subscribed;
        bool rpc_subscribed;
        bool attributes_sub_cleaned;
        bool rpc_sub_cleaned;
        void (*on_response)(struct thingsboard_ctx* ctx, char* json);
        void (*on_update)(struct thingsboard_ctx* ctx, char* json);
        void (*rpc_on_subscribe)(struct thingsboard_ctx* ctx, char* json, int req_id);
        void (*rpc_on_response)(struct thingsboard_ctx* ctx, char* json);
    } thingsboard_ctx;

    struct args {
        thingsboard_ctx* ctx;
        char* host;
        int port;
        char* token;
        int timeout; 
        void* cb;
    };
#endif