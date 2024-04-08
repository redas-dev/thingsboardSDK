#define _DEFAULT_SOURCE
#include "thingsboard_types.h"
#include "thingsboard_MQTT_api.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <cjson/cJSON.h>


void on_MQTT_message(struct mosquitto* mqtt, void* obj, const struct mosquitto_message* msg)
{
    thingsboard_ctx* ctx = (thingsboard_ctx*)obj;

    if (strcmp("v1/devices/me/attributes", msg->topic) == 0)
    {
        if (ctx->on_update)
            ctx->on_update(ctx, msg->payload);
    }
    else if (strstr(msg->topic, "v1/devices/me/attributes/response/") != NULL)
    {
        if (ctx->on_response)
            ctx->on_response(ctx, msg->payload);
    }
    else if (strstr(msg->topic, "v1/devices/me/rpc/request/") != NULL)
    {

        char* req_id_str = strrchr(msg->topic, '/');
        int req_id = atoi(req_id_str + 1);

        if (ctx->rpc_on_subscribe)
            ctx->rpc_on_subscribe(ctx, msg->payload, req_id);
    }
    else if (strstr(msg->topic, "v1/devices/me/rpc/response/") != NULL)
    {
        if (ctx->rpc_on_response)
            ctx->rpc_on_response(ctx, msg->payload);
    }
}

int thingsboard_attributes_request_MQTT(struct mosquitto* ctx, int request_id, char* attribute_data)
{
    if (ctx == NULL) return 2;

    int res = mosquitto_subscribe(ctx, NULL, "v1/devices/me/attributes/response/+", 0);
    if (res != MOSQ_ERR_SUCCESS){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Subscribing to attributes response failed: %s", mosquitto_strerror(res));
        #endif
        return 3;
    }
    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] Subscribed to attributes response");
    #endif

    mosquitto_message_callback_set(ctx, on_MQTT_message);

    char* base_topic = "v1/devices/me/attributes/request/";
    size_t size = strlen(base_topic) + 8;
    char* topic = (char*)malloc(size);
    snprintf(topic, size, "%s%d", base_topic, request_id);

    res = mosquitto_publish(ctx, NULL, topic, strlen(attribute_data), attribute_data, 0, false);
    if (res != MOSQ_ERR_SUCCESS){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Publishing attributes request failed: %s", mosquitto_strerror(res));
        #endif
        free(topic);
        return 3; 
    }
    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] Attributes request sent");
    #endif

    sleep(1);

    mosquitto_unsubscribe(ctx, NULL, "v1/devices/me/attributes/response/+");

    free(topic);

    return 0;
}

// v1/devices/me/telemetry
int thingsboard_telemetry_send_MQTT(struct mosquitto* ctx, char* telemetry_data, char* topic)
{
    if (ctx == NULL) return 2;

    int res = mosquitto_publish(ctx, NULL, topic, strlen(telemetry_data), telemetry_data, 0, false);

    if (res != MOSQ_ERR_SUCCESS){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Publishing telemetry failed: %s", mosquitto_strerror(res));
        #endif
        return 3;
    }
    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] Telemetry sent");
    #endif

    return 0;
}

int thingsboard_MQTT_subscribe(struct mosquitto* ctx, char* topic, void* cb)
{
    if (ctx == NULL) return 2;

    mosquitto_message_callback_set(ctx, cb);
    int res = mosquitto_subscribe(ctx, NULL, topic, 0);

    if (res != MOSQ_ERR_SUCCESS){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Subscribing to %s failed: %s", topic, mosquitto_strerror(res));
        #endif
        return 3;
    }
    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] Subscribed to: %s", topic);
    #endif

    return 0;
}

void thingsboard_attributes_unsubscribe_MQTT(thingsboard_ctx* ctx)
{
    ctx->attributes_subscribed = false;
    ctx->attributes_sub_cleaned = true;
    mosquitto_unsubscribe(ctx->mqtt, NULL, "v1/devices/me/attributes");
}

int thingsboard_attributes_subscribe_MQTT(thingsboard_ctx* ctx)
{
    ctx->attributes_subscribed = true;
    ctx->attributes_sub_cleaned = false;
    return thingsboard_MQTT_subscribe(ctx->mqtt, "v1/devices/me/attributes", on_MQTT_message);
}

