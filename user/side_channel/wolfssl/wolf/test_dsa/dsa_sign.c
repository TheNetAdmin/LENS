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
#include <wolfssl/wolfcrypt/dsa.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/integer.h>
#include <wolfssl/wolfcrypt/tfm.h>
#include <wolfssl/wolfcrypt/wolfmath.h>



#define P_SIZE            1024
#define Q_SIZE            160
#define GENKEY            0


void print_hex(unsigned char * data, size_t l){
    for (int i = 0; i < l; i ++) {
        printf("%02x", data[i]);
    }
    putchar('\n');
}

Sha sha;
byte hash[SHA_DIGEST_SIZE];
byte signature[40];
#define KEY_FILE "dsa1024.der"
#define DER_BLOB_SIZE     447


RNG rng;
DsaKey key;
byte message[10];

int answer = 0, ret = 0;

void dump_bigint(mp_int * x, char * label){
    printf("----------------------------------------------------------------\n");
    printf("%s sign: %u\n", label, x->sign);
    printf("%s words used: %u\n\n", label, x->used);
    for(int i = 0; i < x->used; i++)
        printf("dword[%d]:   %llx\n", i, x->dp[i]);
    printf("----------------------------------------------------------------\n");
}


int main(int argc, char *argv[])
{
    if(argc < 2){
        printf("usage: %s <iteration> <file>\n", argv[0]);
        return 1;
    }
    memset(message, 'A', sizeof(message));
    memset(signature, 0, sizeof(signature));

    ret = wc_InitSha(&sha);
    assert(ret == 0);

    ret = wc_ShaUpdate(&sha, message, sizeof(message));
    assert(ret == 0);

    ret = wc_ShaFinal(&sha, hash);
    assert(ret == 0);

    print_hex(hash, sizeof(hash));

    ret = wc_InitRng(&rng);
    assert(ret == 0);

    ret = wc_InitDsaKey(&key);
    assert(ret == 0);


    size_t sz;
    FILE * derFile = NULL;
    byte der[DER_BLOB_SIZE] = {0};

#if GENKEY
    ret = wc_MakeDsaParameters(&rng, P_SIZE, &key); 
    assert(ret == 0);
    ret = wc_MakeDsaKey(&rng, &key); 
    assert(ret == 0);
    ret = wc_DsaKeyToDer(&key, der, sizeof(der));
    derFile = fopen("private.der", "w");
    sz = fwrite(der, 1, DER_BLOB_SIZE, derFile);
    assert(sz == DER_BLOB_SIZE);
    fclose(derFile);
    print_hex(der, sizeof(der));
#else
    word32 idx = 0;
    derFile = fopen(KEY_FILE, "r");
    sz = fread(der, 1, DER_BLOB_SIZE, derFile);
    assert(sz == DER_BLOB_SIZE);
    ret = wc_DsaPrivateKeyDecode(der, &idx, &key, sizeof(der));
    assert(ret == 0);
#endif

    dump_bigint(&key.x, "x");
    dump_bigint(&key.q, "q");
    

    FILE * sigFile = fopen(argv[2], "w+");

#define PROGRESS_STEP 1000
    for(uint64_t i = 0; i < atol(argv[1]); i++)
    {   
        if(i % PROGRESS_STEP == 1){
            printf("Progress: %lu done\n", i - 1);
        }

        ret = wc_DsaSign(hash, signature, &key, &rng);
        assert(ret == 0);

        ret = wc_DsaVerify(hash, signature, &key, &answer);
        assert(ret == 0);
        assert(answer == 1);
        if(answer){
            print_hex(signature, sizeof(signature));
            fwrite(signature, 1, sizeof(signature), sigFile);
            fflush(sigFile);
        }
        
            
    }

    fclose(sigFile);
    return 0;
}
