#ifndef COMMON_H
#define COMMON_H

#include <cerrno>
#include <cstdint>
#include <iostream>

#include <boost/asio.hpp>

/*
 * Simple network protocol on top of TCP. Used for client-server communication
 * in the Stream API example code.
 *
 * Header format:
 *   +----------+----------+----------|
 *   | SDU Len. | Reserved | EoF Flag |
 *   +----------+----------+----------+
 *   | 12 bits  | 3 bits   | 1 bit    |
 *   +----------+----------+----------|
 *
 * The header fields are defined as follows:
 * SDU Length           Length of the SDU (i.e. payload) encapsulated within
 *                      this datagram.
 * Reserved             3 bits reserved for future use.
 * End-of-File Flag     Determines whether or not the receiver should expect
 *                      to receive more datagrams.
 * */

namespace common {

  using namespace boost;

  const int PORT = 5612;
  const int BUFFER_SIZE = 4095;

  int send_message(asio::ip::tcp::socket &sock, const char *msg, size_t len,
                   int eof) {
    using namespace boost::asio::detail;

    assert(len <= BUFFER_SIZE);

    uint16_t header = (uint16_t)len << 4;

    // set eof flag
    if (eof != 0) {
      header |= 1;
    }

    header = socket_ops::host_to_network_short(header);
    size_t n_bytes = 0;

    // send header
    boost::system::error_code error;
    do {
      auto ret = asio::write(
          sock, asio::buffer(&header + n_bytes, sizeof(header) - n_bytes),
          error);
      if (error) {
        if (error == asio::error::interrupted) {
          continue;
        }

        std::cerr << "Failed to send message header.\n";
        throw boost::system::system_error(error);
      }

      n_bytes += ret;
    } while (n_bytes < sizeof(header));

    if (len > 0) {
      // send payload
      n_bytes = 0;

      do {
        auto ret = asio::write(sock, asio::buffer(msg + n_bytes, len - n_bytes),
                               error);

        if (error) {
          if (error == asio::error::interrupted) {
            continue;
          }

          std::cerr << "Failed to send message header.\n";
          throw boost::system::system_error(error);
        }

        n_bytes += ret;
      } while (n_bytes < len);
    }

    return 0;
  }

  int recv_message(asio::ip::tcp::socket &sock, char *msg, size_t *len,
                   int *eof) {
    using namespace boost::asio::detail;

    uint16_t header;
    size_t n_bytes = 0;

    boost::system::error_code error;
    do {
      auto ret = asio::read(
          sock,
          boost::asio::buffer(&header + n_bytes, sizeof(header) - n_bytes),
          error);
      if (error) {
        if (error == asio::error::interrupted) {
          continue;
        }

        std::cerr << "Failed to receive message header.\n";
        throw boost::system::system_error(error);
      }

      n_bytes += ret;
    } while (n_bytes < sizeof(header));

    header = socket_ops::network_to_host_short(header);

    // eof flag
    *eof = header & 1;

    // message len
    *len = header >> 4;

    if (*len > 0) {
      // read payload
      n_bytes = 0;
      do {
        auto ret = asio::read(sock, asio::buffer(msg + n_bytes, *len - n_bytes),
                              error);
        if (error) {
          if (error == asio::error::interrupted) {
            continue;
          }

          std::cerr << "Failed to receive message payload.\n";
          throw boost::system::system_error(error);
        }
        n_bytes += ret;
      } while (n_bytes < *len);
    }

    return 0;
  }
} // namespace common

#endif