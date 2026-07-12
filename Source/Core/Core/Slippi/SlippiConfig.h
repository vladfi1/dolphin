#pragma once

namespace Melee
{
enum class Version
{
  NTSC,
  TwentyXX,
  UPTM,
  MEX,
  OTHER,
};
}

namespace Slippi
{
enum class Chat
{
  ON,
  DIRECT_ONLY,
  OFF
};

struct Config
{
  Melee::Version melee_version;
  bool oc_enable = true;
  float oc_factor = 1.0f;
  std::string slippi_input = ""; // Putting the default value here doesn't work for some reason
  // Pause playback and require external step commands (see SlippiSpectate "playback_step")
  bool slippi_playback_step = false;
};
}  // namespace Slippi
