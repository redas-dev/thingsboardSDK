#define _DEFAULT_SOURCE
#include <cjson/cJSON.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "thingsboard.h"
#include "thingsboard_types.h"
#include "thingsboard_MQTT_api.h"
#include "thingsboard_HTTP_api.h"

thingsboard_ctx* thingsboard_init(DC_API API)
{
    #ifdef LOGGING_ENABLED
        openlog("thingsboard", LOG_PID, LOG_USER);
    #endif

    thingsboard_ctx* ctx = (thingsboard_ctx*)malloc(sizeof(thingsboard_ctx));
    if (ctx == NULL) return NULL;

    ctx->http = NULL;
    ctx->mqtt = NULL;
    ctx->API = API;
    ctx->attributes_subscribed = false;
    ctx->rpc_subscribed = false;
    ctx->attributes_sub_cleaned = false;
    ctx->rpc_sub_cleaned = false;

    if (API == USE_MQTT){
        mosquitto_lib_init();

        ctx->mqtt = mosquitto_new(NULL, true, ctx);
    }
    else if (API == USE_HTTP) ctx->http = curl_easy_init();
    else return NULL;

    return ctx;
}

void thingsboard_cleanup(thingsboard_ctx* ctx)
{
    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard] Cleaning up");
        closelog();
    #endif
    if (ctx->API == USE_MQTT){
        // Mosquitto cleanup
        mosquitto_disconnect(ctx->mqtt);
        mosquitto_destroy(ctx->mqtt);
        mosquitto_lib_cleanup();
    }
    else if (ctx->API == USE_HTTP){
        // Curl cleanup
        curl_easy_cleanup(ctx->http);
    }
    free(ctx);
}

thingsboard_code thingsboard_connect(thingsboard_ctx* ctx, char* host, int port, char* token)
{
    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard] Connecting to %s:%d", host, port);
    #endif

    if (ctx == NULL){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard] Failed to connect: ctx is NULL");
        #endif
        return THINGSBOARD_UNKNOWN_ERROR;
    }

    if (ctx->API == USE_MQTT){
        #ifdef LOGGING_ENABLED
            syslog(LOG_INFO, "[Thingsboard] Using MQTT API");
        #endif
        int res = mosquitto_username_pw_set(ctx->mqtt, token, NULL);
        if (res != MOSQ_ERR_SUCCESS){
            #ifdef LOGGING_ENABLED
                syslog(LOG_ERR, "[Thingsboard] Failed to set username and password: %s", mosquitto_strerror(res));
            #endif
            return THINGSBOARD_UNKNOWN_ERROR;   
        }
        res = mosquitto_connect_async(ctx->mqtt, host, port, 60);
        if (res != MOSQ_ERR_SUCCESS){
            #ifdef LOGGING_ENABLED
                syslog(LOG_ERR, "[Thingsboard] Failed to connect: %s", mosquitto_strerror(res));
            #endif
            return THINGSBOARD_UNKNOWN_ERROR;
        }
        mosquitto_loop_start(ctx->mqtt);
        #ifdef LOGGING_ENABLED
            syslog(LOG_INFO, "[Thingsboard] MQTT connected to %s:%d", host, port);
        #endif
    }
    else if (ctx->API == USE_HTTP)
        #ifdef LOGGING_ENABLED
            syslog(LOG_INFO, "[Thingsboard] Using HTTP API");
        #endif

    ctx->host = host;
    ctx->token = token;
    ctx->port = port;

    return THINGSBOARD_SUCCESS;
}

