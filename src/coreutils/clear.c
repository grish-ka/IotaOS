/*
* IotaOS - clear.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"

int main(void) {
    fputs("\033[H\033[2J", stdout);
    return 0;
}
