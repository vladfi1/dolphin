// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HLE/HLE_Misc.h"

#include <cstdio>
#include <cstdlib>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/HW/CPU.h"
#include "Core/Host.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace HLE_Misc
{
// HookType::Start trace hook on HSD_Randi (GALE01 0x80380580). Logs the RNG seed BEFORE the draw,
// the requested range (r3), and the caller LR. HookType::Start runs this hook and THEN the original
// HSD_Randi, so the game RNG stream is unaffected. Gated by the MSL_RNG_TRACE env var so it is
// fully inert for normal runs and ordinary engine-dump probes. HSD_Rand/HSD_Randf draws are not
// hooked but are recoverable as seed gaps between consecutive logged Randi calls (the LCG advances
// one step/draw). Used only by the Needle post-hit RNG forensics; trace lines go to stderr.
void HLE_HSD_RandiTrace(const Core::CPUThreadGuard& guard)
{
  static const bool s_enabled = std::getenv("MSL_RNG_TRACE") != nullptr;
  if (!s_enabled)
    return;
  auto& ppc_state = guard.GetSystem().GetPPCState();
  const u32 seed_before = PowerPC::MMU::HostRead_U32(guard, 0x804D5F90);
  const u32 max_val = ppc_state.gpr[3];
  const u32 caller = LR(ppc_state);
  std::fprintf(stderr, "RNGTRACE seed=%08x max=%u lr=%08x\n", seed_before, max_val, caller);
}

// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  auto& ppc_state = system.GetPPCState();
  ppc_state.npc = LR(ppc_state);
}

void HBReload(const Core::CPUThreadGuard& guard)
{
  // There isn't much we can do. Just stop cleanly.
  auto& system = guard.GetSystem();
  system.GetCPU().Break();
  Host_Message(HostMessageID::WMUserStop);
}

void GeckoCodeHandlerICacheFlush(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  auto& ppc_state = system.GetPPCState();
  auto& jit_interface = system.GetJitInterface();

  // Work around the codehandler not properly invalidating the icache, but
  // only the first few frames.
  // (Project M uses a conditional to only apply patches after something has
  // been read into memory, or such, so we do the first 5 frames.  More
  // robust alternative would be to actually detect memory writes, but that
  // would be even uglier.)
  u32 gch_gameid = PowerPC::MMU::HostRead_U32(guard, Gecko::INSTALLER_BASE_ADDRESS);
  if (gch_gameid - Gecko::MAGIC_GAMEID == 5)
  {
    return;
  }
  else if (gch_gameid - Gecko::MAGIC_GAMEID > 5)
  {
    gch_gameid = Gecko::MAGIC_GAMEID;
  }
  PowerPC::MMU::HostWrite_U32(guard, gch_gameid + 1, Gecko::INSTALLER_BASE_ADDRESS);

  ppc_state.iCache.Reset(jit_interface);
}

// Because Dolphin messes around with the CPU state instead of patching the game binary, we
// need a way to branch into the GCH from an arbitrary PC address. Branching is easy, returning
// back is the hard part. This HLE function acts as a trampoline that restores the original LR, SP,
// and PC before the magic, invisible BL instruction happened.
void GeckoReturnTrampoline(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  auto& ppc_state = system.GetPPCState();

  // Stack frame is built in GeckoCode.cpp, Gecko::RunCodeHandler.
  const u32 SP = ppc_state.gpr[1];
  ppc_state.gpr[1] = PowerPC::MMU::HostRead_U32(guard, SP + 8);
  ppc_state.npc = PowerPC::MMU::HostRead_U32(guard, SP + 12);
  LR(ppc_state) = PowerPC::MMU::HostRead_U32(guard, SP + 16);
  ppc_state.cr.Set(PowerPC::MMU::HostRead_U32(guard, SP + 20));
  for (int i = 0; i < 14; ++i)
  {
    ppc_state.ps[i].SetBoth(PowerPC::MMU::HostRead_U64(guard, SP + 24 + 2 * i * sizeof(u64)),
                            PowerPC::MMU::HostRead_U64(guard, SP + 24 + (2 * i + 1) * sizeof(u64)));
  }
}
}  // namespace HLE_Misc
