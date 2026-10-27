#ifndef FRAME_H
#define FRAME_H

#include "usart.h"


typedef struct{
    FRAME_OK;
    FRAME_ERROR;
    FRAME_WAIT
} frame_err_t;

frame_err_t frame_poll(uint16_t len);

#endif // FRAME_H
