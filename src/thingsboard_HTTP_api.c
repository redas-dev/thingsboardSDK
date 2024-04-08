#define _DEFAULT_SOURCE
#include "thingsboard_HTTP_api.h"
#include "thingsboard_types.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <pthread.h>

struct response {
  char *response;
  size_t size;
};

// http://$THINGSBOARD_HOST_NAME/api/v1/$ACCESS_TOKEN/telemetry
int thingsboard_telemetry_send_HTTP(CURL* ctx, char* telemetry_data, char* endpoint, char* host, int port, char* token)
{
    if (ctx == NULL) return 2;

    CURL* http = curl_easy_duphandle(ctx);

    size_t size = strlen(host) + strlen(token) + strlen(endpoint) + 25;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/%s/%s", host, port, token, endpoint);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(http, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(http, CURLOPT_URL, url);
    curl_easy_setopt(http, CURLOPT_POSTFIELDS, telemetry_data);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif

    int res = curl_easy_perform(http);
    curl_slist_free_all(headers);
    curl_easy_cleanup(http);
    free(url);

    if (res != CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] Telemetry send failed: %s", curl_easy_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] Telemetry sent");
    #endif

    return 0;
}

static int on_response(void* data, size_t size, size_t nmemb, void* clientp)
{
    size_t realsize = size * nmemb;
    struct response* mem = (struct response*)clientp;
    
    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(!ptr)
        return 0;
    
    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;
    
    return realsize;
}

char* thingsboard_attributes_request_HTTP(CURL* ctx, int request_id, char* attribute_data, char* host, int port, char* token)
{
    if (ctx == NULL) return NULL;

    CURL* http = curl_easy_duphandle(ctx);

    cJSON* object = cJSON_Parse(attribute_data);
    if (object == NULL || cJSON_IsInvalid(object)){
        cJSON_Delete(object);
        return NULL;
    }

    size_t size = strlen(host) + strlen(token) + 38;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/%s/attributes", host, port, token);

    CURLU* curlu = curl_url();
    curl_url_set(curlu, CURLUPART_URL, url, 0);

    struct response chunk = {0};

    struct curl_slist *headers = NULL;

    curl_easy_setopt(http, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(http, CURLOPT_WRITEFUNCTION, on_response);
    curl_easy_setopt(http, CURLOPT_WRITEDATA, (void*)&chunk);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif

    cJSON* clientKeys = cJSON_GetObjectItem(object, "clientKeys");
    cJSON* sharedKeys = cJSON_GetObjectItem(object, "sharedKeys");

    if (clientKeys != NULL){
        size_t clientKeysSize = strlen(clientKeys->valuestring) + 12;
        char clientKeysQ[clientKeysSize];
        
        snprintf(clientKeysQ, clientKeysSize, "clientKeys=%s", clientKeys->valuestring);

        curl_url_set(curlu, CURLUPART_QUERY, clientKeysQ, CURLU_APPENDQUERY | CURLU_URLENCODE);
    }

    if (sharedKeys != NULL){
        size_t sharedKeysSize = strlen(sharedKeys->valuestring) + 12;
        char sharedKeysQ[sharedKeysSize];
        
        snprintf(sharedKeysQ, sharedKeysSize, "sharedKeys=%s", sharedKeys->valuestring);

        curl_url_set(curlu, CURLUPART_QUERY, sharedKeysQ, CURLU_APPENDQUERY | CURLU_URLENCODE);
    }

    curl_easy_setopt(http, CURLOPT_CURLU, curlu);

    int res = curl_easy_perform(http);

    free(url);
    cJSON_Delete(object);
    curl_slist_free_all(headers);
    curl_url_cleanup(curlu);
    curl_easy_cleanup(http);

    if (res!= CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] Attributes request failed: %s", curl_easy_strerror(res));
        #endif
        return NULL;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] Attributes request sent");
    #endif

    return chunk.response;
}

void thingsboard_attributes_unsubscribe_HTTP(thingsboard_ctx* ctx)
{
    if (ctx == NULL) return;

    ctx->attributes_subscribed = false;
}

