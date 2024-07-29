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
  // No need to clear cache for pico since cache is write-through
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

  // Allocates an arena, basically a stack allocator where we can push and pop memory on command.
  Arena globalArenaInst = {};
  globalArenaInst.allocated = Mega(1); // We need at least half a megabyte for McEliece
  globalArenaInst.ptr = malloc(globalArenaInst.allocated);
  globalArena = &globalArenaInst;

  uart_puts("\n\n\nHello world!\n\n\n");

  // Initializes Versat
  InitializeCryptoSide(VERSAT0_BASE);

  uart_puts("\n\n\nInitialized crypto side\n\n\n");

  // Tests are too big and slow to perform during simulation.
#ifndef SIMULATION
  uart_puts("\n\n\nPC tests\n\n\n");
  test_result |= VersatSHATests();
  test_result |= VersatAESTests();
  test_result |= VersatMcElieceTests();
#else
  uart_puts("\n\n\nSim tests\n\n\n");
  test_result |= VersatSHASimulationTests();
  test_result |= VersatAESSimulationTests();
#endif

  uart_finish();
}
