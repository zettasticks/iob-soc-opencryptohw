#pragma once

#include <stdint.h>
#include <string.h>

typedef struct{
  char* str;
  int size;
} String;

#define STRING(str) (String){str,strlen(str)}

typedef struct {
  int initTime;
  int tests;
  int goodTests;
  int versatTimeAccum;
  int softwareTimeAccum;
  int earlyExit;
} TestState;

// Functions needed by crypto side but implemented elsewhere
// Someone must implement GetTime so that we can time the algorithms relative performance
int GetTime();

// Misc Functions used by tests to parse KAT file 
char* SearchAndAdvance(char* ptr,String str);
int ParseNumber(char* ptr);

void InitializeCryptoSide(int versatAddress);

// Returns zero if pass, nonzero otherwise
int VersatSHASimulationTests();
int VersatAESSimulationTests();

int VersatSHATests();
int VersatAESTests();
int VersatMcElieceTests();

TestState VersatCommonSHATests(String content);
TestState VersatCommonAESTests(String content);
