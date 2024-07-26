#include "versat_crypto.h"

#include "versat_accel.h"
#include "unitConfiguration.h"

#include <string.h>

#include "printf.h"

#include "controlbits.h"
#include "benes.h"
#include "crypto_declassify.h"
#include "crypto_uint64.h"
#include "params.h"
#include "pk_gen.h"
#include "sk_gen.h"
#include "root.h"
#include "uint64_sort.h"
#include "util.h"
#include "crypto_hash.h"
#include "decrypt.h"
#include "randombytes.h"

#include "arena.h"

// Prevents "cast increases required alignment" warnings by gcc
// When mat is allocated in such a way that rows are guaranteed to be aligned
#define CAST_PTR(TYPE,PTR) ((TYPE) ((void*) (PTR)))

void clear_cache();

static McElieceConfig* vec;
static void* matAddr;

#define SBYTE (SYS_N / 8)
#define SINT (SBYTE / 4)

void PrintRow(unsigned char* view){
#if 0
    for(int i = 0; i < SBYTE; i++){
        printf("%02x",view[i]);
        if(i % 16 == 0 && i != 0){
            printf("\n");
        }
    }
#else
    for(int i = 0; i < 16; i++){
        printf("%02x",view[i]);
    }
    printf("\n");
    for(int i = SBYTE - 16; i < SBYTE; i++){
        printf("%02x",view[i]);
    }
    printf("\n");
#endif
}

void PrintFullRow(unsigned char* view){
    for(int i = 0; i < SBYTE; i++){
        printf("%02x",view[i]);
        if(i % 16 == 0 && i != 0){
            printf("\n");
        }
    }
}

void ReadRow(uint32_t* row){
    for (int i = 0; i < SINT; i++){
        row[i] = VersatUnitRead(matAddr,i);
    }
}

void VersatLoadRow(uint32_t* row){
    VersatMemoryCopy(matAddr,CAST_PTR(int*,row),SINT * sizeof(int));
}

void VersatPrintRow(){
    uint32_t values[SINT];
    ReadRow(values);
    PrintRow(CAST_PTR(uint8_t*,values));
}

uint8_t* GetVersatRowForTests(){  
    static uint8_t buffer[SBYTE];
    ReadRow(CAST_PTR(uint32_t*,buffer));
    return buffer;
}

void PrintSimpleMat(unsigned char** mat,int centerRow){
    PrintRow(mat[0]);
    PrintRow(mat[centerRow - 1]);
    PrintRow(mat[centerRow]);
    PrintRow(mat[centerRow + 1]);
    PrintRow(mat[PK_NROWS - 1]);
}

void PrintFullMat(unsigned char** mat){
    printf("Printing full mat:\n");
    for(int i = 0; i < PK_NROWS; i++){
        PrintFullRow(mat[i]);
    }
}

void VersatMcElieceLoop1(uint8_t *row, uint8_t mask,bool first){
    static uint8_t savedMask = 0;
    uint32_t *row_int = CAST_PTR(uint32_t*,row);

    ConfigureSimpleVReadShallow(&vec->row, SINT, (int*) row_int);
    if(first){
        vec->mat.in0_wr = 0;
    } else {
        uint32_t mask_int = (savedMask) | (savedMask << 8) | (savedMask << 8*2) | (savedMask << 8*3);
        vec->mask.constant = mask_int;
        vec->mat.in0_wr = 1;
    }

    EndAccelerator();
    StartAccelerator();

    savedMask = mask;
}

