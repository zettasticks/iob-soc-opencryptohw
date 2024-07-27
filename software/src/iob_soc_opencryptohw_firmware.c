#include <stdlib.h>

#include "bsp.h"
#include "iob-timer.h"
#include "iob-uart.h"
#include "iob_soc_opencryptohw_conf.h"
#include "iob_soc_opencryptohw_periphs.h"
#include "iob_soc_opencryptohw_system.h"
#include "printf.h"
#include <string.h>

#include "arena.h"
#include "crypto_tests.h"

void clear_cache(){
}

// Send signal by uart to receive file by ethernet
uint32_t uart_recvfile_ethernet(const char *file_name) {

  uart_puts(UART_PROGNAME);
  uart_puts(": requesting to receive file by ethernet\n");

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
  globalArenaInst.allocated = Mega(1);
  globalArenaInst.ptr = malloc(globalArenaInst.allocated);
  globalArena = &globalArenaInst;

  uart_puts("\n\n\nHello world!\n\n\n");

  InitializeCryptoSide(VERSAT0_BASE);

  uart_puts("\n\n\nInitialized crypto side\n\n\n");

  // Tests are too big and slow to perform during simulation.
#ifndef SIMULATION
  uart_puts("\n\n\nPC tests\n\n\n");
  test_result |= VersatSHATests();
  test_result |= VersatAESTests();
  //test_result |= VersatMcElieceTests();
#else
  uart_puts("\n\n\nSim tests\n\n\n");
  test_result |= VersatSHASimulationTests();
  test_result |= VersatAESSimulationTests();
#endif

  uart_finish();
}
