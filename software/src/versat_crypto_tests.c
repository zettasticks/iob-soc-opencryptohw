#include "versat_crypto_tests.h"

#include <stdbool.h>

#include "printf.h"
#include "iob-uart.h"

// McEliece
#include "api.h"
#include "arena.h"
void nist_kat_init(unsigned char *entropy_input, unsigned char *personalization_string, int security_strength);
int HexStringToHex(char* buffer,const char* str);
int VersatMcEliece(unsigned char *pk,unsigned char *sk);
char* GetHexadecimal(const char* text,char* buffer,int str_size);
void AES_ECB256(const uint8_t* key,const uint8_t* plaintext,uint8_t* result);

// TODO: Move this to a platform file layer or something.
String PushFileFromEthernet(const char* filepath){
  char* start = PushArray(globalArena,0,char);
  uint32_t file_size = uart_recvfile((char*) filepath,start);
  char* testFile = PushArray(globalArena,file_size + 1,char);
  testFile[file_size] = '\0';

  return (String){.str=testFile,.size=file_size};
}

int VersatSHATests(){
  int mark = MarkArena(globalArena);
  String content = PushFileFromEthernet("../../software/KAT/SHA256ShortMsg.rsp");

  TestState result = VersatCommonSHATests(content);

  printf("Init versat SHA took: %d\n",result.initTime);

  if(result.earlyExit){
    printf("SHA early exit. Check testcases to see if they follow the expected format\n");
    return 1;
  }

  printf("\n\n=======================================================\n");
  printf("SHA tests: %d passed out of %d\n\n",result.goodTests,result.tests);
  printf("  Average cycles (only counting passing tests) (not seconds)\n");
  printf("    Versat: %-7d\n",result.versatTimeAccum / result.goodTests);
  printf("  Software: %-7d\n",result.softwareTimeAccum / result.goodTests);
  printf("=======================================================\n\n");

  PopArena(globalArena,mark);
  return (result.goodTests == result.tests) ? 0 : 1;
}

int VersatAESTests(){
  int mark = MarkArena(globalArena);
  String content = PushFileFromEthernet("../../software/KAT/AESECB256.rsp");

  TestState result = VersatCommonAESTests(content);

  printf("Init versat AES took: %d\n",result.initTime);

  if(result.earlyExit){
    printf("AES early exit. Check testcases to see if they follow the expected format\n");
    return 1;
  }

  printf("\n\n=======================================================\n");
  printf("AES tests: %d passed out of %d\n\n",result.goodTests,result.tests);
  printf("  Average cycles (only counting passing tests)\n");
  printf("    Versat: %-7d\n",result.versatTimeAccum / result.goodTests);
  printf("  Software: %-7d\n",result.softwareTimeAccum / result.goodTests);
  printf("=======================================================\n\n");

  PopArena(globalArena,mark);
  return (result.goodTests == result.tests) ? 0 : 1;
}