thingsboard_code thingsboard_disconnect(thingsboard_ctx* ctx)
{
    if (ctx == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    ctx->attributes_subscribed = false;
    ctx->rpc_subscribed = false;

    if (ctx->API == USE_MQTT){
        mosquitto_loop_stop(ctx->mqtt, true);
        int res = mosquitto_disconnect(ctx->mqtt);
        if (res == MOSQ_ERR_INVAL) return THINGSBOARD_UNKNOWN_ERROR;
    } else if (ctx->API == USE_HTTP){
        curl_easy_reset(ctx->http);
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard] Disconnected");
    #endif

    return THINGSBOARD_SUCCESS;
}

thingsboard_code thingsboard_telemetry_send(thingsboard_ctx* ctx, char* telemetry_data, char* topic)
{
    if (ctx == NULL || telemetry_data == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    switch(ctx->API){
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Sending telemetry data via MQTT");
            #endif
            if (topic == NULL) topic = "v1/devices/me/telemetry";
            return thingsboard_telemetry_send_MQTT(ctx->mqtt, telemetry_data, topic);
        case USE_HTTP:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Sending telemetry data via HTTP");
            #endif
            return thingsboard_telemetry_send_HTTP(ctx->http, telemetry_data, "telemetry", ctx->host, ctx->port, ctx->token);
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }
}

thingsboard_code thingsboard_attributes_publish(thingsboard_ctx* ctx, char* attribute_data)
{
    if (ctx == NULL || attribute_data == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    switch(ctx->API)
    {
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Publishing attributes via MQTT");
            #endif
            return thingsboard_telemetry_send_MQTT(ctx->mqtt, attribute_data, "v1/devices/me/attributes");
        case USE_HTTP:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Publishing attributes via HTTP");
            #endif
            return thingsboard_telemetry_send_HTTP(ctx->http, attribute_data, "attributes", ctx->host, ctx->port, ctx->token);
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }
}

thingsboard_code thingsboard_attributes_request(thingsboard_ctx* ctx, int request_id, char* attribute_data, void (*on_response)(thingsboard_ctx* ctx, char* json))
{
    if (ctx == NULL || attribute_data == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    thingsboard_code res = THINGSBOARD_SUCCESS;
    ctx->on_response = on_response;

    switch(ctx->API)
    {
        case USE_MQTT:{
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Requesting attributes via MQTT");
            #endif
            res = thingsboard_attributes_request_MQTT(ctx->mqtt, request_id, attribute_data);
            break;
        }
        case USE_HTTP:{
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Requesting attributes via HTTP");
            #endif
            char* resp = thingsboard_attributes_request_HTTP(ctx->http, request_id, attribute_data, ctx->host, ctx->port, ctx->token);

            if (resp != NULL){
                if (ctx->on_response)
                {
                    char* tmp = strdup(resp);
                    ctx->on_response(ctx, tmp);
                    free(tmp);
                }
                res = THINGSBOARD_SUCCESS;
            } else res = THINGSBOARD_UNKNOWN_ERROR;

            free(resp);
            break;
        }
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }

    return res;
}

thingsboard_code thingsboard_attributes_unsubscribe(thingsboard_ctx* ctx)
{
    switch(ctx->API)
    {
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Unsubscribing from attributes via MQTT");
            #endif

            thingsboard_attributes_unsubscribe_MQTT(ctx);
            break;
        case USE_HTTP:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Unsubscribing from attributes via HTTP");
            #endif

            thingsboard_attributes_unsubscribe_HTTP(ctx);
            break;
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }

    while(!ctx->attributes_sub_cleaned){
        sleep(1);
    }

    return THINGSBOARD_SUCCESS;
}

thingsboard_code thingsboard_attributes_subscribe(thingsboard_ctx* ctx, int timeout, void (*on_update)(thingsboard_ctx* ctx, char* json))
{
    if (ctx == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    thingsboard_code res = THINGSBOARD_SUCCESS;
    ctx->on_update = on_update;
    ctx->attributes_subscribed = true;
    ctx->attributes_sub_cleaned = false;

    switch(ctx->API)
    {
        case USE_MQTT:{
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Subscribing to attributes via MQTT");
            #endif
            res = thingsboard_attributes_subscribe_MQTT(ctx);
            break;
        }
        case USE_HTTP:{
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Subscribing to attributes via HTTP");
            #endif
            pthread_t thread;

            struct args* args = (struct args*)malloc(sizeof(struct args));
            args->ctx = ctx;
            args->host = ctx->host;
            args->port = ctx->port;
            args->token = ctx->token;
            args->timeout = timeout;
            args->cb = on_update;

            pthread_create(&thread, NULL, thingsboard_attributes_subscribe_HTTP, args);
            pthread_detach(thread);

            break;
        }
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }

    return res;
}

thingsboard_code thingsboard_rpc_unsubscribe(thingsboard_ctx* ctx)
{
    if (ctx == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    switch(ctx->API)
    {
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Unsubscribing from RPC via MQTT");
            #endif
            thingsboard_rpc_unsubscribe_MQTT(ctx);
            break;
        case USE_HTTP:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Unsubscribing from RPC via HTTP");
            #endif
            thingsboard_rpc_unsubscribe_HTTP(ctx);
            break;
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }

    while(!ctx->rpc_sub_cleaned){
        sleep(1);
    }

    return THINGSBOARD_SUCCESS;
}

thingsboard_code thingsboard_rpc_subscribe(thingsboard_ctx* ctx, int timeout, void (*rpc_on_subscribe)(thingsboard_ctx* ctx, char* json, int req_id))
{
    if (ctx == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    thingsboard_code res = THINGSBOARD_SUCCESS;
    ctx->rpc_on_subscribe = rpc_on_subscribe;
    ctx->rpc_subscribed = true;
    ctx->rpc_sub_cleaned = false;

    switch(ctx->API)
    {
        case USE_MQTT:{
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Subscribing to RPC via MQTT");
            #endif
            res = thingsboard_rpc_subscribe_MQTT(ctx);
            break;
        }
        case USE_HTTP:{
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Subscribing to RPC via HTTP");
            #endif

            pthread_t thread;

            struct args* args = (struct args*)malloc(sizeof(struct args));
            args->ctx = ctx;
            args->host = ctx->host;
            args->port = ctx->port;
            args->token = ctx->token;
            args->timeout = timeout;
            args->cb = rpc_on_subscribe;

            pthread_create(&thread, NULL, thingsboard_rpc_subscribe_HTTP, args);
            pthread_detach(thread);
            
            break;
        }
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }

    return res;
}

thingsboard_code thingsboard_rpc_reply(thingsboard_ctx* ctx, int request_id, char* response)
{
    if (ctx == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    switch(ctx->API)
    {
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Replying to RPC via MQTT");
            #endif
            return thingsboard_rpc_reply_MQTT(ctx->mqtt, request_id, response);
        case USE_HTTP:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Replying to RPC via HTTP");
            #endif
            return thingsboard_rpc_reply_HTTP(ctx->http, request_id, response, ctx->host, ctx->port, ctx->token);
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }
}

thingsboard_code thingsboard_rpc_send(thingsboard_ctx* ctx, int request_id, char* method, char* params, void (*rpc_on_response)(thingsboard_ctx* ctx, char* json))
{
    if (ctx == NULL || method == NULL || params == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    ctx->rpc_on_response = rpc_on_response;

    switch(ctx->API)
    {
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Sending RPC via MQTT");
            #endif
            return thingsboard_rpc_send_MQTT(ctx->mqtt, request_id, method, params);
        case USE_HTTP:{
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Sending RPC via HTTP");
            #endif
            char* resp = thingsboard_rpc_send_HTTP(ctx->http, request_id, method, params, ctx->host, ctx->port, ctx->token);

            if (resp != NULL){
                if (ctx->rpc_on_response)
                {
                    char* tmp = strdup(resp);
                    ctx->rpc_on_response(ctx, tmp);
                    free(tmp);
                    free(resp);
                }
                return THINGSBOARD_SUCCESS;
            } 
            free(resp);

            return THINGSBOARD_UNKNOWN_ERROR;
        }
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }
}

thingsboard_code thingsboard_provision_device(thingsboard_ctx* ctx, char* provisionDeviceKey, char* provisionDeviceSecret)
{
    if (ctx == NULL || provisionDeviceKey == NULL || provisionDeviceSecret == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    switch(ctx->API)
    {
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Provisioning device via MQTT");
            #endif
            return thingsboard_provision_device_MQTT(ctx->mqtt, provisionDeviceKey, provisionDeviceSecret, ctx->token);
        case USE_HTTP:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Provisioning device via HTTP");
            #endif
            return thingsboard_provision_device_HTTP(ctx->http, provisionDeviceKey, provisionDeviceSecret, ctx->host, ctx->port, ctx->token);
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }
}

thingsboard_code thingsboard_device_claim(thingsboard_ctx* ctx, char* secret, int duration)
{
    if (ctx == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    switch(ctx->API)
    {
        case USE_MQTT:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Claiming device via MQTT");
            #endif
            return thingsboard_device_claim_MQTT(ctx->mqtt, secret, duration);
        case USE_HTTP:
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard] Claiming device via HTTP");
            #endif
            return thingsboard_device_claim_HTTP(ctx->http, secret, duration, ctx->host, ctx->port, ctx->token);
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }
}

thingsboard_code thingsboard_loop_forever(thingsboard_ctx* ctx)
{
    if (ctx == NULL) return THINGSBOARD_UNKNOWN_ERROR;

    switch (ctx->API)
    {
        case USE_MQTT:
            sleep(3);
            break;
        case USE_HTTP:
            while(1){
                if ((ctx->attributes_subscribed && !ctx->attributes_sub_cleaned) | (ctx->rpc_subscribed && !ctx->rpc_sub_cleaned))
                {
                    sleep(3);
                }
                else break;
            }
            break;
        default:
            return THINGSBOARD_UNKNOWN_ERROR;
    }

    return THINGSBOARD_SUCCESS;
}