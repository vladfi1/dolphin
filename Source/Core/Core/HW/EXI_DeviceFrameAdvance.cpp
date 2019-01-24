// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI_DeviceFrameAdvance.h"

#include<iostream>

CEXIFrameAdvance::CEXIFrameAdvance() {}
CEXIFrameAdvance::~CEXIFrameAdvance() {}

void CEXIFrameAdvance::DMAWrite(u32 address, u32 size) {
  std::cout << "DMAWrite(" << address << ", " << size << ")\n";
}

void CEXIFrameAdvance::DMARead(u32 address, u32 size) {
  std::cout << "DMARead(" << address << ", " << size << ")\n";
}

bool CEXIFrameAdvance::IsPresent() const {return true;}
