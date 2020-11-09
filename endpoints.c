#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "coap.h"
#include "wren.h"

static char light = '0';
static char wrenAbout[] = WREN_VERSION_STRING;   //"Wren v0.3.0";

static char rsp[1500] = "";
static char vmScratch[1000] = ""; bool vmScratchFull = false;
static char tempStr[1000] = "";

void build_rsp(void);

#ifdef ARDUINO
#include "Arduino.h"
static int led = 6;
void endpoint_setup(void)
{
    pinMode(led, OUTPUT);
    build_rsp();
}
#else
#include <stdio.h>

// ----------------------------------------------------------------------
// Only handles \n and \" for wren source over CoAP ... not general purpose.
uint8_t *unescape(const uint8_t *in, size_t inLen) {

    int outi = 0;
    // printf("input len: %ld\n", inLen);

    if (in == NULL) return "";
    uint8_t *out = (uint8_t *) malloc(inLen + 1);

    for (int ini = 0; ini < inLen; ini++) {
        if ((uint8_t)in[ini] != '\\') {
            out[outi] = in[ini];
        } else {
            ini++;
            if (!(ini<inLen)) { return ""; } // bad escape string
            if (in[ini] == 'n') {
                out[outi] = 0x0A;
            } else {
                out[outi] = in[ini];
            };
        }
        outi++;
    }
    // printf("outi: %i\n", outi);
    out[outi] = '\0';
    return out;
};
// ----------------------------------------------------------------------

static void writeFn(WrenVM* vm, const char* text)
{

    printf("%s", text);
    if (vmScratchFull == true) return;
    if ((strlen(vmScratch) + strlen(text) + 18) > 999) {
        strcat(vmScratch, "<output truncated>");
        vmScratchFull = true;
    } else {
        strcat(vmScratch, text);
    }
}

void errorFn(WrenVM* vm, WrenErrorType errorType,
             const char* module, const int line,
             const char* msg)
{
    switch (errorType)
    {
        case WREN_ERROR_COMPILE:
        {
            printf("[%s line %d] [Error] %s\n", module, line, msg);
        } break;
        case WREN_ERROR_STACK_TRACE:
        {
            printf("[%s line %d] in %s\n", module, line, msg);
        } break;
        case WREN_ERROR_RUNTIME:
        {
            printf("[Runtime Error] %s\n", msg);
        } break;
    }
}

// ----------------------------------------------------------------------

void endpoint_setup(void)
{
    build_rsp();
}
#endif

static const coap_endpoint_path_t path_well_known_core = {2, {".well-known", "core"}};
static int handle_get_well_known_core(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    return coap_make_response(scratch, outpkt, (const uint8_t *)rsp, strlen(rsp), id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_APPLICATION_LINKFORMAT);
}
// ----------------------------------------------------------------------
static const coap_endpoint_path_t path_wren_about = {2, {"wren", "about"}};
static const coap_endpoint_path_t path_wren_echo = {2, {"wren", "echo"}};
static const coap_endpoint_path_t path_wren_try = {2, {"wren", "try"}};
static int handle_get_wren_about(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    return coap_make_response(scratch, outpkt, (const uint8_t *)&wrenAbout, strlen(wrenAbout), id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_APPLICATION_LINKFORMAT);
}
static int handle_post_wren_echo(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    if (inpkt->payload.len == 0) {
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_BAD_REQUEST,
                                  COAP_CONTENTTYPE_TEXT_PLAIN);
    } else {
        return coap_make_response(scratch, outpkt, (const uint8_t *)inpkt->payload.p, inpkt->payload.len, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CHANGED, COAP_CONTENTTYPE_TEXT_PLAIN);
    }
}
static int handle_post_wren_try(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    if (inpkt->payload.len == 0) {
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_BAD_REQUEST,
                                  COAP_CONTENTTYPE_TEXT_PLAIN);
    } else {

        // const char* script = "System.print(\"I am running in a VM!\")";

        uint8_t *sentStr = unescape(inpkt->payload.p, inpkt->payload.len);
        const char* script = (const char*)sentStr; // (const char*)unescape(inpkt->payload.p);
        // printf("<--- Start in --->\n%s\n<--- end in --->\n", script);
        //const char* script = (const char*)inpkt->payload.p;
        //printf("%s\n", (const char*)inpkt->payload.p);
        // const char* script = "for (yPixel in 0...20) {\n  var y = yPixel / 12 - 1\n for (xPixel in 0...75) {\n var x = xPixel / 30 - 2\n var x0 = x\n var y0 = y\n var iter = 0\n while (iter < 11 && x0 * x0 + y0 * y0 <= 4) {\n var x1 = (x0 * x0) - (y0 * y0) + x\n var y1 = 2 * x0 * y0 + y\n x0 = x1\n y0 = y1\n iter = iter + 1\n    }\n System.write(\" .-:;+=xX$& \"[iter])\n  }\n  System.print(\"\")\n}\n";

        WrenConfiguration config;
        wrenInitConfiguration(&config);
        config.writeFn = &writeFn;
        config.errorFn = &errorFn;

        const char* module = "main";
        WrenVM* vm = wrenNewVM(&config);
        WrenInterpretResult result = wrenInterpret(vm, module, script);

        switch (result) {
            case WREN_RESULT_COMPILE_ERROR:
            { printf("Compile Error!\n"); } break;
            case WREN_RESULT_RUNTIME_ERROR:
            { printf("Runtime Error!\n"); } break;
            case WREN_RESULT_SUCCESS:
            { printf("Success!\n"); } break;
        }

        free(sentStr);
        wrenFreeVM(vm);

        strcpy(tempStr, vmScratch);
        vmScratch[0] = '\0'; vmScratchFull = 0;

        return coap_make_response(scratch, outpkt, (const uint8_t *)tempStr, strlen(tempStr), id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CHANGED, COAP_CONTENTTYPE_TEXT_PLAIN);
    }
}
// ----------------------------------------------------------------------
static const coap_endpoint_path_t path_light = {1, {"light"}};
static int handle_get_light(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    return coap_make_response(scratch, outpkt, (const uint8_t *)&light, 1, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CONTENT, COAP_CONTENTTYPE_TEXT_PLAIN);
}