void* thingsboard_attributes_subscribe_HTTP(void* args)
{
    struct args* arguments = (struct args*)args;

    thingsboard_ctx* ctx = arguments->ctx;
    void (*on_update)(thingsboard_ctx* ctx, char* json) = arguments->cb;
    char* host = arguments->host;
    int port = arguments->port;
    char* token = arguments->token;
    int timeout = arguments->timeout;

    if (ctx == NULL || ctx->http == NULL){ free(args); return NULL; }

    CURL* http = curl_easy_duphandle(ctx->http);

    size_t size = strlen(host) + strlen(token) + 45;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/%s/attributes/updates", host, port, token);

    CURLU* curlu = curl_url();

    curl_url_set(curlu, CURLUPART_URL, url, 0);

    struct response chunk = { 0 };

    curl_easy_setopt(http, CURLOPT_WRITEFUNCTION, on_response);
    curl_easy_setopt(http, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(http, CURLOPT_WRITEDATA, (void*)&chunk);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif
    size_t timeoutSize = strlen("timeout=") + 10;
    char timeoutQ[timeoutSize];
    snprintf(timeoutQ, timeoutSize, "timeout=%d", timeout);

    curl_url_set(curlu, CURLUPART_QUERY, timeoutQ, CURLU_APPENDQUERY | CURLU_URLENCODE);
    curl_easy_setopt(http, CURLOPT_CURLU, curlu);

    int res = 0;

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] Subscribing to attributes updates");
    #endif
    
    char* prev = NULL;

    while(ctx->attributes_subscribed){
        res = curl_easy_perform(http);
        if (res != CURLE_OK || res == CURLE_OPERATION_TIMEOUTED || chunk.response == NULL){
            break;
        }

        if (prev != NULL){
            if (strcmp(prev, chunk.response) == 0){
                goto loop_end;
            }
            free(prev);
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard HTTP] Attributes update received");
            #endif
        }

        prev = strdup(chunk.response);
        if (on_update)
            on_update(ctx, chunk.response);

        loop_end:
        free(chunk.response);
        chunk.response = NULL;
        chunk.size = 0;
        sleep(3);
    }

    free(url);
    free(prev);
    free(args);
    free(chunk.response);
    curl_url_cleanup(curlu);
    curl_easy_cleanup(http);
    ctx->attributes_sub_cleaned = true;

    if (res != CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] Attributes subscribe failed: %s", curl_easy_strerror(res));
        #endif
        return NULL;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] Attributes subscribe success");
    #endif

    return NULL;
}

void thingsboard_rpc_unsubscribe_HTTP(thingsboard_ctx* ctx)
{
    if (ctx == NULL) return;

    ctx->rpc_subscribed = false;
}

void* thingsboard_rpc_subscribe_HTTP(void* args)
{
    struct args* arguments = (struct args*)args;

    thingsboard_ctx* ctx = arguments->ctx;
    void (*on_rpc)(thingsboard_ctx* ctx, char* json, int req_id) = arguments->cb;
    char* host = arguments->host;
    int port = arguments->port;
    char* token = arguments->token;
    int timeout = arguments->timeout;

    if (ctx == NULL || ctx->http == NULL){ free(args); return NULL; }

    CURL* http = curl_easy_duphandle(ctx->http);

    size_t size = strlen(host) + strlen(token) + 38;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/%s/rpc", host, port, token);

    CURLU* curlu = curl_url();
    curl_url_set(curlu, CURLUPART_URL, url, 0);

    struct response chunk = { 0 };

    curl_easy_setopt(http, CURLOPT_WRITEFUNCTION, on_response);
    curl_easy_setopt(http, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(http, CURLOPT_WRITEDATA, (void*)&chunk);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif

    size_t timeoutSize = strlen("timeout=") + 10;
    char timeoutQ[timeoutSize];
    snprintf(timeoutQ, timeoutSize, "timeout=%d", timeout);

    curl_url_set(curlu, CURLUPART_QUERY, timeoutQ, CURLU_APPENDQUERY | CURLU_URLENCODE);

    curl_easy_setopt(http, CURLOPT_CURLU, curlu);

    int res = 0;

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] Subscribing to RPC updates");
    #endif

    while(ctx->rpc_subscribed){
        res = curl_easy_perform(http);
        if (res != CURLE_OK || res == CURLE_OPERATION_TIMEOUTED || chunk.response == NULL){ 
            break;
        }

        if (chunk.response){
            #ifdef LOGGING_ENABLED
                syslog(LOG_INFO, "[Thingsboard HTTP] RPC update received");
            #endif
        
            cJSON* json = cJSON_Parse(chunk.response);
            if (json == NULL || cJSON_IsInvalid(json)){
                goto end_free;
            }

            cJSON* id = cJSON_GetObjectItem(json, "id");
            if (id == NULL || cJSON_IsInvalid(id)){
                goto end_json;
            }

            if (on_rpc)
                on_rpc(ctx, chunk.response, id->valueint);

            end_json:
                cJSON_Delete(json);
            end_free:
                free(chunk.response);
                chunk.response = NULL;
                chunk.size = 0;
        }
        sleep(3);
    }

    free(url);
    free(args);
    free(chunk.response);
    curl_url_cleanup(curlu);
    curl_easy_cleanup(http);
    ctx->rpc_sub_cleaned = true;

    if (res != CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] RPC subscribe failed: %s", curl_easy_strerror(res));
        #endif
        return NULL;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] RPC subscribe success");
    #endif

    return NULL;
}

