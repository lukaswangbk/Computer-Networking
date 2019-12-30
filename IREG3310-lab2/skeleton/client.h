#ifndef _CLIENT_H
#define _CLIENT_H

#include <sys/types.h>

using namespace std;

typedef enum CLIENT_STATE_tag
{
    WAITING = 0,
    PROCESS_LS = 1,
    PROCESS_SEND = 2,
    PROCESS_GET = 3,
    PROCESS_REMOVE = 4,
    SHUTDOWN = 5,
    QUIT = 6
}Client_State_T;

#endif