static int handle_put_light(coap_rw_buffer_t *scratch, const coap_packet_t *inpkt, coap_packet_t *outpkt, uint8_t id_hi, uint8_t id_lo)
{
    if (inpkt->payload.len == 0)
        return coap_make_response(scratch, outpkt, NULL, 0, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_BAD_REQUEST, COAP_CONTENTTYPE_TEXT_PLAIN);
    if (inpkt->payload.p[0] == '1')
    {
        light = '1';
#ifdef ARDUINO
        digitalWrite(led, HIGH);
#else
        printf("ON\n");
#endif
        return coap_make_response(scratch, outpkt, (const uint8_t *)&light, 1, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CHANGED, COAP_CONTENTTYPE_TEXT_PLAIN);
    }
    else
    {
        light = '0';
#ifdef ARDUINO
        digitalWrite(led, LOW);
#else
        printf("OFF\n");
#endif
        return coap_make_response(scratch, outpkt, (const uint8_t *)&light, 1, id_hi, id_lo, &inpkt->tok, COAP_RSPCODE_CHANGED, COAP_CONTENTTYPE_TEXT_PLAIN);
    }
}

const coap_endpoint_t endpoints[] =
        {
                {COAP_METHOD_GET, handle_get_well_known_core, &path_well_known_core, "ct=40"},
                {COAP_METHOD_GET, handle_get_light, &path_light, "ct=0"},
                {COAP_METHOD_PUT, handle_put_light, &path_light, NULL},
                {COAP_METHOD_GET, handle_get_wren_about, &path_wren_about, "ct=0"},
                {COAP_METHOD_POST, handle_post_wren_echo, &path_wren_echo, "ct=0"},
                {COAP_METHOD_POST, handle_post_wren_try, &path_wren_try, "ct=0"},
                {(coap_method_t)0, NULL, NULL, NULL}
        };

void build_rsp(void)
{
    uint16_t len = 1500;
    const coap_endpoint_t *ep = endpoints;
    int i;

    len--; // Null-terminated string

    while(NULL != ep->handler)
    {
        if (NULL == ep->core_attr) {
            ep++;
            continue;
        }

        if (0 < strlen(rsp)) {
            strncat(rsp, ",", len);
            len--;
        }

        strncat(rsp, "<", len);
        len--;

        for (i = 0; i < ep->path->count; i++) {
            strncat(rsp, "/", len);
            len--;

            strncat(rsp, ep->path->elems[i], len);
            len -= strlen(ep->path->elems[i]);
        }

        strncat(rsp, ">;", len);
        len -= 2;

        strncat(rsp, ep->core_attr, len);
        len -= strlen(ep->core_attr);

        ep++;
    }
}

