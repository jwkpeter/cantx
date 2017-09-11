#include "cansocket.h"


#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <cstring>


void can::socket::open(const std::string& device, time_t timeout)
{
  if (fd_ != -1)
    throw socket_error{"Already open"};

  fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);

  if (fd_ == -1)
    throw socket_error{"Could not open"};

  if (device.size() + 1 >= IFNAMSIZ)
    throw socket_error{"Device name too long"};

  addr_.can_family = AF_CAN;
  ifreq ifr;
  std::memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
  std::strcpy(ifr.ifr_name, device.c_str());

  if (ioctl(fd_, SIOCGIFINDEX, &ifr) < 0)
    throw socket_error{"Error retrieving interface index"};
  addr_.can_ifindex = ifr.ifr_ifindex;

  if (timeout > 0) {
    timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
      throw socket_error{"Error setting receive timeout"};
  }

  if (bind(fd_, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_)) < 0)
    throw socket_error{"Error while binding socket"};

  msg_.msg_name = &addr_;
  msg_.msg_iov = &iov_;
  msg_.msg_iovlen = 1;
  msg_.msg_control = nullptr;
}


void can::socket::close()
{
  if (fd_ != -1) {
    ::close(fd_);
    reset();
  }
}


int can::socket::transmit(const can_frame* frame)
{
  return write(fd_, frame, sizeof(*frame));
}


int can::socket::receive(can_frame* frame)
{
  iov_.iov_base = frame;
  iov_.iov_len = sizeof(can_frame);
  msg_.msg_namelen = sizeof(addr_);
  msg_.msg_controllen = 0;
  msg_.msg_flags = 0;

  return recvmsg(fd_, &msg_, 0);
}


void can::socket::reset()
{
  fd_ = -1;
  addr_ = sockaddr_can{};
  iov_ = iovec{};
  msg_ = msghdr{};
}