int VersatMcElieceTests(){
  int mark = MarkArena(globalArena);

  unsigned char* public_key = PushArray(globalArena,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_PUBLICKEYBYTES,unsigned char);
  unsigned char* secret_key = PushArray(globalArena,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_SECRETKEYBYTES,unsigned char);

  int versatTimeAccum = 0;

  String content = PushFileFromEthernet("../../software/KAT/McElieceRound4kat_kem_short.rsp");
  printf("Content size: %d\n",content.size);

  char* ptr = content.str;
  int goodTests = 0;
  int tests = 0;
  while(1){
    int testMark = MarkArena(globalArena);

    ptr = SearchAndAdvance(ptr,STRING("COUNT = "));
    if(ptr == NULL){
      break;
    }

    int count = ParseNumber(ptr);

    ptr = SearchAndAdvance(ptr,STRING("SEED = "));
    if(ptr == NULL){
      printf("McEliece early exit 1. Something wrong with testfile\n");
      break;
    }

    unsigned char seed[49];
    HexStringToHex(seed,ptr);

    ptr = SearchAndAdvance(ptr,STRING("PK = "));
    if(ptr == NULL){
      printf("McEliece early exit 2. Something wrong with testfile\n");
      break;
    }
  
    char* good_pk = ptr;

    ptr = SearchAndAdvance(ptr,STRING("PKL = "));
    if(ptr == NULL){
      printf("McEliece early exit 3. Something wrong with testfile\n");
      break;
    }

    char* good_pkl = ptr;

    ptr = SearchAndAdvance(ptr,STRING("SK = "));
    if(ptr == NULL){
      printf("McEliece early exit 4. Something wrong with testfile\n");
      break;
    }

    char* good_sk = ptr;

    ptr = SearchAndAdvance(ptr,STRING("SKL = "));
    if(ptr == NULL){
      printf("McEliece early exit 5. Something wrong with testfile\n");
      break;
    }

    char* good_skl = ptr;

    nist_kat_init(seed, NULL, 256);

    printf("Before\n");

    int start = GetTime();
    VersatMcEliece(public_key, secret_key);
    int end = GetTime();

    printf("After\n");
    // Software only implementation is slow and we are already comparing to KAT anyway and so, for McEliece, we skipping software implementation test of McEliece.
    //PQCLEAN_MCELIECE348864_CLEAN_crypto_kem_keypair(public_key, secret_key);

    unsigned char* public_key_hex = PushArray(globalArena,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_PUBLICKEYBYTES * 2 + 1,char);
    unsigned char* secret_key_hex = PushArray(globalArena,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_SECRETKEYBYTES * 2 + 1,char);

    GetHexadecimal(public_key,public_key_hex,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_PUBLICKEYBYTES);
    GetHexadecimal(secret_key,secret_key_hex,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_SECRETKEYBYTES);

    bool good = true;
    for(int i = 0; i < 1024; i++){
      if(public_key_hex[i] != good_pk[i]){
        good = false;
        break;
      }
      if(public_key_hex[i + PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_PUBLICKEYBYTES * 2 - 1024] != good_pkl[i]){
        good = false;
        break;
      }
    }
    for(int i = 0; i < 1024; i++){
      if(secret_key_hex[i] != good_sk[i]){
        good = false;
        break;
      }
      if(secret_key_hex[i + PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_SECRETKEYBYTES * 2 - 1024] != good_skl[i]){
        good = false;
        break;
      }
    }

    if(good){
      versatTimeAccum += end - start;
      goodTests += 1;
    } else {
      printf("McEliece Test %02d: Error\n",tests);
      printf("  Expected Public      (first 32 chars): %.32s\n",good_pk); 
      printf("  Expected Public Last (first 32 chars): %.32s\n",good_pkl); 
      printf("  Expected Secret      (first 32 chars): %.32s\n",good_sk);  
      printf("  Expected Secret Last (first 32 chars): %.32s\n",good_skl); 
      printf("  Got Public      (first 32 chars):      %.32s\n",public_key_hex);
      printf("  Got Public Last (first 32 chars):      %.32s\n",&public_key_hex[PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_PUBLICKEYBYTES * 2 - 1024]);
      printf("  Got Secret      (first 32 chars):      %.32s\n",secret_key_hex);
      printf("  Got Secret Last (first 32 chars):      %.32s\n",&secret_key_hex[PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_SECRETKEYBYTES * 2 - 1024]);
    }

    tests += 1;
    PopArena(globalArena,testMark);
  }
  printf("\n\n=======================================================\n");
  printf("McEliece tests: %d passed out of %d\n",goodTests,tests);
  printf("  No time taken since software implementation is really\n");
  printf("  slow, so we would just be wasting time. We are already\n");
  printf("  comparing solutions to a KAT.\n");
  printf("=======================================================\n\n");
  PopArena(globalArena,mark);

  return (goodTests == tests) ? 0 : 1;
}
