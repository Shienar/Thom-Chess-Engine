#include "debug.h"

int printDebugMessages = 0;

void enableDebugMessages() {printDebugMessages = 1;}
void disableDebugMessages() {printDebugMessages = 0;}