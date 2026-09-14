#include "Platform/PlatformNetworking.h"
#include "Platform/Windows/Win32Networking.h"

#include <cstring>
#include <limits>
#include <string>

namespace Platform::Networking
{
bool Startup() noexcept
{
    WSADATA data{};
    return WSAStartup(MAKEWORD(1, 1), &data) == 0;
}

void Shutdown() noexcept
{
    WSACleanup();
}

void Close(NativeSocket& socket) noexcept
{
    if (socket)
        closesocket(Windows::ToSocket(socket));
    socket = {};
}

int LastError() noexcept
{
    return WSAGetLastError();
}

bool IsWouldBlock(int error) noexcept
{
    return error == WSAEWOULDBLOCK;
}

bool GetLocalHostName(char* destination, std::size_t size) noexcept
{
    if (!destination || size == 0 || size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    return gethostname(destination, static_cast<int>(size)) != SOCKET_ERROR;
}

bool ResolveIPv4(std::string_view address, std::uint32_t& hostOrderAddress) noexcept
{
    hostOrderAddress = 0;
    if (address.empty())
        return false;

    const std::string nullTerminated(address);
    if (nullTerminated.front() >= '0' && nullTerminated.front() <= '9')
    {
        hostOrderAddress = ntohl(inet_addr(nullTerminated.c_str()));
        return true;
    }

    const HOSTENT* host = gethostbyname(nullTerminated.c_str());
    if (!host || !host->h_addr || host->h_length < static_cast<int>(sizeof(std::uint32_t)))
        return false;

    std::uint32_t networkOrderAddress = 0;
    std::memcpy(&networkOrderAddress, host->h_addr, sizeof(networkOrderAddress));
    hostOrderAddress = ntohl(networkOrderAddress);
    return true;
}
}
