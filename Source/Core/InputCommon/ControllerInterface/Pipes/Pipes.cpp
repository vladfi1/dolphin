// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifndef USE_ZMQ
#include <unistd.h>
#else
#include <zmq.h>

static const char* ZMQErrorString()
{
  return zmq_strerror(zmq_errno());
}
#endif

#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Pipes/Pipes.h"

namespace ciface
{
namespace Pipes
{
static const std::array<std::string, 12> s_button_tokens{
    {"A", "B", "X", "Y", "Z", "START", "L", "R", "D_UP", "D_DOWN", "D_LEFT", "D_RIGHT"}};

static const std::array<std::string, 2> s_shoulder_tokens{{"L", "R"}};

static const std::array<std::string, 2> s_axis_tokens{{"MAIN", "C"}};

static double StringToDouble(const std::string& text)
{
  std::istringstream is(text);
  // ignore current locale
  is.imbue(std::locale::classic());
  double result;
  is >> result;
  return result;
}

void PopulateDevices()
{
  // Search the Pipes directory for files that we can open in read-only,
  // non-blocking mode. The device name is the virtual name of the file.
  File::FSTEntry fst;
  std::string dir_path = File::GetUserPath(D_PIPES_IDX);
  if (!File::Exists(dir_path))
    return;
  fst = File::ScanDirectoryTree(dir_path, false);
  if (!fst.isDirectory)
    return;
  for (unsigned int i = 0; i < fst.size; ++i)
  {
    const File::FSTEntry& child = fst.children[i];
    if (child.isDirectory)
      continue;
#ifndef USE_ZMQ
    int fd = open(child.physicalName.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
      continue;
    g_controller_interface.AddDevice(std::make_shared<PipeDevice>(fd, child.virtualName));
#else
    std::ifstream in(child.physicalName);
    int port;
    in >> port;
    g_controller_interface.AddDevice(std::make_shared<PipeDevice>(port, child.virtualName));
#endif
  }
}

#ifndef USE_ZMQ
PipeDevice::PipeDevice(int fd, const std::string& name) : m_fd(fd), m_name(name)
{
#else
PipeDevice::PipeDevice(int port, const std::string& name) : m_name(name)
{
  std::cout << "Creating pipe device with port " << port << std::endl;

  if (!(m_context = zmq_ctx_new()))
  {
    std::cout << "Failed to allocate zmq context: " << ZMQErrorString() << std::endl;
  }

  if(!(m_socket = zmq_socket(m_context, ZMQ_PULL)))
  {
    std::cout << "Failed to create zmq socket: " << ZMQErrorString() << std::endl;
  }

  std::string address = "tcp://localhost:" + std::to_string(port);
  if (zmq_connect(m_socket, address.c_str()) < 0)
  {
    std::cout << "Error connecting socket: " << ZMQErrorString() << std::endl;
  }

  std::cout << "Connected pipe device to " << address << std::endl;
#endif
  for (const auto& tok : s_button_tokens)
  {
    PipeInput* btn = new PipeInput("Button " + tok);
    AddInput(btn);
    m_buttons[tok] = btn;
  }
  for (const auto& tok : s_shoulder_tokens)
  {
    AddAxis(tok, 0.0);
  }
  for (const auto& tok : s_axis_tokens)
  {
    AddAxis(tok + " X", 0.5);
    AddAxis(tok + " Y", 0.5);
  }
}

PipeDevice::~PipeDevice()
{
#ifndef USE_ZMQ
  close(m_fd);
#else
  zmq_close(m_socket);
  zmq_ctx_destroy(m_context);
#endif
}


void PipeDevice::UpdateInput()
{
#ifndef USE_ZMQ
  // Read any pending characters off the pipe. If we hit a newline,
  // then dequeue a command off the front of m_buf and parse it.
  char buf[32];
  ssize_t bytes_read = read(m_fd, buf, sizeof buf);
  while (bytes_read > 0)
  {
    m_buf.append(buf, bytes_read);
    bytes_read = read(m_fd, buf, sizeof buf);
  }
  std::size_t newline = m_buf.find("\n");
  while (newline != std::string::npos)
  {
    std::string command = m_buf.substr(0, newline);
    ParseCommand(command);
    m_buf.erase(0, newline + 1);
    newline = m_buf.find("\n");
  }
#else
  zmq_msg_t msg;
  int rc = zmq_msg_init(&msg);
  //assert(rc == 0);

  rc = zmq_recvmsg(m_socket, &msg, ZMQ_DONTWAIT);

  if (rc >= 0)
  {
    const char* data = static_cast<char*>(zmq_msg_data(&msg));
    size_t size = zmq_msg_size(&msg);
    size_t prev = 0;
    for (size_t next = 0; next < size; ++next)
    {
      if (data[next] == '\n')
      {
        std::string command(data + prev, next - prev);
        ParseCommand(command);
        prev = next + 1;
      }
    }
  }
  else
  {
    if(zmq_errno() != EAGAIN)
      std::cout << ZMQErrorString() << std::endl;
  }

  zmq_msg_close (&msg);
#endif
}

void PipeDevice::AddAxis(const std::string& name, double value)
{
  // Dolphin uses separate axes for left/right, which complicates things.
  PipeInput* ax_hi = new PipeInput("Axis " + name + " +");
  ax_hi->SetState(value);
  PipeInput* ax_lo = new PipeInput("Axis " + name + " -");
  ax_lo->SetState(value);
  m_axes[name + " +"] = ax_hi;
  m_axes[name + " -"] = ax_lo;
  AddAnalogInputs(ax_lo, ax_hi);
}

void PipeDevice::SetAxis(const std::string& entry, double value)
{
  value = MathUtil::Clamp(value, 0.0, 1.0);
  double hi = std::max(0.0, value - 0.5) * 2.0;
  double lo = (0.5 - std::min(0.5, value)) * 2.0;
  auto search_hi = m_axes.find(entry + " +");
  if (search_hi != m_axes.end())
    search_hi->second->SetState(hi);
  auto search_lo = m_axes.find(entry + " -");
  if (search_lo != m_axes.end())
    search_lo->second->SetState(lo);
}

void PipeDevice::ParseCommand(const std::string& command)
{
  const std::vector<std::string> tokens = SplitString(command, ' ');
  if (tokens.size() < 2 || tokens.size() > 4)
    return;
  if (tokens[0] == "PRESS" || tokens[0] == "RELEASE")
  {
    auto search = m_buttons.find(tokens[1]);
    if (search != m_buttons.end())
      search->second->SetState(tokens[0] == "PRESS" ? 1.0 : 0.0);
  }
  else if (tokens[0] == "SET")
  {
    if (tokens.size() == 3)
    {
      double value = StringToDouble(tokens[2]);
      SetAxis(tokens[1], (value / 2.0) + 0.5);
    }
    else if (tokens.size() == 4)
    {
      double x = StringToDouble(tokens[2]);
      double y = StringToDouble(tokens[3]);
      SetAxis(tokens[1] + " X", x);
      SetAxis(tokens[1] + " Y", y);
    }
  }
}
}
}