void thingsboard_rpc_unsubscribe_MQTT(thingsboard_ctx* ctx)
{
    ctx->rpc_subscribed = false;
    ctx->rpc_sub_cleaned = true;
    mosquitto_unsubscribe(ctx->mqtt, NULL, "v1/devices/me/rpc/request/+");
}

int thingsboard_rpc_subscribe_MQTT(thingsboard_ctx* ctx)
{
    ctx->rpc_subscribed = true;
    ctx->rpc_sub_cleaned = false;
    return thingsboard_MQTT_subscribe(ctx->mqtt, "v1/devices/me/rpc/request/+", on_MQTT_message);
}

int thingsboard_rpc_reply_MQTT(struct mosquitto* ctx, int request_id, char* response)
{
    if (ctx == NULL) return 2;

    char* base_topic = "v1/devices/me/rpc/response/";
    size_t size = strlen(base_topic) + 8;
    char* topic = (char*)malloc(size);
    snprintf(topic, size, "%s%d", base_topic, request_id);

    int res = mosquitto_publish(ctx, NULL, topic, strlen(response), response, 0, false);
    free(topic);

    if (res != MOSQ_ERR_SUCCESS){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Publishing RPC response failed: %s", mosquitto_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] RPC response sent");
    #endif

    return 0;
}

int thingsboard_rpc_send_MQTT(struct mosquitto* ctx, int request_id, char* method, char* params)
{
    if (ctx == NULL) return 2;

    char* base_topic = "v1/devices/me/rpc/request/";
    size_t size = strlen(base_topic) + 8;
    char* topic = (char*)malloc(size);
    snprintf(topic, size, "%s%d", base_topic, request_id);

    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "method", method);
    cJSON_AddStringToObject(json, "params", params);

    char* rpc = cJSON_Print(json);
    cJSON_Delete(json);

    char* base_resp_topic = "v1/devices/me/rpc/response/";
    size_t resp_size = strlen(base_resp_topic) + 8;
    char* resp_topic = (char*)malloc(resp_size);
    snprintf(resp_topic, resp_size, "%s%d", base_resp_topic, request_id);

    mosquitto_message_callback_set(ctx, on_MQTT_message);
    mosquitto_subscribe(ctx, NULL, resp_topic, 0);

    int res = mosquitto_publish(ctx, NULL, topic, strlen(rpc), rpc, 0, false);
    free(topic);
    free(rpc);

    if (res != MOSQ_ERR_SUCCESS){
        free(resp_topic);
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Publishing RPC request failed: %s", mosquitto_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] RPC request sent");
    #endif

    mosquitto_unsubscribe(ctx, NULL, resp_topic);
    free(resp_topic);

    return 0;
}

int thingsboard_provision_device_MQTT(struct mosquitto* ctx, char* provisionDeviceKey, char* provisionDeviceSecret, char* token)
{
    if (ctx == NULL) return 2;

    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "provisionDeviceKey", provisionDeviceKey);
    cJSON_AddStringToObject(json, "provisionDeviceSecret", provisionDeviceSecret);
    cJSON_AddStringToObject(json, "token", token);
    cJSON_AddStringToObject(json, "credentialsType", "ACCESS_TOKEN");

    char* rpc = cJSON_Print(json);
    cJSON_Delete(json);

    int res = mosquitto_publish(ctx, NULL, "/provision", strlen(rpc), rpc, 0, false);
    free(rpc);

    if (res != MOSQ_ERR_SUCCESS){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Provisioning device failed: %s", mosquitto_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] Device provisioned");
    #endif

    return 0;
}

int thingsboard_device_claim_MQTT(struct mosquitto* ctx, char* secret, int duration)
{
    if (ctx == NULL) return 2;

    cJSON* json = cJSON_CreateObject();
    if (secret) cJSON_AddStringToObject(json, "secretKey", secret);
    if (duration >= 0) cJSON_AddNumberToObject(json, "durationMs", duration);

    char* rpc = cJSON_Print(json);
    cJSON_Delete(json);

    int res = mosquitto_publish(ctx, NULL, "v1/devices/me/claim", strlen(rpc), rpc, 0, false);
    free(rpc);

    if (res != MOSQ_ERR_SUCCESS){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard MQTT] Device claiming failed: %s", mosquitto_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard MQTT] Device claimed");
    #endif

    return 0;
}