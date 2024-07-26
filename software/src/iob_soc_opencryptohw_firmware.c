#include <stdlib.h>

#include "bsp.h"
#include "iob-timer.h"
#include "iob-uart.h"
//#include "iob-eth.h"
#include "iob_soc_opencryptohw_conf.h"
#include "iob_soc_opencryptohw_periphs.h"
#include "iob_soc_opencryptohw_system.h"
#include "printf.h"
#include <string.h>

#include "arena.h"
#include "versat_crypto_tests.h"

void nist_kat_init(unsigned char *entropy_input, unsigned char *personalization_string, int security_strength);
int VersatMcEliece(unsigned char *pk,unsigned char *sk);

void clear_cache(){
  for (unsigned int i = 0; i < 10; i++)
    asm volatile("nop");

  int size = 1024 * 32;
  char* m = (char*) malloc(size); // Should not use malloc but some random fixed ptr in embedded. No use calling malloc since we can always read at any point in memory without worrying about memory protection.

  // volatile and asm are used to make sure that gcc does not optimize away this loop that appears to do nothing
  volatile int val = 0;
  for(int i = 0; i < size; i += 32){
    val += m[i];
    __asm__ volatile("" : "+g" (val) : :);
  }
  free(m);
}

// Send signal by uart to receive file by ethernet
uint32_t uart_recvfile_ethernet(const char *file_name) {

  uart_puts(UART_PROGNAME);
  uart_puts(": requesting to receive file by ethernet\n");

  uart_puts("Before\n");
  // send file receive by ethernet request
  uart_putc(0x13);

  // send file name (including end of string)
  uart_puts(file_name);
  uart_putc(0);

  // receive file size
  uint32_t file_size = uart_getc();
  file_size |= ((uint32_t)uart_getc()) << 8;
  file_size |= ((uint32_t)uart_getc()) << 16;
  file_size |= ((uint32_t)uart_getc()) << 24;

  // send ACK before receiving file
  uart_putc(ACK);

  return file_size;
}

int GetTime(){
  return (int) timer_get_count();
}

static int intStatic;

#include "api.h"

#define Kilo(VAL) (1024 * (VAL))
#define Mega(VAL) (1024 * Kilo(VAL))

int main() {
  int test_result = 0;
  // init timer
  timer_init(TIMER0_BASE);

  // init uart
  uart_init(UART0_BASE, FREQ / BAUD);
  printf_init(&uart_putc);

  Arena globalArenaInst = {};
  globalArenaInst.ptr = malloc(Mega(1));
  globalArenaInst.allocated = Mega(1);
  globalArena = &globalArenaInst;

#if 0
  Arena globalArenaInst = InitArena(8*1024*1024); // 8 megabytes should suffice. Arena memory used by crypto algorithms, both by software and Versat impl.
  globalArenaInst.ptr += (1024 * 1024);
  globalArenaInst.allocated -= (1024 * 1024);
  globalArena = &globalArenaInst;
#endif

  // init eth
  // eth_init(ETH0_BASE, &clear_cache);
  // eth_wait_phy_rst();

  printf("%p\n",&intStatic);
  printf("%p\n",&test_result);
  // test puts
  uart_puts("\n\n\nHello world!\n\n\n");

  printf("Using printf\n");

  InitializeCryptoSide(VERSAT0_BASE);

  uart_puts("\n\n\nInitialized crypto side\n\n\n");

  // Tests are too big and slow to perform during simulation.
  // Comment out the source files in sw_build.mk to also reduce binary size and speedup simulation.
//#ifndef SIMULATION
  uart_puts("\n\n\nPC tests\n\n\n");

   unsigned char seed[49] = {};

   for(int i = 0; i < 48; i++){
      seed[i] = 0;
   }

   seed[47] = 8;

  unsigned char* public_key = PushAndZeroArray(globalArena,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_PUBLICKEYBYTES + 1,unsigned char);
  unsigned char* secret_key = PushAndZeroArray(globalArena,PQCLEAN_MCELIECE348864_CLEAN_CRYPTO_SECRETKEYBYTES + 1,unsigned char);

   printf("2\n");
   nist_kat_init(seed, NULL, 256);   

   printf("3\n");

   VersatMcEliece(public_key, secret_key);

   printf("%.32s\n",public_key);
   printf("%.32s\n",secret_key);

  
  //VersatMcEliece(public_key, secret_key);

  //test_result |= VersatSHATests();
  //test_result |= VersatAESTests();
  //test_result |= VersatMcElieceTests();

  //void TestMcEliece();
  //TestMcEliece();
//#else
//  uart_puts("\n\n\nSim tests\n\n\n");
//  test_result |= VersatSimpleSHATests();
//  test_result |= VersatSimpleAESTests();
//#endif

  // read current timer count, compute elapsed time
  unsigned long long elapsed = timer_get_count();
  unsigned int elapsedu = elapsed / (FREQ / 1000000);

  uart_finish();
}
