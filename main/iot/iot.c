#include "iot.h"

const char *descriptor =
#include "descripor.txt"
    ;
const char *state =
#include "state.txt"
    ;

char *iot_get_descriptor(void)
{
    return descriptor;
}

char *iot_get_state(void)
{
    return state;
}
