#ifndef __XXTEA_H__
#define __XXTEA_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int en_xxtea(char *out, size_t olen, const char *buf, size_t len, const char *key, size_t kl);

#endif