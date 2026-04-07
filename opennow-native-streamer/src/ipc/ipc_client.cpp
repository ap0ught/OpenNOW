#include "opennow/native/ipc_client.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <cstring>
#include <vector>

namespace opennow::native {

namespace {
constexpr std::size_t kReadChunkSize = 4096;

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif
}

IpcClient::IpcClient() = default;

IpcClient::~IpcClient() {
  Disconnect();
}

bool IpcClient::Connect(const std::string& host, std::uint16_t port) {
  Disconnect();

#if defined(_WIN32)
  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    EmitStatus("WSAStartup failed");
    return false;
  }
#endif

  socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ == kInvalidSocket) {
    EmitStatus("Failed to create IPC socket");
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
    EmitStatus("Failed to parse IPC host address");
    Disconnect();
    return false;
  }

  if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    EmitStatus("Failed to connect to Electron IPC server");
    Disconnect();
    return false;
  }

  running_.store(true);
  read_thread_ = std::thread([this]() { ReadLoop(); });
  return true;
}

void IpcClient::Disconnect() {
  running_.store(false);
  if (socket_fd_ != kInvalidSocket) {
#if defined(_WIN32)
    ::shutdown(socket_fd_, SD_BOTH);
    ::closesocket(socket_fd_);
    WSACleanup();
#else
    ::shutdown(socket_fd_, SHUT_RDWR);
    ::close(socket_fd_);
#endif
    socket_fd_ = kInvalidSocket;
  }
  if (read_thread_.joinable()) {
    read_thread_.join();
  }
}

bool IpcClient::SendJson(const std::string& json) {
  std::lock_guard<std::mutex> lock(write_mutex_);
  if (socket_fd_ == kInvalidSocket) {
    return false;
  }

  const auto size = static_cast<std::uint32_t>(json.size());
  std::array<std::uint8_t, 4> header{
      static_cast<std::uint8_t>((size >> 24) & 0xff),
      static_cast<std::uint8_t>((size >> 16) & 0xff),
      static_cast<std::uint8_t>((size >> 8) & 0xff),
      static_cast<std::uint8_t>(size & 0xff),
  };

  if (::send(socket_fd_, header.data(), header.size(), 0) != static_cast<ssize_t>(header.size())) {
    return false;
  }
  if (::send(socket_fd_, json.data(), json.size(), 0) != static_cast<ssize_t>(json.size())) {
    return false;
  }
  return true;
}

void IpcClient::SetMessageHandler(MessageHandler handler) {
  message_handler_ = std::move(handler);
}

void IpcClient::SetStatusHandler(StatusHandler handler) {
  status_handler_ = std::move(handler);
}

void IpcClient::ReadLoop() {
  std::vector<std::uint8_t> buffer;
  buffer.reserve(kReadChunkSize * 2);
  std::array<std::uint8_t, kReadChunkSize> chunk{};

  while (running_.load()) {
    const auto received = ::recv(socket_fd_, chunk.data(), chunk.size(), 0);
    if (received <= 0) {
      break;
    }
    buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + received);

    while (buffer.size() >= 4) {
      const std::uint32_t size =
          (static_cast<std::uint32_t>(buffer[0]) << 24) |
          (static_cast<std::uint32_t>(buffer[1]) << 16) |
          (static_cast<std::uint32_t>(buffer[2]) << 8) |
          static_cast<std::uint32_t>(buffer[3]);
      if (buffer.size() < size + 4) {
        break;
      }

      const std::string json(buffer.begin() + 4, buffer.begin() + 4 + size);
      if (message_handler_) {
        message_handler_(json);
      }
      buffer.erase(buffer.begin(), buffer.begin() + 4 + size);
    }
  }

  EmitStatus("IPC connection closed");
}

void IpcClient::EmitStatus(const std::string& message) {
  if (status_handler_) {
    status_handler_(message);
  }
}

}  // namespace opennow::native
