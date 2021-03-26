#include "newAuth.h"

authHandler::authHandler() {
    sprintf(this -> header, "{\"alg\": \"ES256\", \"typ\": \"JWT\", \"kid\": \"%s\"}", kid);
}//authHandler

int authHandler::base64url_encode(uint8_t *dst, size_t dlen, size_t *olen, const uint8_t *src, size_t slen) {
    int ret = 0;
    size_t elen;

    //encode to base64
    ret = mbedtls_base64_encode(dst, dlen, &elen, src, slen);
    if (ret) return ret;

    //adjust output to comply with base64url
    for (uint8_t index = 0; index < elen; index++) {
        if (dst[index] == '+') {
            dst[index] = '-';
        } else if (dst[index] == '/') {
            dst[index] = '_';
        } else if (dst[index] == '=') {
            dst[index] = 0;
            elen = index;
        }
    }//for (uint8_t index = 0; index < elen; index++)

    *olen = elen;
    return ret;
}//base64url_endoce

int authHandler::sign(const uint8_t *digest, size_t digestLen, uint8_t *signature, size_t siglen) {
    int ret = 0;
    
    //create and init pkey context
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    
    //parse rpivate key
    ret = mbedtls_pk_parse_key(&pk, (unsigned char*)PRIVATE_KEY, strlen(PRIVATE_KEY) + 1, NULL, 0);
    if (ret) return ret;

    //create init ecdsa context
    mbedtls_ecdsa_context ecdsa;
    mbedtls_ecdsa_init(&ecdsa);
    
    //setup ecdsa context from pk context
    ret = mbedtls_ecdsa_from_keypair(&ecdsa, mbedtls_pk_ec(pk));
    if (ret) return ret;
    
    //create and init mpi points
    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    
    //create signature
    ret = mbedtls_ecdsa_sign_det(&ecdsa.grp, &r, &s, &ecdsa.d, digest, digestLen, MBEDTLS_MD_SHA256);
    if (ret) return ret;

    //write r and s to the signature
    ret = mbedtls_mpi_write_binary(&r, signature, mbedtls_mpi_size(&r));
    if (ret) return ret;

    ret = mbedtls_mpi_write_binary(&s, signature + mbedtls_mpi_size(&r), mbedtls_mpi_size(&s));
    if (ret) return ret;

    siglen = mbedtls_mpi_size(&r) + mbedtls_mpi_size(&s);

    //cleanup
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecdsa_free(&ecdsa);
    mbedtls_pk_free(&pk);

    return ret;
}//sign

int authHandler::createJWT(uint8_t *dst, size_t dlen, size_t *olen, long iat) {
    int ret = 0;
    size_t cursor = 0;
    
    //encode header
    ret = this -> base64url_encode(dst, dlen, &cursor, (const uint8_t*)this -> header, strlen(this -> header));
    if (ret) return ret;

    //append delimiter
    dst[cursor++] = '.';

    //encode claims
    size_t c64len;
    sprintf(this -> claim, "{\"iat\": %ld,\"exp\": %ld, \"sub\": \"%s\"}", iat, iat + 300, SN);
    ret = this -> base64url_encode(dst + cursor, dlen - cursor, &c64len, (const uint8_t*)this -> claim, strlen(this -> claim));
    if (ret) return ret;

    //add encoded claims length to the total
    cursor += c64len;
    
    //hash the buffer
    uint8_t hash[32];
    ret = mbedtls_sha256_ret(dst, cursor, hash, 0);
    if (ret) return ret;
    
    //create signature
    uint8_t signature[secp256r1SignSize];
    ret = this -> sign(hash, sizeof(hash), signature, sizeof(signature));
    if(ret) return ret;

    //append delimiter
    dst[cursor++] = '.';

    //encode the signature
    size_t s64len;
    ret = this -> base64url_encode(dst + cursor, dlen - cursor, &s64len, signature, sizeof(signature));
    if (ret) return ret;

    //add the length of the encoded signature to the total jwt length
    cursor += s64len;
    *olen = cursor;

    return ret;
}//createJWT