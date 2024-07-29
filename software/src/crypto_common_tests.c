#include "crypto_tests.h"

#include "stdbool.h"

#include "printf.h"

#include "versat_accel.h"
#include "versat_crypto.h"
#include "crypto/aes.h"
#include "crypto/sha2.h"

#include "arena.h"
#include "string.h"

void versat_init(int);
void AES_ECB256(uint8_t* key,uint8_t* data,uint8_t* encrypted);

void InitializeCryptoSide(int versatAddress){
  versat_init(versatAddress);
  ConfigEnableDMA(true); // No problem using DMA in embedded.
}

static char* SearchAndAdvance(char* ptr,String str){
  char* firstChar = strstr(ptr,str.str);
  if(firstChar == NULL){
    return NULL;
  }

  char* advance = firstChar + str.size;
  return advance;
}

static int ParseNumber(char* ptr){
  int count = 0;

  while(ptr != NULL){
    char ch = *ptr;

    if(ch >= '0' && ch <= '9'){
      count *= 10;
      count += ch - '0';
      ptr += 1;
      continue;
    }

    break;
  }

  return count;
}

// Parses content and performs each test found
TestState VersatCommonSHATests(String content){
  TestState result = {};

  int mark = MarkArena(globalArena);

  int start = GetTime();
  InitVersatSHA();
  int end = GetTime();

  result.initTime = end - start;

  static const int HASH_SIZE = (256/8);

  char* ptr = content.str;
  while(1){
    int testMark = MarkArena(globalArena);

    ptr = SearchAndAdvance(ptr,STRING("LEN = "));
    if(ptr == NULL){
      break;
    }

    int len = ParseNumber(ptr);

    ptr = SearchAndAdvance(ptr,STRING("MSG = "));
    if(ptr == NULL){ // Note: It's only a error if any check after the first one fails, because we are assuming that if the first passes then that must mean that the rest should pass as well.
      result.earlyExit = 1;
      break;
    }

    unsigned char* message = PushArray(globalArena,len,unsigned char);
    int bytes = HexStringToHex(message,ptr);

    ptr = SearchAndAdvance(ptr,STRING("MD = "));
    if(ptr == NULL){
      result.earlyExit = 1;
      break;
    }

    char* expected = ptr;

    unsigned char versat_digest[32];
    unsigned char software_digest[32];
    for(int i = 0; i < 32; i++){
      versat_digest[i] = 0;
      software_digest[i] = 0;
    }
  
    int start = GetTime();
    VersatSHA(versat_digest,message,len / 8);
    int middle = GetTime();
    sha256(software_digest,message,len / 8);
    int end = GetTime();

    bool good = true;
    for(int i = 0; i < 32; i++){
      if(versat_digest[i] != software_digest[i]){
        good = false;
        break;
      }
    }

    if(good){
      result.versatTimeAccum += middle - start;
      result.softwareTimeAccum += end - middle;
      result.goodTests += 1;
    } else {
      char versat_buffer[256];
      char software_buffer[256];
      GetHexadecimal((char*) versat_digest,versat_buffer, HASH_SIZE);
      GetHexadecimal((char*) software_digest,software_buffer, HASH_SIZE);

      printf("SHA Test %02d: Error\n",result.tests);
      printf("  Expected: %.64s\n",expected); 
      printf("  Software: %s\n",software_buffer);
      printf("  Versat:   %s\n",versat_buffer);
    }

    result.tests += 1;
    PopArena(globalArena,testMark);
  }

  PopArena(globalArena,mark);

  return result;
}

// Parses content and performs each test found
TestState VersatCommonAESTests(String content){
  TestState result = {};

  int mark = MarkArena(globalArena);

  int start = GetTime();
  InitVersatAES();
  InitAES();
  int end = GetTime();

  result.initTime = end - start;

  char* ptr = content.str;
  while(1){
    int testMark = MarkArena(globalArena);

    ptr = SearchAndAdvance(ptr,STRING("COUNT = "));
    if(ptr == NULL){
      break;
    }

    int count = ParseNumber(ptr);

    ptr = SearchAndAdvance(ptr,STRING("KEY = "));
    if(ptr == NULL){
      result.earlyExit = 1;
      break;
    }

    unsigned char* key = PushArray(globalArena,32 + 1,unsigned char);
    HexStringToHex(key,ptr);

    ptr = SearchAndAdvance(ptr,STRING("PLAINTEXT = "));
    if(ptr == NULL){
      result.earlyExit = 1;
      break;
    }
  
    unsigned char* plain = PushArray(globalArena,16 + 1,unsigned char);
    HexStringToHex(plain,ptr);

    ptr = SearchAndAdvance(ptr,STRING("CIPHERTEXT = "));
    if(ptr == NULL){
      result.earlyExit = 1;
      break;
    }

    char* cypher = ptr;

    uint8_t versat_result[AES_BLK_SIZE] = {};
    uint8_t software_result[AES_BLK_SIZE] = {};

    int start = GetTime();
    AES_ECB256(key,plain,versat_result);
    int middle = GetTime();
    
    struct AES_ctx ctx;
    AES_init_ctx(&ctx,key);
    memcpy(software_result,plain,AES_BLK_SIZE);
    AES_ECB_encrypt(&ctx,software_result);
    int end = GetTime();

    bool good = true;
    for(int i = 0; i < AES_BLK_SIZE; i++){
      if(versat_result[i] != software_result[i]){
        good = false;
        break;
      }
    }

    if(good){
      result.versatTimeAccum += middle - start;
      result.softwareTimeAccum += end - middle;
      result.goodTests += 1;
    } else {
      char versat_buffer[256];
      char software_buffer[256];
      GetHexadecimal((char*) versat_result,versat_buffer, AES_BLK_SIZE);
      GetHexadecimal((char*) software_result,software_buffer, AES_BLK_SIZE);

      printf("AES Test %02d: Error\n",result.tests);
      printf("  Expected: %.32s\n",cypher); 
      printf("  Software: %s\n",software_buffer);
      printf("  Versat:   %s\n",versat_buffer);
    }

    result.tests += 1;
    PopArena(globalArena,testMark);
  }

  PopArena(globalArena,mark);

  return result;
}

static char GetHexadecimalChar(unsigned char value){
  if(value < 10){
    return '0' + value;
  } else{
    return 'A' + (value - 10);
  }
}

char* GetHexadecimal(const char* text,char* buffer,int str_size){
  int i = 0;
  unsigned char* view = (unsigned char*) text;
  for(; i< str_size; i++){
    buffer[i*2] = GetHexadecimalChar(view[i] / 16);
    buffer[i*2+1] = GetHexadecimalChar(view[i] % 16);
  }

  buffer[i*2] = '\0';

  return buffer;
}

static char HexToInt(char ch){
   if('0' <= ch && ch <= '9'){
      return (ch - '0');
   } else if('a' <= ch && ch <= 'f'){
      return ch - 'a' + 10;
   } else if('A' <= ch && ch <= 'F'){
      return ch - 'A' + 10;
   } else {
      return 0x7f;
   }
}

// Make sure that buffer is capable of storing the whole thing. Returns number of bytes inserted
int HexStringToHex(char* buffer,const char* str){
   int inserted = 0;
   for(int i = 0; ; i += 2){
      char upper = HexToInt(str[i]);
      char lower = HexToInt(str[i+1]);

      if(upper >= 16 || lower >= 16){
         if(upper < 16){ // Upper is good but lower is not
            printf("Warning: HexString was not divisible by 2\n");
         }
         break;
      }

      buffer[inserted++] = upper * 16 + lower;
   }

   return inserted;
}