int thingsboard_rpc_reply_HTTP(CURL* ctx, int request_id, char* response, char* host, int port, char* token)
{
    if (ctx == NULL) return 2;

    CURL* http = curl_easy_duphandle(ctx);

    size_t size = strlen(host) + strlen(token) + 38;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/%s/rpc/%d", host, port, token, request_id);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(http, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(http, CURLOPT_URL, url);
    curl_easy_setopt(http, CURLOPT_POSTFIELDS, response);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif

    int res = curl_easy_perform(http);

    curl_slist_free_all(headers);
    free(url);
    curl_easy_cleanup(http);

    if (res != CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] RPC reply failed: %s", curl_easy_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] RPC reply success");
    #endif

    return 0;
}

char* thingsboard_rpc_send_HTTP(CURL* ctx, int request_id, char* method, char* params, char* host, int port, char* token)
{
    if (ctx == NULL) return NULL;

    CURL* http = curl_easy_duphandle(ctx);

    size_t size = strlen(host) + strlen(token) + 38;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/%s/rpc", host, port, token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "id", request_id);
    cJSON_AddStringToObject(json, "method", method);
    cJSON_AddStringToObject(json, "params", params);

    char* rpc = cJSON_Print(json);
    cJSON_Delete(json);

    struct response chunk = {0};

    curl_easy_setopt(http, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(http, CURLOPT_URL, url);
    curl_easy_setopt(http, CURLOPT_POSTFIELDS, rpc);
    curl_easy_setopt(http, CURLOPT_WRITEFUNCTION, on_response);
    curl_easy_setopt(http, CURLOPT_WRITEDATA, (void*)&chunk);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif

    int res = curl_easy_perform(http);

    free(rpc);
    curl_slist_free_all(headers);
    free(url);
    curl_easy_cleanup(http);

    if (res != CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] RPC send failed: %s", curl_easy_strerror(res));
        #endif
        return NULL;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] RPC send success");
    #endif

    return chunk.response;
}

int thingsboard_provision_device_HTTP(CURL* ctx, char* provisionDeviceKey, char* provisionDeviceSecret, char* host, int port, char* token)
{
    if (ctx == NULL) return 2;

    CURL* http = curl_easy_duphandle(ctx);

    size_t size = strlen(host) + strlen(token) + 38;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/provision", host, port);

    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "provisionDeviceKey", provisionDeviceKey);
    cJSON_AddStringToObject(json, "provisionDeviceSecret", provisionDeviceSecret);
    cJSON_AddStringToObject(json, "token", token);
    cJSON_AddStringToObject(json, "credentialsType", "ACCESS_TOKEN");

    char* provision = cJSON_Print(json);
    cJSON_Delete(json);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(http, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(http, CURLOPT_URL, url);
    curl_easy_setopt(http, CURLOPT_POSTFIELDS, provision);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif

    int res = curl_easy_perform(http);

    free(provision);
    curl_slist_free_all(headers);
    free(url);
    curl_easy_cleanup(http);

    if (res != CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] Provision device failed: %s", curl_easy_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] Provision device success");
    #endif

    return 0;
}

int thingsboard_device_claim_HTTP(CURL* ctx, char* secret, int duration, char* host, int port, char* token)
{
    if (ctx == NULL) return 2;

    CURL* http = curl_easy_duphandle(ctx);

    size_t size = strlen(host) + strlen(token) + 38;
    char* url = (char*)malloc(size);
    snprintf(url, size, "http://%s:%d/api/v1/%s/claim", host, port, token);

    cJSON* json = cJSON_CreateObject();
    if (secret) cJSON_AddStringToObject(json, "secretKey", secret);
    if (duration >=0) cJSON_AddNumberToObject(json, "durationMs", duration);

    char* claim = cJSON_Print(json);
    cJSON_Delete(json);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(http, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(http, CURLOPT_URL, url);
    curl_easy_setopt(http, CURLOPT_POSTFIELDS, claim);
    #ifdef LOGGING_ENABLED
        curl_easy_setopt(http, CURLOPT_VERBOSE, 1L);
    #endif
    
    int res = curl_easy_perform(http);

    free(claim);
    curl_slist_free_all(headers);
    free(url);
    curl_easy_cleanup(http);

    if (res != CURLE_OK){
        #ifdef LOGGING_ENABLED
            syslog(LOG_ERR, "[Thingsboard HTTP] Device claim failed: %s", curl_easy_strerror(res));
        #endif
        return 3;
    }

    #ifdef LOGGING_ENABLED
        syslog(LOG_INFO, "[Thingsboard HTTP] Device claim success");
    #endif

    return 0;
}
