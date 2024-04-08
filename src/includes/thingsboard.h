#ifndef _THINGSBOARD_H
#define _THINGSBOARD_H
    // Defines the Thingsboard APIs
    typedef enum DC_API {
        USE_MQTT,
        USE_HTTP
    } DC_API;

    // Different return codes for the Thingsboard API
    typedef enum thingsboard_code {
        THINGSBOARD_SUCCESS       = 0,
        THINGSBOARD_UNAUTHORIZED  = 1,
        THINGSBOARD_BAD_REQUEST   = 2,
        THINGSBOARD_UNKNOWN_ERROR = 3,
    } thingsboard_code;

    // The Thingsboard context
    typedef struct thingsboard_ctx thingsboard_ctx;

    /*
    * Initializes the Thingsboard context
    *
    * @param API - The API to use
    * @return On success: thingsboard_ctx* - The Thingsboard context, On failure: NULL
    */
    thingsboard_ctx* thingsboard_init(DC_API API);

    /*
    * Connects to the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param host - The Thingsboard server host
    * @param port - The Thingsboard server port
    * @param token - The Thingsboard device token
    * @return thingsboard_code - The return code
    */
    thingsboard_code thingsboard_connect(thingsboard_ctx* ctx, char* host, int port, char* token);

    /*
    * Disconnects from the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @return thingsboard_code - The return code
    */
    thingsboard_code thingsboard_disconnect(thingsboard_ctx* ctx);

    /*
    * Cleans up the Thingsboard context
    *
    * @param ctx - The Thingsboard context
    * @return thingsboard_code - The return code
    */
    void thingsboard_cleanup(thingsboard_ctx* ctx);

    /*
    * Sends a telemetry message to the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param telemetry_data - The telemetry data
    * @param topic - The telemetry topic
    * @return thingsboard_code - The return code
    * @note If the topic is NULL, the default topic will be used
    * @note The default topic is: "v1/devices/me/telemetry"
    * @note The telemetry data should be in JSON format
    * @note Example: "{\"temperature\":50}"
    */
    thingsboard_code thingsboard_telemetry_send(thingsboard_ctx* ctx, char* telemetry_data, char* topic);

    /*
    * Sends an attributes request to the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param request_id - The request ID
    * @param attribute_data - The attribute data
    * @param on_response - The callback function to call when the response is received
    * @return thingsboard_code - The return code
    * @note The attribute data should be in JSON format
    * @note Example "{\"sharedKeys\":\"yourAttribute,otherAttribute\"}"
    */
    thingsboard_code thingsboard_attributes_request(thingsboard_ctx* ctx, int request_id, char* attribute_data, void (*on_response)(thingsboard_ctx* ctx, char* json));
    
    /*
    * Publishes attributes to the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param attribute_data - The attribute data
    * @return thingsboard_code - The return code
    * @note The attribute data should be in JSON format
    * @note Example "{\"temperature\":50}"
    */
    thingsboard_code thingsboard_attributes_publish(thingsboard_ctx* ctx, char* attribute_data);
    
    /*
    * Subscribes to the Thingsboard attribute updates
    *
    * @param ctx - The Thingsboard context
    * @param timeout - The timeout in milliseconds
    * @param on_update - The callback function to call when the attributes are updated
    * @return thingsboard_code - The return code
    */
    thingsboard_code thingsboard_attributes_subscribe(thingsboard_ctx* ctx, int timeout, void (*on_update)(thingsboard_ctx* ctx, char* json));
    
    /*
    * Unsubscribes from the Thingsboard attribute updates
    *
    * @param ctx - The Thingsboard context
    * @return thingsboard_code - The return code
    * @note This function should be called when the subscription is no longer needed
    * @note This function should be called before the client is disconnected
    * @note The function will wait for the timeout (only on HTTP API) to receive the last message before unsubscribing
    */
    thingsboard_code thingsboard_attributes_unsubscribe(thingsboard_ctx* ctx);

    /*
    * Subscribes to the Thingsboard RPC updates
    *
    * @param ctx - The Thingsboard context
    * @param timeout - The timeout in milliseconds
    * @param rpc_on_subscribe - The callback function to call when the RPC is subscribed
    * @return thingsboard_code - The return code
    */
    thingsboard_code thingsboard_rpc_subscribe(thingsboard_ctx* ctx, int timeout, void (*rpc_on_subscribe)(thingsboard_ctx* ctx, char* json, int req_id));
    
    /*
    * Unsubscribes from the Thingsboard RPC updates
    *
    * @param ctx - The Thingsboard context
    * @return thingsboard_code - The return code
    * @note This function should be called when the subscription is no longer needed
    * @note This function should be called before the client is disconnected
    * @note The function will wait for the timeout (only on HTTP API) to receive the last message before unsubscribing
    */
    thingsboard_code thingsboard_rpc_unsubscribe(thingsboard_ctx* ctx);

    /*
    * Sends an RPC reply to the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param request_id - The request ID
    * @param response - The response data
    * @return thingsboard_code - The return code
    * @note The response data should be in JSON format
    * @note Example "{\"response\":\"OK\"}"
    * @note This function should be called when the RPC request is received
    */
    thingsboard_code thingsboard_rpc_reply(thingsboard_ctx* ctx, int request_id, char* response);
    
    /*
    * Sends an RPC request to the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param request_id - The request ID
    * @param method - The method name
    * @param params - The parameters
    * @param rpc_on_response - The callback function to call when the response is received
    * @return thingsboard_code - The return code
    * @note The parameters should be in the following format
    * @note Example "param:value"
    */
    thingsboard_code thingsboard_rpc_send(thingsboard_ctx* ctx, int request_id, char* method, char* params, void (*rpc_on_response)(thingsboard_ctx* ctx, char* json));
    
    /*
    * Provisions a device in the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param provisionDeviceKey - The provision device key
    * @param provisionDeviceSecret - The provision device secret
    * @return thingsboard_code - The return code
    */
    thingsboard_code thingsboard_provision_device(thingsboard_ctx* ctx, char* provisionDeviceKey, char* provisionDeviceSecret);
    
    /*
    * Claims a device in the Thingsboard server
    *
    * @param ctx - The Thingsboard context
    * @param secret - The secret
    * @param duration - The duration in seconds
    * @return thingsboard_code - The return code
    * @note Secret and duration are optional parameters
    * @note If the duration is -1, the device will be claimed indefinitely
    */
    thingsboard_code thingsboard_device_claim(thingsboard_ctx* ctx, char* secret, int duration);

    /*
    * Loops forever to keep the connection alive
    *
    * @param ctx - The Thingsboard context
    * @return thingsboard_code - The return code
    * @note This function should be called in a loop to keep the connection alive
    */
    thingsboard_code thingsboard_loop_forever(thingsboard_ctx* ctx);
#endif