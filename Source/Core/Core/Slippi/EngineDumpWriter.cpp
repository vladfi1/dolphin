#include "Core/Slippi/EngineDumpWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>

#include "Common/FileUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/System.h"

namespace {
constexpr u8 ENGINE_DUMP_MAGIC[8] = {'M', 'S', 'I', 'M', 'D', 'M', 'P', 0};
constexpr u32 ENGINE_DUMP_ENDIAN_TAG = 0x01020304;
constexpr u32 ENGINE_DUMP_VERSION = 12;

constexpr u32 R13_BASE = 0x804DB6A0;
constexpr u32 FRAME_INDEX_PTR = R13_BASE - 0x49AC;
constexpr u32 PLAYER_SLOTS = 0x80453080;
constexpr u32 STATIC_PLAYER_SIZE = 0xE90;
constexpr u32 GOBJ_USER_DATA_OFF = 0x2C;
constexpr u32 FIGHTER_POS_X_OFF = 0xB0;
constexpr u32 FIGHTER_POS_Y_OFF = 0xB4;
constexpr u32 FIGHTER_POS_Z_OFF = 0xB8;
constexpr u32 FIGHTER_SELF_VEL_X_OFF = 0x80;
constexpr u32 FIGHTER_SELF_VEL_Y_OFF = 0x84;
constexpr u32 FIGHTER_KB_VEL_OFF = 0x8C;
constexpr u32 FIGHTER_ATK_SHIELD_KB_OFF = 0x98;
constexpr u32 FIGHTER_XA4_UNK_VEL_OFF = 0xA4;
constexpr u32 FIGHTER_GR_VEL_OFF = 0xEC;
constexpr u32 FIGHTER_XD4_UNK_VEL_OFF = 0xD4;
constexpr u32 FIGHTER_PLAYER_NUDGE_OFF = 0xF8;
constexpr u32 FIGHTER_X100_OFF = 0x100;
constexpr u32 FIGHTER_ACTION_OFF = 0x10;
constexpr u32 FIGHTER_ANIM_OFF = 0x14;
constexpr u32 FIGHTER_ACTION_FRAME_OFF = 0x894;
constexpr u32 FIGHTER_BLEND_FRAMES_OFF = 0x8A4;
constexpr u32 FIGHTER_ANIM_FRAME_OFF = 0x8A8;
constexpr u32 FIGHTER_X8B0_OFF = 0x8B0;
constexpr u32 FIGHTER_X8B0_STRIDE = 0x14;
constexpr u32 FIGHTER_FT_DATA_PTR_OFF = 0x10C;
constexpr u32 FTDATA_X20_PTR_OFF = 0x20;
constexpr u32 FTDATA_X20_JOINT_OFF = 0x04;
constexpr u32 FIGHTER_GROUND_OR_AIR_OFF = 0xE0;
constexpr u32 FIGHTER_FACING_OFF = 0x2C;
constexpr u32 FIGHTER_PERCENT_OFF = 0x1830;
constexpr u32 FIGHTER_TEAM_OFF = 0x61B;
constexpr u32 FIGHTER_COSTUME_OFF = 0x619;
constexpr u32 FIGHTER_STATE_FLAGS_2218_OFF = 0x2218;
constexpr u32 FIGHTER_STATE_FLAGS_221A_OFF = 0x221A;
constexpr u32 FIGHTER_STATE_FLAGS_221B_OFF = 0x221B;
constexpr u32 FIGHTER_STATE_FLAGS_221C_OFF = 0x221C;
constexpr u32 FIGHTER_STATE_FLAGS_221F_OFF = 0x221F;
constexpr u32 FIGHTER_PARTS_PTR_OFF = 0x5E8;
constexpr u32 FIGHTER_BONE_STRIDE = 0x10;
constexpr u32 FIGHTER_BONE_JOBJ_PTR_OFF = 0x00;

constexpr u32 HSD_JOBJ_FLAGS_OFF = 0x14;
constexpr u32 HSD_JOBJ_ROTATE_X_OFF = 0x1C;
constexpr u32 HSD_JOBJ_ROTATE_Y_OFF = 0x20;
constexpr u32 HSD_JOBJ_ROTATE_Z_OFF = 0x24;
constexpr u32 HSD_JOBJ_ROTATE_W_OFF = 0x28;
constexpr u32 HSD_JOBJ_MTX_OFF = 0x44;
constexpr u32 FIGHTER_HITLAG_OFF = 0x195C;
constexpr u32 FIGHTER_DMG_X1948_OFF = 0x1948;
constexpr u32 FIGHTER_DMG_X194C_OFF = 0x194C;
constexpr u32 FIGHTER_MISC_AS_OFF = 0x2340;
constexpr u32 FIGHTER_X2222_FLAGS_OFF = 0x2222;
constexpr u32 FIGHTER_THROW_FLAGS_OFF = 0x2210;
constexpr u32 FIGHTER_TRANSN_POS_OFF = 0x68C;
constexpr u32 FIGHTER_TRANSN_OFFSET_OFF = 0x6A4;
constexpr u32 FIGHTER_COMBO_COUNT_OFF = 0x2090;
constexpr u32 FIGHTER_COMBO_PUSH_TIMER_OFF = 0x2092;
constexpr u32 FIGHTER_SHIELD_HEALTH_OFF = 0x1998;
constexpr u32 FIGHTER_HURTBOX_COLLISION1_OFF = 0x1988;
constexpr u32 FIGHTER_HURTBOX_COLLISION2_OFF = 0x198C;
constexpr u32 FIGHTER_ECB_TOP_OFF = 0x794;
constexpr u32 FIGHTER_ECB_BOTTOM_OFF = 0x79C;
constexpr u32 FIGHTER_ECB_RIGHT_OFF = 0x7A4;
constexpr u32 FIGHTER_ECB_LEFT_OFF = 0x7AC;
constexpr u32 FIGHTER_DESIRED_ECB_TOP_OFF = 0x774;
constexpr u32 FIGHTER_PREV_ECB_TOP_OFF = 0x7B4;
constexpr u32 FIGHTER_ECB_SOURCE_OFF = 0x7F4;
constexpr u32 FIGHTER_ECB_SOURCE_X128_OFF = FIGHTER_ECB_SOURCE_OFF + 0x28;
constexpr u32 FIGHTER_ECB_SOURCE_X12C_OFF = FIGHTER_ECB_SOURCE_OFF + 0x2C;
constexpr u32 FIGHTER_COLL_X130_FLAGS_OFF = 0x820;
constexpr u32 FIGHTER_ECB_LOCK_TIMER_OFF = 0x88C;
constexpr u32 FIGHTER_FLOOR_NORMAL_X_OFF = 0x844;
constexpr u32 FIGHTER_FLOOR_NORMAL_Y_OFF = 0x848;
constexpr u32 FIGHTER_GROUND_ACCEL_1_OFF = 0xE4;
constexpr u32 FIGHTER_GROUND_ACCEL_2_OFF = 0xE8;
constexpr u32 FIGHTER_ANIM_VEL_X_OFF = 0x74;
constexpr u32 FIGHTER_ANIM_VEL_Y_OFF = 0x78;
constexpr u32 FIGHTER_HITBOX_BASE_OFF = 0x914;
constexpr u32 HITBOX_STRIDE = 0x138;
constexpr u32 HITBOX_VICTIMS1_CURSOR_OFF = 0x44;
constexpr u32 HITBOX_VICTIMS2_CURSOR_OFF = 0x45;
constexpr u32 HITBOX_VICTIMS1_BASE_OFF = 0x74;
constexpr u32 HITBOX_VICTIMS2_BASE_OFF = 0xD4;
constexpr u32 HITBOX_VICTIM_STRIDE = 0x8;
constexpr u32 HITBOX_OWNER_OFF = 0x134;
constexpr u32 FIGHTER_HURTBOX_BASE_OFF = 0x11A0;
constexpr u32 HURTBOX_STRIDE = 0x4C;
constexpr u32 ITEM_MANAGER_PTR = R13_BASE - 0x3E74;
constexpr u32 ITEM_MANAGER_FIRST_GOBJ_OFF = 0x24;
constexpr u32 GOBJ_NEXT_OFF = 0x08;
constexpr u32 ITEM_DATA_OFF = 0x2C;
constexpr u32 ITEM_KIND_OFF = 0x10;
constexpr u32 ITEM_STATE_OFF = 0x24;
constexpr u32 ITEM_ANIM_ID_OFF = 0x28;
constexpr u32 ITEM_FACING_OFF = 0x2C;
constexpr u32 ITEM_POS_OFF = 0x4C;
constexpr u32 ITEM_VEL_OFF = 0x40;
constexpr u32 ITEM_ANIM_FRAME_OFF = 0x5CC;
constexpr u32 ITEM_LIFETIME_OFF = 0xD44;
constexpr u32 ITEM_HITBOX0_OFF = 0x5D4;
constexpr u32 ITEM_HITBOX_STRIDE = 0x13C;
constexpr u32 ITEM_OWNER_OFF = 0x518;
constexpr u32 ITEM_XC34_DAMAGE_DEALT_OFF = 0xC34;
constexpr u32 ITEM_XC48_CLANK_DAMAGE_OFF = 0xC48;
constexpr u32 ITEM_XC4C_REFLECT_DAMAGE_OFF = 0xC4C;
constexpr u32 ITEM_XC50_SHIELD_DAMAGE_OFF = 0xC50;
constexpr u32 ITEM_XCA8_CALLBACK_DAMAGE_OFF = 0xCA8;
constexpr u32 ITEM_XCBC_HITLAG_OFF = 0xCBC;
constexpr u32 ITEM_XCC0_HITLAG_MIN_OFF = 0xCC0;
constexpr u32 ITEM_XDA8_SHORT_OFF = 0xDA8;
constexpr u32 ITEM_XDC8_WORD_OFF = 0xDC8;
constexpr u32 ITEM_XDCE_FLAGS_OFF = 0xDCE;
constexpr u32 ITEM_FOXLASER_SCALE_OFF = 0xDD4;
constexpr u32 ITEM_FOXLASER_ANGLE_OFF = 0xDD8;
constexpr u32 ITEM_FOXLASER_SPEED_OFF = 0xDDC;
constexpr u32 ITEM_FOXLASER_POS_OFF = 0xDE0;
constexpr u32 MAX_ITEMS = 15;
constexpr u32 RNG_STATE_ADDR = 0x804D5F90;
constexpr u32 RNG_MULTIPLIER = 0x343FD;
constexpr u32 RNG_INCREMENT = 0x269EC3;
constexpr u16 FRAME_FLAG_RNG_SEED_EXISTS = 1u << 0;
constexpr u32 RNG_STEPS_UNKNOWN = 0xFFFFFFFFu;
constexpr u32 RNG_STEP_SEARCH_MAX = 8192;
constexpr u32 GAME_TIMER_ADDR = 0x8046B6C8;
constexpr u32 TEAMS_FLAG_ADDR = 0x804807C8;
constexpr u32 P1_STOCK_ADDR = 0x8045310E;
constexpr u32 P2_STOCK_ADDR = 0x80453F9E;

constexpr u32 BUTTON_A = 0x0100;
constexpr u32 BUTTON_B = 0x0200;
constexpr u32 BUTTON_X = 0x0400;
constexpr u32 BUTTON_Y = 0x0800;
constexpr u32 BUTTON_Z = 0x0010;
constexpr u32 BUTTON_L = 0x0040;
constexpr u32 BUTTON_R = 0x0020;
constexpr u32 BUTTON_START = 0x1000;
constexpr u32 BUTTON_D_UP = 0x0008;
constexpr u32 BUTTON_LR = 1u << 31;

inline u32 ReadU32(u32 addr)
{
  return Core::System::GetInstance().GetMemory().Read_U32(addr);
}

inline u8 ReadU8(u32 addr)
{
  return Core::System::GetInstance().GetMemory().Read_U8(addr);
}

inline u16 ReadU16(u32 addr)
{
  return Core::System::GetInstance().GetMemory().Read_U16(addr);
}

inline s32 ReadS32(u32 addr)
{
  return static_cast<s32>(Core::System::GetInstance().GetMemory().Read_U32(addr));
}

inline u32 FloatBits(float v)
{
  u32 bits = 0;
  std::memcpy(&bits, &v, sizeof(bits));
  return bits;
}

inline u32 AdvanceRng(u32 state)
{
  return state * RNG_MULTIPLIER + RNG_INCREMENT;
}

inline u32 RngStepsBetween(u32 start, u32 target, u32 max_steps)
{
  u32 state = start;
  if (state == target)
    return 0;
  for (u32 step = 1; step <= max_steps; step++)
  {
    state = AdvanceRng(state);
    if (state == target)
      return step;
  }
  return RNG_STEPS_UNKNOWN;
}

inline void AppendU8(std::vector<u8>& out, u8 v)
{
  out.push_back(v);
}

inline void AppendU16(std::vector<u8>& out, u16 v)
{
  out.push_back(static_cast<u8>(v & 0xFF));
  out.push_back(static_cast<u8>((v >> 8) & 0xFF));
}

inline void AppendU32(std::vector<u8>& out, u32 v)
{
  out.push_back(static_cast<u8>(v & 0xFF));
  out.push_back(static_cast<u8>((v >> 8) & 0xFF));
  out.push_back(static_cast<u8>((v >> 16) & 0xFF));
  out.push_back(static_cast<u8>((v >> 24) & 0xFF));
}

inline void AppendI32(std::vector<u8>& out, s32 v)
{
  AppendU32(out, static_cast<u32>(v));
}

inline u32 FighterGobjForPort(int port)
{
  u32 base = PLAYER_SLOTS + static_cast<u32>(port - 1) * STATIC_PLAYER_SIZE;
  u8 transformed = ReadU8(base + 0x0C);
  return ReadU32(base + 0xB0 + static_cast<u32>(transformed) * 4);
}

inline u32 FighterPtrForPort(int port)
{
  u32 gobj = FighterGobjForPort(port);
  if (!gobj)
    return 0;
  return ReadU32(gobj + GOBJ_USER_DATA_OFF);
}
} // namespace