// Can only be called if k != row. Care
void VersatMcElieceLoop2(unsigned char** mat,int timesCalled,int k,int row,uint8_t mask){
    static uint8_t savedMask = 0;

    int toRead =    k;
    int toCompute = ((toRead - 1    == row) ? toRead - 2    : toRead - 1);
    int toWrite =   ((toCompute - 1 == row) ? toCompute - 2 : toCompute - 1);

    if(toRead < PK_NROWS){
        int *toRead_int = CAST_PTR(int*,mat[toRead]);

        vec->mat.in0_wr = 0;

        ConfigureSimpleVReadShallow(&vec->row, SINT,toRead_int);        
    } else {
        vec->row.enableRead = 0;
        toRead = -9;
    }
    
    if(timesCalled >= 1 && toCompute >= 0 && toCompute < PK_NROWS){
        uint32_t mask_int = (savedMask) | (savedMask << 8) | (savedMask << 8*2) | (savedMask << 8*3);

        vec->mask.constant = mask_int;
    } else {
        ConfigureSimpleVWrite(&vec->writer, SINT, (int*) NULL);
        vec->writer.enableWrite = 0;
        toCompute = -9;
    }
    
    if(timesCalled >= 2 && toWrite >= 0){
        int* toWrite_int = CAST_PTR(int*,mat[toWrite]);
        ConfigureSimpleVWrite(&vec->writer, SINT,toWrite_int);
    } else {
        toWrite = -9;
        vec->writer.enableWrite = 0;
    }

    EndAccelerator();
    StartAccelerator();

    savedMask = mask;
}

static crypto_uint64 uint64_is_equal_declassify(uint64_t t, uint64_t u) {
    crypto_uint64 mask = crypto_uint64_equal_mask(t, u);
    crypto_declassify(&mask, sizeof mask);
    return mask;
}

static crypto_uint64 uint64_is_zero_declassify(uint64_t t) {
    crypto_uint64 mask = crypto_uint64_zero_mask(t);
    crypto_declassify(&mask, sizeof mask);
    return mask;
}


#if 0
module McEliece(){
   ReadWriteMem mat;
   VRead row;
   VWrite writer;
   Const mask;
#
   a = row & mask;
   b = mat ^ a;

   c = mat & mask;
   d = row ^ c;

   b -> mat;
   d -> writer;
}
#endif

void TestMcEliece(){
    // Init needed values for versat later on.  
    CryptoAlgosConfig* topConfig = (CryptoAlgosConfig*) accelConfig;
    vec = (McElieceConfig*) &topConfig->eliece;
    matAddr = (void*) TOP_eliece_mat_addr;

    vec->mask.constant = 0xff;

    int matValues[] = {0,1,2,3,4,5,6,7};
    int rowValues[] = {8,9,10,11,12,13,14,15};
    int result[8] = {};

    vec->mat.iterA = 1;
    vec->mat.incrA = 1;
    vec->mat.iterB = 1;
    vec->mat.incrB = 1;
    vec->mat.perA = 8;
    vec->mat.dutyA = 8;
    vec->mat.perB = 8;
    vec->mat.dutyB = 8;

    VersatMemoryCopy(matAddr,matValues,8 * sizeof(int));
    ConfigureSimpleVRead(&vec->row,8,rowValues);

    RunAccelerator(1);
    vec->mat.in0_wr = 1;
    RunAccelerator(1);

    for(int i = 0; i < 8; i++){
        printf("%x ",VersatUnitRead(matAddr,i));
    }
    printf("\n");

    //ConfigureSimpleVReadBare(&vec->row);
}

