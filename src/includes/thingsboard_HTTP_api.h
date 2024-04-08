#include <curl/curl.h>
#include <thingsboard.h>

#ifndef _THINGSBOARD_HTTP_API_H
#define _THINGSBOARD_HTTP_API_H
    int thingsboard_telemetry_send_HTTP(CURL* ctx, char* telemetry_data, char* endpoint, char* host, int port, char* token);

    char* thingsboard_attributes_request_HTTP(CURL* ctx, int request_id, char* attribute_data, char* host, int port, char* token);
    void* thingsboard_attributes_subscribe_HTTP(void* args);
    void thingsboard_attributes_unsubscribe_HTTP(thingsboard_ctx* ctx);

    void* thingsboard_rpc_subscribe_HTTP(void* args);
    void thingsboard_rpc_unsubscribe_HTTP(thingsboard_ctx* ctx);
    int thingsboard_rpc_reply_HTTP(CURL* ctx, int request_id, char* response, char* host, int port, char* token);
    char* thingsboard_rpc_send_HTTP(CURL* ctx, int request_id, char* method, char* params, char* host, int port, char* token);

    int thingsboard_device_claim_HTTP(CURL* ctx, char* secret, int duration, char* host, int port, char* token);

    int thingsboard_provision_device_HTTP(CURL* ctx, char* provisionDeviceKey, char* provisionDeviceSecret, char* host, int port, char* token);
#endif