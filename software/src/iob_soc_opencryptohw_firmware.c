#include "bsp.h"
#include "iob-timer.h"
#include "iob-uart.h"
//#include "iob-eth.h"
#include "iob_soc_opencryptohw_conf.h"
#include "iob_soc_opencryptohw_periphs.h"
#include "iob_soc_opencryptohw_system.h"
#include "printf.h"
#include <string.h>

#include "versat_crypto_tests.h"

void clear_cache(){
  for (unsigned int i = 0; i < 10; i++)
    asm volatile("nop");  
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

int main() {
  int test_result = 0;

  // init timer
  timer_init(TIMER0_BASE);

  // init uart
  uart_init(UART0_BASE, FREQ / BAUD);
  printf_init(&uart_putc);

  // init eth
  // eth_init(ETH0_BASE, &clear_cache);
  // eth_wait_phy_rst();

  // test puts
  uart_puts("\n\n\nHello world!\n\n\n");

  printf("Using printf\n");

  InitializeCryptoSide(VERSAT0_BASE);

  uart_puts("\n\n\nInitialized crypto side\n\n\n");

  // Tests are too big and slow to perform during simulation.
  // Comment out the source files in sw_build.mk to also reduce binary size and speedup simulation.
//#ifndef SIMULATION
  uart_puts("\n\n\nPC tests\n\n\n");
  test_result |= VersatSHATests();
  test_result |= VersatAESTests();
  test_result |= VersatMcElieceTests();
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