EngineDumpWriter::EngineDumpWriter(const std::string& path, int start_frame, int end_frame)
    : m_path(path), m_start_frame(start_frame), m_end_frame(end_frame)
{
}

EngineDumpWriter::~EngineDumpWriter()
{
  Finalize();
}

void EngineDumpWriter::SetGameSettings(const Slippi::GameSettings& settings)
{
  m_stage_id = settings.stage;
  m_ports.clear();
  for (const auto& entry : settings.players)
  {
    const u32 port = static_cast<u32>(entry.first + 1);
    if (port >= 1 && port <= 4)
      m_ports.push_back(static_cast<u8>(port));
  }
  std::sort(m_ports.begin(), m_ports.end());
  if (m_ports.empty())
  {
    m_ports.push_back(1);
    m_ports.push_back(2);
  }
  m_port_count = static_cast<u8>(m_ports.size());
}

void EngineDumpWriter::CaptureFrame(s32 frame_index, Slippi::FrameData* frame)
{
  const bool debug = std::getenv("MSL_ENGINE_DUMP_DEBUG") != nullptr;
  if (m_path.empty() || frame == nullptr)
  {
    if (debug)
      std::cerr << "[ENGINE_DUMP_SKIP] path_or_frame frame=" << frame_index << "\n";
    return;
  }
  if (frame_index < m_start_frame || frame_index > m_end_frame)
  {
    if (debug)
      std::cerr << "[ENGINE_DUMP_SKIP] window frame=" << frame_index << " start=" << m_start_frame
                << " end=" << m_end_frame << "\n";
    return;
  }

  u32 fighter_ptrs[4] = {};
  const u32 active_port_count = static_cast<u32>(m_ports.size());
  for (u32 i = 0; i < active_port_count; i++)
    fighter_ptrs[i] = FighterPtrForPort(static_cast<int>(m_ports[i]));
  bool all_fighters_live = true;
  for (u32 i = 0; i < active_port_count; i++)
    all_fighters_live = all_fighters_live && fighter_ptrs[i] != 0;
  if (!all_fighters_live)
  {
    if (debug)
    {
      std::cerr << "[ENGINE_DUMP_SKIP] fighter_ptr frame=" << frame_index;
      for (u32 i = 0; i < active_port_count; i++)
        std::cerr << " p" << static_cast<int>(m_ports[i]) << "=0x" << std::hex
                  << fighter_ptrs[i] << std::dec;
      std::cerr << "\n";
    }
    return;
  }

  if (!m_started)
  {
    m_started = true;
    m_last_frame = frame_index - 1;
  }

  if (m_last_frame != INT_MIN && frame_index != m_last_frame + 1)
  {
    if (debug)
      std::cerr << "[ENGINE_DUMP_SKIP] noncontiguous frame=" << frame_index
                << " last=" << m_last_frame << "\n";
    return;
  }

  m_last_frame = frame_index;

  // Needle RNG forensics: the HSD_Randi (0x80380580) HLE hook is registered at boot in
  // HLE::PatchFunctions, but the replay savestate load can leave a stale (pre-patch) JIT block for
  // it. Force a one-time targeted recompile of just that block here (safe -- it is not the executing
  // block) so the HLE_HOOK_START trace reliably engages. Inert unless MSL_RNG_TRACE is set.
  static int s_rng_trace_invalidate_frames = 0;
  if (s_rng_trace_invalidate_frames < 4 && std::getenv("MSL_RNG_TRACE") != nullptr)
  {
    s_rng_trace_invalidate_frames++;
    // Invalidate the whole RNG function cluster (HSD_Rand/Randf/Randi live together around
    // 0x80380580) so whatever JIT block covers HSD_Randi recompiles with the HLE hook. Done on the
    // first few captured frames (not once) because the savestate-load JIT state is timing-flaky;
    // re-invalidating until the hook reliably engages well before the contact frame is cheap for a
    // short probe window.
    Core::System::GetInstance().GetJitInterface().InvalidateICache(0x80380400, 0x300, true);
    std::fprintf(stderr, "RNGTRACE_INIT invalidated HSD rand cluster frame=%d\n", frame_index);
  }

  m_is_teams = ReadU8(TEAMS_FLAG_ADDR);

  u32 rng_state = ReadU32(RNG_STATE_ADDR);
  u16 frame_flags = 0;

  // Needle RNG forensics: emit a per-frame marker so the HSD_Randi trace stream can be split into
  // frames and the pre-Needle-callback draw count measured. This is the frame-capture-point real LCG
  // state. Inert unless MSL_RNG_TRACE is set.
  if (std::getenv("MSL_RNG_TRACE") != nullptr)
    std::fprintf(stderr, "RNGFRAME frame=%d rng_state=%08x\n", frame_index, rng_state);
  u32 rng_seed = 0;
  if (frame->randomSeedExists)
  {
    frame_flags |= FRAME_FLAG_RNG_SEED_EXISTS;
    rng_seed = *(u32 *)&frame->randomSeed;
  }
  u32 rng_steps = m_last_rng_seed_valid
                      ? RngStepsBetween(m_last_rng_seed, rng_state, RNG_STEP_SEARCH_MAX)
                      : RNG_STEPS_UNKNOWN;
  u32 game_timer = ReadU32(GAME_TIMER_ADDR);

  FrameRecord fr = {};
  fr.frame_index = frame_index;
  fr.rng_state = rng_state;
  fr.rng_seed = rng_seed;
  fr.rng_steps = rng_steps;
  fr.item_offset = static_cast<u32>(m_items.size());
  fr.item_count = 0;
  fr.flags = frame_flags;
  fr.game_timer = game_timer;
  fr.randall_exists = 0;
  fr.randall_x_bits = 0;
  fr.randall_y_bits = 0;
  fr.fountain0_exists = 0;
  fr.fountain0_y_bits = 0;
  fr.fountain1_exists = 0;
  fr.fountain1_y_bits = 0;

  auto player_for_port = [frame](int port) -> const Slippi::PlayerFrameData* {
    auto it = frame->players.find(static_cast<u8>(port - 1));
    if (it == frame->players.end())
      return nullptr;
    return &it->second;
  };

  for (u8 port : m_ports)
  {
    const Slippi::PlayerFrameData* pdata = player_for_port(static_cast<int>(port));
    InputRecord in = {};
    if (pdata)
    {
      u32 mask = pdata->buttons &
                 (BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y | BUTTON_Z | BUTTON_L | BUTTON_R |
                  BUTTON_START | BUTTON_D_UP);
      float l_trig = pdata->lTrigger;
      float r_trig = pdata->rTrigger;
      if (l_trig == 0.0f && r_trig == 0.0f)
      {
        l_trig = pdata->trigger;
        r_trig = pdata->trigger;
      }
      if ((mask & (BUTTON_L | BUTTON_R)) || l_trig > 0.0f || r_trig > 0.0f)
        mask |= BUTTON_LR;

      in.buttons = mask;
      in.stick_x_bits = FloatBits(pdata->joystickX);
      in.stick_y_bits = FloatBits(pdata->joystickY);
      in.cstick_x_bits = FloatBits(pdata->cstickX);
      in.cstick_y_bits = FloatBits(pdata->cstickY);
      in.l_shoulder_bits = FloatBits(l_trig);
      in.r_shoulder_bits = FloatBits(r_trig);
      in.raw_stick_x = pdata->joystickXRaw;
      in.raw_stick_y = pdata->joystickYRaw;
      in.raw_cstick_x = pdata->cstickXRaw;
      in.raw_cstick_y = pdata->cstickYRaw;
    }
    m_inputs.push_back(in);
  }

  auto add_fighter = [this, frame_index](u32 fp_ptr, int port) {
    FighterRecord f = {};
    f.flags = 0;
    f.pos_x_bits = ReadU32(fp_ptr + FIGHTER_POS_X_OFF);
    f.pos_y_bits = ReadU32(fp_ptr + FIGHTER_POS_Y_OFF);
    f.pos_z_bits = ReadU32(fp_ptr + FIGHTER_POS_Z_OFF);
    f.self_vel_x_bits = ReadU32(fp_ptr + FIGHTER_SELF_VEL_X_OFF);
    f.self_vel_y_bits = ReadU32(fp_ptr + FIGHTER_SELF_VEL_Y_OFF);
    f.gr_vel_bits = ReadU32(fp_ptr + FIGHTER_GR_VEL_OFF);
    f.action_state = static_cast<u16>(ReadU32(fp_ptr + FIGHTER_ACTION_OFF) & 0xFFFF);
    f.anim_id = static_cast<u16>(ReadU32(fp_ptr + FIGHTER_ANIM_OFF) & 0xFFFF);
    f.anim_frame_bits = ReadU32(fp_ptr + FIGHTER_ANIM_FRAME_OFF);
    f.action_frame_bits = ReadU32(fp_ptr + FIGHTER_ACTION_FRAME_OFF);
    f.state_flags_2218 = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_2218_OFF);
    f.state_flags_221a = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221A_OFF);
    f.state_flags_221b = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221B_OFF);
    f.state_flags_221c = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221C_OFF);
    f.state_flags_221f = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221F_OFF);
    u32 collision = ReadU32(fp_ptr + FIGHTER_HURTBOX_COLLISION1_OFF);
    if (collision == 0)
      collision = ReadU32(fp_ptr + FIGHTER_HURTBOX_COLLISION2_OFF);
    f.invulnerable = static_cast<u8>(collision & 0xFF);
    // ground_or_air is a 32-bit enum; reading a single byte would grab the MSB (0) on big-endian.
    f.ground_or_air = static_cast<u8>(ReadU32(fp_ptr + FIGHTER_GROUND_OR_AIR_OFF) & 0xFF);
    f.stocks = (port == 1) ? ReadU8(P1_STOCK_ADDR) : ReadU8(P2_STOCK_ADDR);
    f.team = ReadU8(fp_ptr + FIGHTER_TEAM_OFF);
    f.costume_id = ReadU8(fp_ptr + FIGHTER_COSTUME_OFF);
    f.facing_bits = ReadU32(fp_ptr + FIGHTER_FACING_OFF);
    f.percent_bits = ReadU32(fp_ptr + FIGHTER_PERCENT_OFF);
    f.hitlag_left_bits = ReadU32(fp_ptr + FIGHTER_HITLAG_OFF);
    f.misc_as_bits = ReadU32(fp_ptr + FIGHTER_MISC_AS_OFF);
    f.shield_health_bits = ReadU32(fp_ptr + FIGHTER_SHIELD_HEALTH_OFF);
    f.ecb_top_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_TOP_OFF + 0x00);
    f.ecb_top_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_TOP_OFF + 0x04);
    f.ecb_bottom_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_BOTTOM_OFF + 0x00);
    f.ecb_bottom_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_BOTTOM_OFF + 0x04);
    f.ecb_left_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_LEFT_OFF + 0x00);
    f.ecb_left_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_LEFT_OFF + 0x04);
    f.ecb_right_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_RIGHT_OFF + 0x00);
    f.ecb_right_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_RIGHT_OFF + 0x04);
    f.floor_normal_x_bits = ReadU32(fp_ptr + FIGHTER_FLOOR_NORMAL_X_OFF);
    f.floor_normal_y_bits = ReadU32(fp_ptr + FIGHTER_FLOOR_NORMAL_Y_OFF);
    f.ground_accel_1_bits = ReadU32(fp_ptr + FIGHTER_GROUND_ACCEL_1_OFF);
    f.ground_accel_2_bits = ReadU32(fp_ptr + FIGHTER_GROUND_ACCEL_2_OFF);
    f.anim_vel_x_bits = ReadU32(fp_ptr + FIGHTER_ANIM_VEL_X_OFF);
    f.anim_vel_y_bits = ReadU32(fp_ptr + FIGHTER_ANIM_VEL_Y_OFF);
    f.x670_timers_bits = ReadU32(fp_ptr + 0x670);
    f.x674_timers_bits = ReadU32(fp_ptr + 0x674);
    f.x2344_bits = ReadU32(fp_ptr + FIGHTER_MISC_AS_OFF + 0x04);
    f.x2348_bits = ReadU32(fp_ptr + FIGHTER_MISC_AS_OFF + 0x08);
    f.x234c_bits = ReadU32(fp_ptr + FIGHTER_MISC_AS_OFF + 0x0C);
    f.transn_x_bits = ReadU32(fp_ptr + FIGHTER_TRANSN_POS_OFF + 0x00);
    f.transn_y_bits = ReadU32(fp_ptr + FIGHTER_TRANSN_POS_OFF + 0x04);
    f.transn_z_bits = ReadU32(fp_ptr + FIGHTER_TRANSN_POS_OFF + 0x08);
    f.x1a50_bits = ReadU32(fp_ptr + 0x1A50);
    m_fighters.push_back(f);
  };

  for (u32 i = 0; i < active_port_count; i++)
    add_fighter(fighter_ptrs[i], static_cast<int>(m_ports[i]));

  for (u32 port_idx = 0; port_idx < active_port_count; port_idx++)
  {
    u32 fp_ptr = fighter_ptrs[port_idx];
    for (u32 idx = 0; idx < 15; idx++)
    {
      u32 base = fp_ptr + FIGHTER_HURTBOX_BASE_OFF + idx * HURTBOX_STRIDE;
      HurtboxRecord hb = {};
      hb.state = ReadU32(base + 0x00);
      hb.a_offset_x_bits = ReadU32(base + 0x04);
      hb.a_offset_y_bits = ReadU32(base + 0x08);
      hb.a_offset_z_bits = ReadU32(base + 0x0C);
      hb.b_offset_x_bits = ReadU32(base + 0x10);
      hb.b_offset_y_bits = ReadU32(base + 0x14);
      hb.b_offset_z_bits = ReadU32(base + 0x18);
      hb.scale_bits = ReadU32(base + 0x1C);
      hb.flags = ReadU8(base + 0x24);
      hb.a_pos_x_bits = ReadU32(base + 0x28);
      hb.a_pos_y_bits = ReadU32(base + 0x2C);
      hb.a_pos_z_bits = ReadU32(base + 0x30);
      hb.b_pos_x_bits = ReadU32(base + 0x34);
      hb.b_pos_y_bits = ReadU32(base + 0x38);
      hb.b_pos_z_bits = ReadU32(base + 0x3C);
      hb.bone_index = static_cast<s32>(ReadU32(base + 0x40));
      hb.height = ReadU32(base + 0x44);
      // `FighterHurtCapsule::is_grabbable` is written as a 32-bit value in the retail binary
      // (`ftColl_HurtboxInit` uses `stw r7, 0x48(r4)`), so reading a single byte would grab
      // the MSB (0) on big-endian for `0x00000001`. Use the low byte.
      hb.is_grabbable = static_cast<u8>(ReadU32(base + 0x48) & 0xFF);
      m_hurtboxes.push_back(hb);
    }
  }

  // Debug: capture per-joint Euler rotation + matrix bits for a known mismatch frame.
  // This is intentionally sidecar-only (no dump schema change).
  //
  // Decomp references:
  // - Fighter.parts: fp + 0x5E8 (struct Fighter, refs/melee/src/melee/ft/types.h)
  // - struct FighterBone: size 0x10; joint at +0 (refs/melee/src/melee/ft/types.h)
  // - struct HSD_JObj: flags +0x14, rotate (Vec3 view) at +0x1C, mtx at +0x44
  if (frame_index == -14 || frame_index == -13)
    {
      const int port = static_cast<int>(m_ports[0]);
      const u32 fp_ptr = fighter_ptrs[0];
      const u32 parts_ptr = ReadU32(fp_ptr + FIGHTER_PARTS_PTR_OFF);
      static const u32 bone_idxs[] = {3, 4};

      auto dump_jobj = [this](std::ofstream& dbg, const char* tag, u32 ptr) {
        if (!ptr)
          return;
      const u32 jobj_flags = ReadU32(ptr + HSD_JOBJ_FLAGS_OFF);
      const u32 rot_x_bits = ReadU32(ptr + HSD_JOBJ_ROTATE_X_OFF);
      const u32 rot_y_bits = ReadU32(ptr + HSD_JOBJ_ROTATE_Y_OFF);
      const u32 rot_z_bits = ReadU32(ptr + HSD_JOBJ_ROTATE_Z_OFF);
      const u32 rot_w_bits = ReadU32(ptr + HSD_JOBJ_ROTATE_W_OFF);
      dbg << " " << tag << "=0x" << std::hex << ptr;
      dbg << " " << tag << "_flags=0x" << std::hex << jobj_flags;
      dbg << " " << tag << "_rot=0x" << std::hex << rot_x_bits << ",0x" << std::hex << rot_y_bits << ",0x"
          << std::hex << rot_z_bits << ",0x" << std::hex << rot_w_bits;
      dbg << " " << tag << "_mtx_bits=[";
      for (u32 mi = 0; mi < 12; mi++)
      {
        if (mi)
          dbg << ",";
        dbg << "0x" << std::hex << ReadU32(ptr + HSD_JOBJ_MTX_OFF + mi * 4);
      }
        dbg << "]";
      };

      if (parts_ptr)
      {
        std::ofstream dbg(m_path + ".jobj_dbg.txt", std::ios::app);
        dbg << "frame=" << frame_index << " port=" << port;
        dbg << " fp=0x" << std::hex << fp_ptr;
        dbg << " parts=0x" << std::hex << parts_ptr;
        dbg << " anim_id=0x" << std::hex << ReadU32(fp_ptr + FIGHTER_ANIM_OFF);
        dbg << " action_frame_bits=0x" << std::hex << ReadU32(fp_ptr + FIGHTER_ACTION_FRAME_OFF);
      dbg << " blend_frames_bits=0x" << std::hex << ReadU32(fp_ptr + FIGHTER_BLEND_FRAMES_OFF);
      dbg << " blend_frame_bits=0x" << std::hex << ReadU32(fp_ptr + FIGHTER_ANIM_FRAME_OFF);
      const u32 ft_data_ptr = ReadU32(fp_ptr + FIGHTER_FT_DATA_PTR_OFF);
      const u32 ftdata_x20_ptr = ft_data_ptr ? ReadU32(ft_data_ptr + FTDATA_X20_PTR_OFF) : 0;
      dbg << " ft_data=0x" << std::hex << ft_data_ptr;
      dbg << " ftdata_x20=0x" << std::hex << ftdata_x20_ptr;
      if (ftdata_x20_ptr)
      {
        dbg << " ftdata_x20_words=[0x" << std::hex << ReadU32(ftdata_x20_ptr + 0x0);
        dbg << ",0x" << std::hex << ReadU32(ftdata_x20_ptr + 0x4);
        dbg << ",0x" << std::hex << ReadU32(ftdata_x20_ptr + 0x8);
        dbg << ",0x" << std::hex << ReadU32(ftdata_x20_ptr + 0xC) << "]";
      }
      if (parts_ptr)
      {
        static const u32 watched_parts[] = {0, 1, 2, 3, 4, 6, 7, 12, 13, 18, 22, 25, 26, 41, 55, 56};
        dbg << " watched_quat=[";
        for (u32 wi = 0; wi < sizeof(watched_parts) / sizeof(watched_parts[0]); wi++)
        {
          const u32 part = watched_parts[wi];
          const u32 bone_ptr2 = parts_ptr + part * FIGHTER_BONE_STRIDE;
          const u32 jp = ReadU32(bone_ptr2 + 0x0);
          const u32 fl = jp ? ReadU32(jp + HSD_JOBJ_FLAGS_OFF) : 0;
          if (wi)
            dbg << ",";
          dbg << std::dec << part << ":" << (((fl & 0x20000) != 0) ? 1 : 0);
        }
        dbg << "]";
      }
      for (u32 i = 0; i < 5; i++)
      {
        const u32 base = fp_ptr + FIGHTER_X8B0_OFF + i * FIGHTER_X8B0_STRIDE;
        const u32 x4_bits = ReadU32(base + 0x04);
        const u8 x10_u = ReadU8(base + 0x10);
        const u8 x11_u = ReadU8(base + 0x11);
        const int x10 = x10_u >= 0x80 ? (int)x10_u - 0x100 : (int)x10_u;
        const int x11 = x11_u >= 0x80 ? (int)x11_u - 0x100 : (int)x11_u;
        dbg << " x8B0[" << std::dec << i << "]={x10=" << x10 << " x11=" << x11 << " x4_bits=0x"
              << std::hex << x4_bits << "}";
        }
        for (u32 bi = 0; bi < sizeof(bone_idxs) / sizeof(bone_idxs[0]); bi++)
        {
          const u32 bone_idx = bone_idxs[bi];
          const u32 bone_ptr = parts_ptr + bone_idx * FIGHTER_BONE_STRIDE;
          const u32 jobj_ptr = ReadU32(bone_ptr + 0x0);
          const u32 jobj2_ptr = ReadU32(bone_ptr + 0x4);
          dbg << " bone=" << std::dec << bone_idx;
          dump_jobj(dbg, "jobj", jobj_ptr);
          dump_jobj(dbg, "jobj2", jobj2_ptr);
        }
        dbg << std::dec << "\n";
      }
    }


    for (u32 port_idx = 0; port_idx < active_port_count; port_idx++)
    {
      u32 fp_ptr = fighter_ptrs[port_idx];
      for (u32 idx = 0; idx < 4; idx++)
      {
        u32 base = fp_ptr + FIGHTER_HITBOX_BASE_OFF + idx * HITBOX_STRIDE;
        HitboxRecord hb = {};
        hb.state = ReadU32(base + 0x00);
        hb.group = ReadU32(base + 0x04);
        hb.damage = ReadU32(base + 0x08);
        hb.damage_stale_bits = ReadU32(base + 0x0C);
        hb.offset_x_bits = ReadU32(base + 0x18);
        hb.offset_y_bits = ReadU32(base + 0x14);
        hb.offset_z_bits = ReadU32(base + 0x10);
        hb.size_bits = ReadU32(base + 0x1C);
        hb.angle = ReadU32(base + 0x20);
        hb.kbg = ReadU32(base + 0x24);
        hb.wsk = ReadU32(base + 0x28);
        hb.bkb = ReadU32(base + 0x2C);
        hb.element = ReadU32(base + 0x30);
        hb.shield_damage = ReadU32(base + 0x34);
        hb.sfx = ReadU32(base + 0x38);
        hb.sfx_kind = ReadU32(base + 0x3C);
        for (u32 i = 0; i < 8; i++)
        {
          hb.flags[i] = ReadU8(base + 0x40 + i);
        }
        hb.bone_ptr = ReadU32(base + 0x48);
        hb.pos_x_bits = ReadU32(base + 0x54);
        hb.pos_y_bits = ReadU32(base + 0x50);
        hb.pos_z_bits = ReadU32(base + 0x4C);
        m_hitboxes.push_back(hb);

        HitlistRecord hl = {};
        hl.group = ReadU32(base + 0x04);
        // Provenance lanes for lbColl_8000ACFC victim containment and
        // ftColl_800768A0 clear/copy ownership:
        // - refs/melee/src/melee/lb/types.h (HitCapsule +0x44/+0x45/+0x74/+0xD4/+0x134)
        // - refs/melee/src/melee/lb/lbcollision.c (lbColl_80008440, lbColl_CopyHitCapsule, lbColl_8000ACFC)
        // - refs/melee/src/melee/ft/ftcoll.c (ftColl_800768A0, ftColl_80076CBC)
        hl.victims1_cursor = ReadU8(base + HITBOX_VICTIMS1_CURSOR_OFF);
        hl.victims2_cursor = ReadU8(base + HITBOX_VICTIMS2_CURSOR_OFF);
        hl.owner_gobj = ReadU32(base + HITBOX_OWNER_OFF);
        for (u32 slot = 0; slot < 12; slot++)
        {
          u32 v1 = base + HITBOX_VICTIMS1_BASE_OFF + slot * HITBOX_VICTIM_STRIDE;
          u32 v2 = base + HITBOX_VICTIMS2_BASE_OFF + slot * HITBOX_VICTIM_STRIDE;
          hl.victims1_ptr[slot] = ReadU32(v1 + 0x0);
          hl.victims1_cooldown[slot] = ReadU32(v1 + 0x4);
          hl.victims2_ptr[slot] = ReadU32(v2 + 0x0);
          hl.victims2_cooldown[slot] = ReadU32(v2 + 0x4);
        }
        m_hitlists.push_back(hl);
      }
    }

  std::unordered_map<u32, int> owner_map;
  owner_map[FighterGobjForPort(1)] = 1;
  owner_map[FighterGobjForPort(2)] = 2;

  u32 manager_ptr = ReadU32(ITEM_MANAGER_PTR);
  u32 item_gobj = manager_ptr ? ReadU32(manager_ptr + ITEM_MANAGER_FIRST_GOBJ_OFF) : 0;
  u32 count = 0;
  while (item_gobj != 0 && count < MAX_ITEMS)
  {
    u32 item_data = ReadU32(item_gobj + ITEM_DATA_OFF);
    if (item_data != 0)
    {
      ItemRecord it = {};
      it.item_id = item_data;
      it.kind = static_cast<u16>(ReadU32(item_data + ITEM_KIND_OFF) & 0xFFFF);
      it.state = static_cast<u16>(ReadU32(item_data + ITEM_STATE_OFF) & 0xFFFF);
      u32 owner_gobj = ReadU32(item_data + ITEM_OWNER_OFF);
      auto found = owner_map.find(owner_gobj);
      it.owner_port = found == owner_map.end() ? -1 : static_cast<int8_t>(found->second);
      it.flags = 0;
      it.pos_x_bits = ReadU32(item_data + ITEM_POS_OFF + 0x00);
      it.pos_y_bits = ReadU32(item_data + ITEM_POS_OFF + 0x04);
      it.pos_z_bits = ReadU32(item_data + ITEM_POS_OFF + 0x08);
      it.vel_x_bits = ReadU32(item_data + ITEM_VEL_OFF + 0x00);
      it.vel_y_bits = ReadU32(item_data + ITEM_VEL_OFF + 0x04);
      it.vel_z_bits = ReadU32(item_data + ITEM_VEL_OFF + 0x08);
      it.facing_bits = ReadU32(item_data + ITEM_FACING_OFF);
      it.anim_id = static_cast<u16>(ReadU32(item_data + ITEM_ANIM_ID_OFF) & 0xFFFF);
      it.anim_frame_bits = ReadU32(item_data + ITEM_ANIM_FRAME_OFF);
      it.lifetime_bits = ReadU32(item_data + ITEM_LIFETIME_OFF);
      it.damage = ReadU32(item_data + ITEM_HITBOX0_OFF + 0x08);
      it.xC34_damage_dealt = ReadS32(item_data + ITEM_XC34_DAMAGE_DEALT_OFF);
      it.xC48_clank_damage = ReadS32(item_data + ITEM_XC48_CLANK_DAMAGE_OFF);
      it.xC4C_reflect_damage = ReadS32(item_data + ITEM_XC4C_REFLECT_DAMAGE_OFF);
      it.xC50_shield_damage = ReadS32(item_data + ITEM_XC50_SHIELD_DAMAGE_OFF);
      it.xCA8_callback_damage = ReadS32(item_data + ITEM_XCA8_CALLBACK_DAMAGE_OFF);
      it.xCBC_hitlag_bits = ReadU32(item_data + ITEM_XCBC_HITLAG_OFF);
      it.xCC0_hitlag_min_bits = ReadU32(item_data + ITEM_XCC0_HITLAG_MIN_OFF);
      it.xDA8_short = ReadU16(item_data + ITEM_XDA8_SHORT_OFF);
      it.xDC8_word = ReadU32(item_data + ITEM_XDC8_WORD_OFF);
      it.xDCE_flags = ReadU8(item_data + ITEM_XDCE_FLAGS_OFF);
      it.xDD4_laser_scale_bits = ReadU32(item_data + ITEM_FOXLASER_SCALE_OFF);
      it.xDD8_laser_angle_bits = ReadU32(item_data + ITEM_FOXLASER_ANGLE_OFF);
      it.xDDC_laser_speed_bits = ReadU32(item_data + ITEM_FOXLASER_SPEED_OFF);
      it.xDE0_laser_pos_x_bits = ReadU32(item_data + ITEM_FOXLASER_POS_OFF + 0x00);
      it.xDE4_laser_pos_y_bits = ReadU32(item_data + ITEM_FOXLASER_POS_OFF + 0x04);
      it.xDE8_laser_pos_z_bits = ReadU32(item_data + ITEM_FOXLASER_POS_OFF + 0x08);
      m_items.push_back(it);
      for (u32 idx = 0; idx < 4; idx++)
      {
        u32 base = item_data + ITEM_HITBOX0_OFF + idx * ITEM_HITBOX_STRIDE;
        HitlistRecord hl = {};
        hl.group = ReadU32(base + 0x04);
        // Item HitCapsule victim-ring/callback provenance:
        // - item BODY/reflect/shield paths call it_8026FAC4 -> it_8026FA2C, which updates
        //   matching item hitbox victim rings through lbColl_80008688.
        // - refs/melee/src/melee/it/itcoll.c::{it_8026FAC4,it_8026FA2C}
        // - refs/melee/src/melee/lb/lbcollision.c::lbColl_80008688
        // - refs/melee/src/melee/it/item.c::{OnGiveDamageThink,Item_8026A294}
        hl.victims1_cursor = ReadU8(base + HITBOX_VICTIMS1_CURSOR_OFF);
        hl.victims2_cursor = ReadU8(base + HITBOX_VICTIMS2_CURSOR_OFF);
        hl.owner_gobj = ReadU32(base + HITBOX_OWNER_OFF);
        for (u32 slot = 0; slot < 12; slot++)
        {
          u32 v1 = base + HITBOX_VICTIMS1_BASE_OFF + slot * HITBOX_VICTIM_STRIDE;
          u32 v2 = base + HITBOX_VICTIMS2_BASE_OFF + slot * HITBOX_VICTIM_STRIDE;
          hl.victims1_ptr[slot] = ReadU32(v1 + 0x0);
          hl.victims1_cooldown[slot] = ReadU32(v1 + 0x4);
          hl.victims2_ptr[slot] = ReadU32(v2 + 0x0);
          hl.victims2_cooldown[slot] = ReadU32(v2 + 0x4);
        }
        m_item_hitlists.push_back(hl);
      }
      fr.item_count += 1;
    }
    item_gobj = ReadU32(item_gobj + GOBJ_NEXT_OFF);
    count += 1;
  }

  m_frames.push_back(fr);
  if (frame_flags & FRAME_FLAG_RNG_SEED_EXISTS)
  {
    m_last_rng_seed = rng_seed;
    m_last_rng_seed_valid = true;
  }

  if (frame_index >= m_end_frame)
    Finalize();
}

