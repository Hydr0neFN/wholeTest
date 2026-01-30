/*
 * UI Library Header - Forces PlatformIO LDF to include lib/ui
 * Include this from display_test.cpp to trigger library compilation
 */

#ifndef UI_LIB_H
#define UI_LIB_H

// This header forces PlatformIO's Library Dependency Finder (LDF)
// to recognize and compile the UI library in lib/ui/

// The actual UI declarations are in the include/ directory:
#include "display.h"  // C++ header - includes LovyanGFX
#include "ui.h"       // C header with extern "C" guards

#endif
