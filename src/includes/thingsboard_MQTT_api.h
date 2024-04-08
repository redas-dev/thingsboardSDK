#include <mosquitto.h>

#ifndef _THINGSBOARD_MQTT_API_H_
#define _THINGSBOARD_MQTT_API_H_
    int thingsboard_telemetry_send_MQTT(struct mosquitto* ctx, char* telemetry_data, char* topic);

    int thingsboard_attributes_request_MQTT(struct mosquitto* ctx, int request_id, char* attribute_data);
    int thingsboard_attributes_subscribe_MQTT(thingsboard_ctx* ctx);
    void thingsboard_attributes_unsubscribe_MQTT(thingsboard_ctx* ctx);

    int thingsboard_rpc_subscribe_MQTT(thingsboard_ctx* ctx);
    void thingsboard_rpc_unsubscribe_MQTT(thingsboard_ctx* ctx);
    int thingsboard_rpc_reply_MQTT(struct mosquitto* ctx, int request_id, char* response);
    int thingsboard_rpc_send_MQTT(struct mosquitto* ctx, int request_id, char* method, char* params);

    int thingsboard_device_claim_MQTT(struct mosquitto* ctx, char* secret, int duration);

    int thingsboard_provision_device_MQTT(struct mosquitto* ctx, char* provisionDeviceKey, char* provisionDeviceSecret, char* token);
#endif