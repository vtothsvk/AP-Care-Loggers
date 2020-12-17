#ifndef newAuth_h
#define newAuth_h

//#include <M5StickC.h>
#include <cstring>
#include "mbedtls/md.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include "newAuth_tpyes.h"
#include "authCredentials.h"

class authHandler{
public:
    authHandler();
    int base64url_encode(uint8_t *dst, size_t dlen, size_t *olen, const uint8_t *src, size_t slen);
    int sign(const uint8_t *digest, size_t digestLen, uint8_t *signature, size_t siglen);
    int createJWT(uint8_t *dst, size_t dlen, size_t *olen, long iat);

private:
    char header[200];
    char claim[100];
};

#endif