void EngineDumpWriter::Finalize()
{
  if (m_path.empty() || m_frames.empty())
    return;

  const u32 frame_count = static_cast<u32>(m_frames.size());
  const u32 port_count = m_port_count;
  const u32 total_items = static_cast<u32>(m_items.size());

  const u32 frame_rec_size = 48;
  const u32 input_rec_size = 32;
  // v12 fighter record is 164 bytes (v7 128 + v8/v9/v11/v12 hidden lanes). This stride drives the
  // items/hitboxes/hurtboxes/hitlists section offsets; a stale 128 here mislocated every
  // post-fighter section by port_count*frame_count*36 bytes (item/hitbox/hurtbox lanes unreadable).
  const u32 fighter_rec_size = 164;
  const u32 item_rec_size = 113;
  const u32 hitbox_rec_size = 88;
  const u32 hurtbox_rec_size = 68;
  const u32 hitlist_rec_size = 204;

  const u32 frames_offset = 80;
  const u32 inputs_offset = frames_offset + frame_count * frame_rec_size;
  const u32 fighters_offset = inputs_offset + frame_count * port_count * input_rec_size;
  const u32 items_offset = fighters_offset + frame_count * port_count * fighter_rec_size;
  const u32 hitboxes_offset = items_offset + total_items * item_rec_size;
  const u32 hurtboxes_offset = hitboxes_offset + frame_count * port_count * 4 * hitbox_rec_size;
  const u32 hitlists_offset = hurtboxes_offset + frame_count * port_count * 15 * hurtbox_rec_size;
  const u32 item_hitlists_offset = hitlists_offset + frame_count * port_count * 4 * hitlist_rec_size;

  std::vector<u8> out;
  out.reserve(item_hitlists_offset + total_items * 4 * hitlist_rec_size);

  for (u32 i = 0; i < 8; i++)
    AppendU8(out, ENGINE_DUMP_MAGIC[i]);
  AppendU32(out, ENGINE_DUMP_VERSION);
  AppendU32(out, ENGINE_DUMP_ENDIAN_TAG);
  AppendU32(out, frame_count);
  AppendU8(out, static_cast<u8>(port_count));
  AppendU16(out, m_stage_id);
  AppendU8(out, m_is_teams);
  for (u32 i = 0; i < 4; i++)
    AppendU8(out, i < m_ports.size() ? m_ports[i] : 0);
  for (u32 i = 0; i < 13; i++)
    AppendU8(out, 0);
  AppendU32(out, frames_offset);
  AppendU32(out, inputs_offset);
  AppendU32(out, fighters_offset);
  AppendU32(out, items_offset);
  AppendU32(out, hitboxes_offset);
  AppendU32(out, hurtboxes_offset);
  AppendU32(out, total_items);
  AppendU32(out, hitlists_offset);
  AppendU32(out, item_hitlists_offset);
  for (u32 i = 0; i < 3; i++)
    AppendU8(out, 0);
  while (out.size() < frames_offset)
    out.push_back(0);

  for (const auto& fr : m_frames)
  {
    AppendI32(out, fr.frame_index);
    AppendU32(out, fr.rng_state);
    AppendU32(out, fr.rng_seed);
    AppendU32(out, fr.rng_steps);
    AppendU16(out, fr.item_count);
    AppendU32(out, fr.item_offset);
    AppendU16(out, fr.flags);
    AppendU32(out, fr.game_timer);
    AppendU8(out, fr.randall_exists);
    AppendU32(out, fr.randall_x_bits);
    AppendU32(out, fr.randall_y_bits);
    AppendU8(out, fr.fountain0_exists);
    AppendU32(out, fr.fountain0_y_bits);
    AppendU8(out, fr.fountain1_exists);
    AppendU32(out, fr.fountain1_y_bits);
    AppendU8(out, 0);
  }

  for (const auto& in : m_inputs)
  {
    AppendU32(out, in.buttons);
    AppendU32(out, in.stick_x_bits);
    AppendU32(out, in.stick_y_bits);
    AppendU32(out, in.cstick_x_bits);
    AppendU32(out, in.cstick_y_bits);
    AppendU32(out, in.l_shoulder_bits);
    AppendU32(out, in.r_shoulder_bits);
    AppendU8(out, in.raw_stick_x);
    AppendU8(out, in.raw_stick_y);
    AppendU8(out, in.raw_cstick_x);
    AppendU8(out, in.raw_cstick_y);
  }

  for (const auto& f : m_fighters)
  {
    AppendU32(out, f.flags);
    AppendU32(out, f.pos_x_bits);
    AppendU32(out, f.pos_y_bits);
    AppendU32(out, f.pos_z_bits);
    AppendU32(out, f.self_vel_x_bits);
    AppendU32(out, f.self_vel_y_bits);
    AppendU32(out, f.gr_vel_bits);
    AppendU16(out, f.action_state);
    AppendU16(out, f.anim_id);
    AppendU32(out, f.anim_frame_bits);
    AppendU32(out, f.action_frame_bits);
    AppendU8(out, f.state_flags_2218);
    AppendU8(out, f.state_flags_221a);
    AppendU8(out, f.state_flags_221b);
    AppendU8(out, f.state_flags_221c);
    AppendU8(out, f.state_flags_221f);
    AppendU8(out, f.invulnerable);
    AppendU8(out, f.ground_or_air);
    AppendU8(out, f.stocks);
    AppendU8(out, f.team);
    AppendU8(out, f.costume_id);
    AppendU32(out, f.facing_bits);
    AppendU32(out, f.percent_bits);
    AppendU32(out, f.hitlag_left_bits);
    AppendU32(out, f.misc_as_bits);
    AppendU32(out, f.shield_health_bits);
    AppendU32(out, f.ecb_top_x_bits);
    AppendU32(out, f.ecb_top_y_bits);
    AppendU32(out, f.ecb_bottom_x_bits);
    AppendU32(out, f.ecb_bottom_y_bits);
    AppendU32(out, f.ecb_left_x_bits);
    AppendU32(out, f.ecb_left_y_bits);
    AppendU32(out, f.ecb_right_x_bits);
    AppendU32(out, f.ecb_right_y_bits);
    AppendU32(out, f.floor_normal_x_bits);
    AppendU32(out, f.floor_normal_y_bits);
    AppendU32(out, f.ground_accel_1_bits);
    AppendU32(out, f.ground_accel_2_bits);
    AppendU32(out, f.anim_vel_x_bits);
    AppendU32(out, f.anim_vel_y_bits);
    AppendU16(out, 0);
    AppendU32(out, f.x670_timers_bits);
    AppendU32(out, f.x674_timers_bits);
    AppendU32(out, f.x2344_bits);
    AppendU32(out, f.x2348_bits);
    AppendU32(out, f.x234c_bits);
    AppendU32(out, f.transn_x_bits);
    AppendU32(out, f.transn_y_bits);
    AppendU32(out, f.transn_z_bits);
    AppendU32(out, f.x1a50_bits);
  }

  for (const auto& it : m_items)
  {
    AppendU32(out, it.item_id);
    AppendU16(out, it.kind);
    AppendU16(out, it.state);
    AppendU8(out, static_cast<u8>(it.owner_port));
    AppendU8(out, it.flags);
    AppendU32(out, it.pos_x_bits);
    AppendU32(out, it.pos_y_bits);
    AppendU32(out, it.pos_z_bits);
    AppendU32(out, it.vel_x_bits);
    AppendU32(out, it.vel_y_bits);
    AppendU32(out, it.vel_z_bits);
    AppendU32(out, it.facing_bits);
    AppendU16(out, it.anim_id);
    AppendU16(out, 0);
    AppendU32(out, it.anim_frame_bits);
    AppendU32(out, it.lifetime_bits);
    AppendU32(out, it.damage);
    AppendI32(out, it.xC34_damage_dealt);
    AppendI32(out, it.xC48_clank_damage);
    AppendI32(out, it.xC4C_reflect_damage);
    AppendI32(out, it.xC50_shield_damage);
    AppendI32(out, it.xCA8_callback_damage);
    AppendU32(out, it.xCBC_hitlag_bits);
    AppendU32(out, it.xCC0_hitlag_min_bits);
    AppendU16(out, it.xDA8_short);
    AppendU32(out, it.xDC8_word);
    AppendU8(out, it.xDCE_flags);
    AppendU32(out, it.xDD4_laser_scale_bits);
    AppendU32(out, it.xDD8_laser_angle_bits);
    AppendU32(out, it.xDDC_laser_speed_bits);
    AppendU32(out, it.xDE0_laser_pos_x_bits);
    AppendU32(out, it.xDE4_laser_pos_y_bits);
    AppendU32(out, it.xDE8_laser_pos_z_bits);
  }

  for (const auto& hb : m_hitboxes)
  {
    AppendU32(out, hb.state);
    AppendU32(out, hb.group);
    AppendU32(out, hb.damage);
    AppendU32(out, hb.damage_stale_bits);
    AppendU32(out, hb.offset_x_bits);
    AppendU32(out, hb.offset_y_bits);
    AppendU32(out, hb.offset_z_bits);
    AppendU32(out, hb.size_bits);
    AppendU32(out, hb.angle);
    AppendU32(out, hb.kbg);
    AppendU32(out, hb.wsk);
    AppendU32(out, hb.bkb);
    AppendU32(out, hb.element);
    AppendU32(out, hb.shield_damage);
    AppendU32(out, hb.sfx);
    AppendU32(out, hb.sfx_kind);
    for (u32 i = 0; i < 8; i++)
      AppendU8(out, hb.flags[i]);
    AppendU32(out, hb.bone_ptr);
    AppendU32(out, hb.pos_x_bits);
    AppendU32(out, hb.pos_y_bits);
    AppendU32(out, hb.pos_z_bits);
  }

  for (const auto& hb : m_hurtboxes)
  {
    AppendU32(out, hb.state);
    AppendU32(out, hb.a_offset_x_bits);
    AppendU32(out, hb.a_offset_y_bits);
    AppendU32(out, hb.a_offset_z_bits);
    AppendU32(out, hb.b_offset_x_bits);
    AppendU32(out, hb.b_offset_y_bits);
    AppendU32(out, hb.b_offset_z_bits);
    AppendU32(out, hb.scale_bits);
    AppendU32(out, hb.a_pos_x_bits);
    AppendU32(out, hb.a_pos_y_bits);
    AppendU32(out, hb.a_pos_z_bits);
    AppendU32(out, hb.b_pos_x_bits);
    AppendU32(out, hb.b_pos_y_bits);
    AppendU32(out, hb.b_pos_z_bits);
    AppendI32(out, hb.bone_index);
    AppendU32(out, hb.height);
    AppendU8(out, hb.is_grabbable);
    AppendU8(out, hb.flags);
    AppendU16(out, 0);
  }

  for (const auto& hl : m_hitlists)
  {
    AppendU32(out, hl.group);
    AppendU8(out, hl.victims1_cursor);
    AppendU8(out, hl.victims2_cursor);
    AppendU16(out, 0);
    AppendU32(out, hl.owner_gobj);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims1_ptr[i]);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims1_cooldown[i]);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims2_ptr[i]);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims2_cooldown[i]);
  }

  for (const auto& hl : m_item_hitlists)
  {
    AppendU32(out, hl.group);
    AppendU8(out, hl.victims1_cursor);
    AppendU8(out, hl.victims2_cursor);
    AppendU16(out, 0);
    AppendU32(out, hl.owner_gobj);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims1_ptr[i]);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims1_cooldown[i]);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims2_ptr[i]);
    for (u32 i = 0; i < 12; i++)
      AppendU32(out, hl.victims2_cooldown[i]);
  }

  File::CreateFullPath(m_path);
  std::ofstream file(m_path, std::ios::binary);
  if (!file.is_open())
    return;
  file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
  file.close();

  m_path.clear();
  m_frames.clear();
  m_inputs.clear();
  m_fighters.clear();
  m_items.clear();
  m_hitboxes.clear();
  m_hitlists.clear();
  m_item_hitlists.clear();
  m_hurtboxes.clear();
  m_started = false;
  m_last_frame = INT_MIN;
}
