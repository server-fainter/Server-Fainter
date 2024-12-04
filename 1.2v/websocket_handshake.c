#include "websocket_handshake.h"

#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

// SHA-1 해시 함수
void sha1_hash(const char *input, size_t len, unsigned char *output) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) {
        fprintf(stderr, "EVP_MD_CTX_new failed\n");
        return;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL) != 1) {
        fprintf(stderr, "EVP_DigestInit_ex failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    if (EVP_DigestUpdate(mdctx, input, len) != 1) {
        fprintf(stderr, "EVP_DigestUpdate failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    unsigned int md_len;
    if (EVP_DigestFinal_ex(mdctx, output, &md_len) != 1) {
        fprintf(stderr, "EVP_DigestFinal_ex failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    EVP_MD_CTX_free(mdctx);
}

// Base64 인코딩 함수
void base64_encode(const unsigned char *input, int length, char *output, int output_size) {
    int encoded_length = EVP_EncodeBlock((unsigned char *)output, input, length);
    if (encoded_length < 0 || encoded_length >= output_size) {
        fprintf(stderr, "Base64 encoding failed\n");
    }
    output[encoded_length] = '\0';
}

// WebSocket Accept 키 생성 함수
void generate_websocket_accept_key(const char *client_key, char *accept_key) {
    const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined_key[256];
    unsigned char sha1_result[SHA_DIGEST_LENGTH];

    snprintf(combined_key, sizeof(combined_key), "%s%s", client_key, guid);

    sha1_hash(combined_key, strlen(combined_key), sha1_result);
    base64_encode(sha1_result, SHA_DIGEST_LENGTH, accept_key, 256);
}