/* input: secret key sk */
/* output: public key pk */
int Versat_pk_gen(unsigned char *pk, unsigned char *sk, const uint32_t *perm, int16_t *pi) {
    int i, j, k;
    int row, c;

    int mark = MarkArena(globalArena);

    printf("SK:%02x%02x%02x%02x%02x%02x%02x%02x\n",sk[0],sk[1],sk[2],sk[3],sk[4],sk[5],sk[6],sk[7]);
    printf("PE:%02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
    printf("PI:%02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);
    
    // Init needed values for versat later on.  
    CryptoAlgosConfig* topConfig = (CryptoAlgosConfig*) accelConfig;
    vec = (McElieceConfig*) &topConfig->eliece;
    matAddr = (void*) TOP_eliece_mat_addr;

    ConfigureSimpleVReadBare(&vec->row);

    vec->mat.iterA = 1;
    vec->mat.incrA = 1;
    vec->mat.iterB = 1;
    vec->mat.incrB = 1;
    vec->mat.perA = SINT + 1;
    vec->mat.dutyA = SINT + 1;
    vec->mat.perB = SINT + 1;
    vec->mat.dutyB = SINT + 1;

    uint64_t buf[ 1 << GFBITS ];

    unsigned char** mat = PushArray(globalArena,PK_NROWS,unsigned char*);
    for(int i = 0; i < PK_NROWS; i++){
        mat[i] = PushArray(globalArena,SYS_N / 8,unsigned char); // This guarantees that each row is properly aligned to a 32 bit boundary.
    }

    unsigned char mask;
    unsigned char b;

    gf* g = PushArray(globalArena,SYS_T + 1,gf);
    gf* L = PushArray(globalArena,SYS_N,gf); // support
    gf* inv = PushArray(globalArena,SYS_N,gf);

    //

    g[ SYS_T ] = 1;

    for (i = 0; i < SYS_T; i++) {
        g[i] = load_gf(sk);
        sk += 2;
    }

    for (i = 0; i < (1 << GFBITS); i++) {
        buf[i] = perm[i];
        buf[i] <<= 31;
        buf[i] |= i;
    }

    uint64_sort(buf, 1 << GFBITS);

    for (i = 1; i < (1 << GFBITS); i++) {
        if (uint64_is_equal_declassify(buf[i - 1] >> 31, buf[i] >> 31)) {
            PopArena(globalArena,mark);
            return -1;
        }
    }

    for (i = 0; i < (1 << GFBITS); i++) {
        pi[i] = buf[i] & GFMASK;
    }
    for (i = 0; i < SYS_N;         i++) {
        L[i] = bitrev(pi[i]);
    }

    // filling the matrix

    root(inv, g, L);

    for (i = 0; i < SYS_N; i++) {
        inv[i] = gf_inv(inv[i]);
    }

    for (i = 0; i < PK_NROWS; i++) {
        for (j = 0; j < SYS_N / 8; j++) {
            mat[i][j] = 0;
        }
    }

    for (i = 0; i < SYS_T; i++) {
        for (j = 0; j < SYS_N; j += 8) {
            for (k = 0; k < GFBITS;  k++) {
                b  = (inv[j + 7] >> k) & 1;
                b <<= 1;
                b |= (inv[j + 6] >> k) & 1;
                b <<= 1;
                b |= (inv[j + 5] >> k) & 1;
                b <<= 1;
                b |= (inv[j + 4] >> k) & 1;
                b <<= 1;
                b |= (inv[j + 3] >> k) & 1;
                b <<= 1;
                b |= (inv[j + 2] >> k) & 1;
                b <<= 1;
                b |= (inv[j + 1] >> k) & 1;
                b <<= 1;
                b |= (inv[j + 0] >> k) & 1;

                mat[ i * GFBITS + k ][ j / 8 ] = b;
            }
        }

        for (j = 0; j < SYS_N; j++) {
            inv[j] = gf_mul(inv[j], L[j]);
        }
    }

    // gaussian elimination
    for (i = 0; i < (PK_NROWS + 7) / 8; i++) {
        for (j = 0; j < 8; j++) {
            row = i * 8 + j;

            printf("%d\n",i);

            if (row >= PK_NROWS) {
                break;
            }

            uint32_t *out_int = CAST_PTR(uint32_t*,mat[row]);
            EndAccelerator();

            //printf("%02x%02x%02x%02x%02x%02x%02x%02x\n",mat[row][0],mat[row][1],mat[row][2],mat[row][3],mat[row][4],mat[row][5],mat[row][6],mat[row][7]);

            VersatLoadRow(out_int);
            bool first = true;
            for (k = row + 1; k < PK_NROWS; k++) {
                mask = mat[ row ][ i ] ^ mat[ k ][ i ];
                mask >>= j;
                mask &= 1;
                mask = -mask;

                VersatMcElieceLoop1(mat[k],mask,first);

                // We could fetch the value from Versat, but it's easier to calculate it CPU side.
                mat[row][i] ^= mat[k][i] & mask;
                first = false;
            }

            // Last run, use valid data to compute last operation
            VersatMcElieceLoop1(mat[PK_NROWS - 1],0,false); // TODO: Have a proper function instead of sending a "fake" adress

            EndAccelerator();

            if ( uint64_is_zero_declassify((mat[ row ][ i ] >> j) & 1) ) { // return if not systematic
               PopArena(globalArena,mark);
               return -1;
            }

            ReadRow(out_int);

            //printf("%02x%02x%02x%02x%02x%02x%02x%02x\n",mat[row][0],mat[row][1],mat[row][2],mat[row][3],mat[row][4],mat[row][5],mat[row][6],mat[row][7]);

            //uart_finish();
            //exit(0);

            int index = 0;
            for (k = 0; k < PK_NROWS; k++) {
                if (k != row) {
                    mask = mat[k][i] >> j;
                    mask &= 1;
                    mask = -mask;

                    VersatMcElieceLoop2(mat,index,k,row,mask);
                    index += 1;
                }
            }

            VersatMcElieceLoop2(mat,index++,PK_NROWS,row,0);
            VersatMcElieceLoop2(mat,index++,PK_NROWS + 1,row,0);
            vec->writer.enableWrite = 0;

            clear_cache();
        }
    }

    for (i = 0; i < PK_NROWS; i++) {
        memcpy(pk + i * PK_ROW_BYTES, mat[i] + PK_NROWS / 8, PK_ROW_BYTES);
    }

    PopArena(globalArena,mark);
    return 0;
}

int VersatMcEliece
(
    unsigned char *pk,
    unsigned char *sk
) {
    int i;
    int iteration = 0;
    unsigned char seed[ 33 ] = {64};
    //unsigned char r[ SYS_N / 8 + (1 << GFBITS)*sizeof(uint32_t) + SYS_T * 2 + 32 ] = {0};
    unsigned char *rp, *skp;

    memset(seed,64,sizeof(unsigned char) * 33);

    printf("SEED: %c%c%c%c%c%c%c%c\n",seed[0],seed[1],seed[2],seed[3],seed[4],seed[5],seed[6],seed[7]);

    int sizeofR = SYS_N / 8 + (1 << GFBITS)*sizeof(uint32_t) + SYS_T * 2 + 32;
    int sizeofF = SYS_T * sizeof(gf);
    int sizeofPerm = (1 << GFBITS) * sizeof(uint32_t);

    int mark = MarkArena(globalArena);

    printf("SEED: %c%c%c%c%c%c%c%c\n",seed[0],seed[1],seed[2],seed[3],seed[4],seed[5],seed[6],seed[7]);

    unsigned char* r = PushAndZeroArray(globalArena,sizeofR,unsigned char);

    gf* f = PushAndZeroArray(globalArena,SYS_T + 1000,gf);
    printf("%p %p %d %d\n",globalArena,globalArena->ptr,globalArena->used,globalArena->allocated);
    printf(" F0: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
    gf* irr = PushAndZeroArray(globalArena,SYS_T + 1000,gf);
    printf("%p %p %d %d\n",globalArena,globalArena->ptr,globalArena->used,globalArena->allocated);
    printf(" F0: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
    uint32_t* perm = PushAndZeroArray(globalArena,(1 << GFBITS) + 1000,uint32_t);
    printf("%p %p %d %d\n",globalArena,globalArena->ptr,globalArena->used,globalArena->allocated);
    printf(" F0: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
    int16_t* pi = PushAndZeroArray(globalArena,(1 << GFBITS) + 1000,int16_t);
    printf("%p %p %d %d\n",globalArena,globalArena->ptr,globalArena->used,globalArena->allocated);
    printf(" F0: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);

    for(int i = 0; i < 16; i++){
        pi[i] = 0;
    }

#if 0
    gf f[ SYS_T ]; // element in GF(2^mt)
    gf irr[ SYS_T ]; // Goppa polynomial
    uint32_t perm[ 1 << GFBITS ]; // random permutation as 32-bit integers
    int16_t pi[ 1 << GFBITS ]; // random permutation

    printf("F: %d\n",sizeof(f));
    printf("I: %d\n",sizeof(irr));
    printf("p: %d\n",sizeof(perm));
    printf("P: %d\n",sizeof(pi));
    memset(f,0,sizeof(f));
    memset(irr,0,sizeof(irr));
    memset(perm,0,sizeof(perm));
    memset(pi,0,sizeof(pi));
#endif

    printf(" F0: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
    printf("IR0: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
    printf("PE0: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
    printf("PI0: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);

    randombytes(seed + 1, 32);

    printf(" F1: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
    printf("IR1: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
    printf("PE1: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
    printf("PI1: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);

    while (1) {
        //printf("iteration: %d\n",iteration++);
        rp = &r[ sizeofR - 32 ];
        skp = sk;

        printf(" F2: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
        printf("IR2: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
        printf("PE2: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
        printf("PI2: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);

        // expanding and updating the seed
        shake(r, sizeofR, seed, 33);
        memcpy(skp, seed + 1, 32);
        skp += 32 + 8;
        memcpy(seed + 1, &r[ sizeofR - 32 ], 32);

        printf(" F3: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
        printf("IR3: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
        printf("PE3: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
        printf("PI3: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);
        // generating irreducible polynomial
        rp -= sizeofF;

        for (i = 0; i < SYS_T; i++) {
            f[i] = load_gf(rp + i * 2);
        }

        printf(" F4: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
        printf("IR4: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
        printf("PE4: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
        printf("PI4: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);
        if (genpoly_gen(irr, f)) {
            continue;
        }

        printf(" F5: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
        printf("IR5: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
        printf("PE5: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
        printf("PI5: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);

        for (i = 0; i < SYS_T; i++) {
            store_gf(skp + i * 2, irr[i]);
        }

        printf(" F6: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
        printf("IR6: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
        printf("PE6: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
        printf("PI6: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);

        skp += IRR_BYTES;

        // generating permutation
        rp -= sizeofPerm;

        for (i = 0; i < (1 << GFBITS); i++) {
            perm[i] = load4(rp + i * 4);
        }

        printf(" F7: %02x%02x%02x%02x%02x%02x%02x%02x\n",f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
        printf("IR7: %02x%02x%02x%02x%02x%02x%02x%02x\n",irr[0],irr[1],irr[2],irr[3],irr[4],irr[5],irr[6],irr[7]);
        printf("PE7: %02x%02x%02x%02x%02x%02x%02x%02x\n",perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7]);
        printf("PI7: %02x%02x%02x%02x%02x%02x%02x%02x\n",pi[0],pi[1],pi[2],pi[3],pi[4],pi[5],pi[6],pi[7]);
     
        //uart_finish();
        //exit(0);

#if 1
        if (Versat_pk_gen(pk, skp - IRR_BYTES, perm, pi)) {
            continue;
        }
#endif

#if 0
        if (pk_gen(pk, skp - IRR_BYTES, perm, pi)) {
            continue;
        }
#endif

        controlbitsfrompermutation(skp, pi, GFBITS, 1 << GFBITS);
        skp += COND_BYTES;

        // storing the random string s
        rp -= SYS_N / 8;
        memcpy(skp, rp, SYS_N / 8);

        // storing positions of the 32 pivots

        store8(sk + 32, 0xFFFFFFFF);

        break;
    }

    PopArena(globalArena,mark);
    return 0;
}
