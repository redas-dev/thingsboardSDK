#include <thingsboard.h>
#include <stdio.h>
#include <signal.h>

void on_response(thingsboard_ctx* ctx, char* resp)
{
    printf("Attribute request response:  %s\n", resp);
}

void on_update(thingsboard_ctx* ctx, char* resp)
{
    printf("Attribute subscribe response:  %s\n", resp);
}

void on_rpc_update(thingsboard_ctx* ctx, char* resp, int req_id)
{
    printf("RPC subscribe response:  %s\n", resp);

    thingsboard_rpc_reply(ctx, req_id, "{\"response\":\"OK\"}");
}

void on_rpc_response(thingsboard_ctx* ctx, char* resp)
{
    printf("RPC send response:  %s\n", resp);
}

volatile sig_atomic_t running = 1;

void sigint_handler(int sig)
{
    printf("Caught signal %d\n", sig);
    running = 0;
}

int main(void)
{
    signal(SIGINT, sigint_handler);

    thingsboard_ctx* ctx =  thingsboard_init(USE_HTTP);
    printf("Initialized thingsboard\n");

    thingsboard_connect(ctx, "127.0.0.1", 8080, "AvZTC8mOGSmKGdCF05Gx");
    printf("Connected to thingsboard\n");

    int res = thingsboard_telemetry_send(ctx, "{\"temperature\":50}", NULL);
    printf("Published telemetry. Return: %d\n", res);

    res = thingsboard_attributes_publish(ctx, "{\"temperature\":50}");
    printf("Published attributes. Return: %d\n", res);

    res = thingsboard_attributes_request(ctx, 5, "{\"sharedKeys\":\"manoAtributas,kitoks\"}", on_response);
    printf("Requested attributes. Return: %d\n", res);

    thingsboard_device_claim(ctx, "tipoSecret", -1);
    printf("Claimed device. Return: %d\n", res);

    thingsboard_rpc_subscribe(ctx, 5000, on_rpc_update);

    thingsboard_attributes_subscribe(ctx, 5000, on_update);

    thingsboard_rpc_send(ctx, 1, "getTemperature", "param:value", on_rpc_response);

    while(running)
    {
        thingsboard_loop_forever(ctx);
    }

    thingsboard_rpc_unsubscribe(ctx);

    thingsboard_attributes_unsubscribe(ctx);

    thingsboard_provision_device(ctx, "vhw6np4h74cjmvvceusb", "slrtwe67wv0lrwhdca5a");

    thingsboard_device_claim(ctx, "tipoSecret", -1);

    thingsboard_disconnect(ctx);

    thingsboard_cleanup(ctx);

    return 0;
}