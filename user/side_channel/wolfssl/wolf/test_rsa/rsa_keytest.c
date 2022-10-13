#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

#define WOLFSSL_DEBUG_MATH
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/asn_public.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/integer.h>
#include <wolfssl/wolfcrypt/tfm.h>
#include <wolfssl/wolfcrypt/wolfmath.h>



void print_hex(unsigned char * data, size_t l){
    for (int i = 0; i < l; i ++) {
        printf("%02x", data[i]);
    }
    putchar('\n');
}


void dump_bigint(mp_int * x, char * label){
    printf("----------------------------------------------------------------\n");
    printf("%s sign: %u\n", label, x->sign);
    printf("%s words used: %u\n\n", label, x->used);
    for(int i = 0; i < x->used; i++)
        printf("dword[%d]:   %llx\n", i, x->dp[i]);
    printf("----------------------------------------------------------------\n");
}


#define KEY_FILE "private.der"
#define DER_BLOB_SIZE     609

int main(int argc, char *argv[])
{

    RsaKey key;   
    RNG rng;
    int ret;
    size_t sz;
    FILE * derFile = NULL;
    byte der[DER_BLOB_SIZE] = {0};


    ret = wc_InitRng(&rng);
    assert(ret == 0);


    ret = wc_InitRsaKey(&key, 0);
    assert(ret == 0);

    

    word32 idx = 0;
    derFile = fopen(KEY_FILE, "r");
    sz = fread(der, 1, DER_BLOB_SIZE, derFile);
    assert(sz == DER_BLOB_SIZE);
    
    ret = wc_RsaPrivateKeyDecode(der, &idx, &key, sizeof(der));
    assert(ret == 0);

    ret = wc_CheckRsaKey(&key);
    assert(ret == 0);


    // print_hex(der, sizeof(der));

    dump_bigint(&key.n, "n");
    dump_bigint(&key.e, "e");
    dump_bigint(&key.d, "d");
    dump_bigint(&key.p, "p");
    dump_bigint(&key.q, "q");
    dump_bigint(&key.dP, "dP");
    dump_bigint(&key.dQ, "dQ");
    dump_bigint(&key.u, "u");

    return 0;
}
