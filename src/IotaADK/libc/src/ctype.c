/*
* IotaOS - ctype.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "ctype.h"

int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int isdigit(int c) { return (c >= '0' && c <= '9'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }
int isupper(int c) { return (c >= 'A' && c <= 'Z'); }
int islower(int c) { return (c >= 'a' && c <= 'z'); }
int isprint(int c) { return (c >= 0x20 && c <= 0x7E); }
int ispunct(int c) { return isprint(c) && !isalnum(c) && !isspace(c); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isblank(int c) { return c == ' ' || c == '\t'; }
int iscntrl(int c) { return (c >= 0 && c <= 0x1F) || c == 0x7F; }
int isgraph(int c) { return c > 0x20 && c <= 0x7E; }

int toupper(int c) { return islower(c) ? c - ('a' - 'A') : c; }
int tolower(int c) { return isupper(c) ? c + ('a' - 'A') : c; }