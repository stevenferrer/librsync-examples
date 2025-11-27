#include <boost/asio.hpp>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>

extern "C" {
#include "librsync.h"
}

#include "common.hpp"
#include "librsyncw.hpp"

using namespace boost::asio::ip;

static char in_buf[common::BUFFER_SIZE * 2], out_buf[common::BUFFER_SIZE];

static int recv_sign(tcp::socket &, rsyncw::Sig &sig);
static int send_delta(tcp::socket &, rsyncw::Sig &sig, const char *fname);

int main(int argc, char *argv[]) {
  const char *fname = (argc >= 2) ? argv[1] : nullptr;

  try {
    std::cout << "Waiting for connection...\n";
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), common::PORT));

    tcp::socket socket(io_context);
    acceptor.accept(socket);

    std::cout << "Receiving signature...\n";
    rsyncw::Sig sig;
    auto ret = recv_sign(socket, sig);
    if (ret == -1) {
      return EXIT_FAILURE;
    }

    std::cout << "Sending delta...\n";
    ret = send_delta(socket, sig, fname);
    if (ret == -1) {
      return EXIT_FAILURE;
    }
    std::cout << "Done.\n";
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

static int recv_sign(tcp::socket &sock, rsyncw::Sig &sig) {
  // calculate signature of a file
  rsyncw::Job job;
  job.loadsig_begin(sig);

  // setup buffers
  rs_buffers_t bufs = {0};

  rs_result res;
  do {
    if ((bufs.eof_in == 0) && (bufs.avail_in <= common::BUFFER_SIZE)) {
      if (bufs.avail_in > 0) {
        /* Leftover tail data, move it to front */
        std::memmove(in_buf, bufs.next_in, bufs.avail_in);
      }

      size_t n_bytes;
      int ret = common::recv_message(sock, in_buf + bufs.avail_in, &n_bytes,
                                     &bufs.eof_in);
      if (ret == -1) {
        return -1;
      }

      bufs.next_in = in_buf;
      bufs.avail_in += n_bytes;
    }

    res = job.iter(&bufs);
    if (res != RS_DONE && res != RS_BLOCKED) {
      return -1;
    }

    /* The job should take care of draining the buffers */
  } while (res != RS_DONE);

  return 0;
}

static int send_delta(tcp::socket &sock, rsyncw::Sig &sig, const char *fname) {
  // initialize signature hashtable
  rs_result res = rs_build_hash_table(sig.__sig);
  if (res != RS_DONE) {
    return -1;
  }

  // Open up-to-date file
  std::ifstream infile(fname, std::ios::binary);
  if (!infile.is_open()) {
    std::cerr << "Error opening file\n";
    return -1;
  }

  // calculate delta between a signature and a new file
  rsyncw::Job job;
  job.delta_begin(sig);

  // setup buffers
  rs_buffers_t bufs = {0};
  bufs.next_out = out_buf;
  // We cannot send more in one message
  bufs.avail_out = common::BUFFER_SIZE;

  do {
    // TODO: Understand buffers more
    std::cout << "avail_in: " << bufs.avail_in
              << ", avail_out: " << bufs.avail_out
              << ", eof_in: " << bufs.eof_in << "\n";

    if ((bufs.eof_in == 0) && (bufs.avail_in < sizeof(in_buf))) {
      if (bufs.avail_in > 0) {
        // Left over tail data, move to front
        std::memmove(in_buf, bufs.next_in, bufs.avail_in);
      }

      // fill input buffer
      infile.read(in_buf + bufs.avail_in, sizeof(in_buf) - bufs.avail_in);
      auto n_bytes = infile.gcount();
      if (n_bytes == 0) {
        if (infile.fail() && !infile.eof()) {
          std::cerr << "Failed to read file.\n";
          return -1;
        }
        bufs.eof_in = infile.eof();
      }

      bufs.next_in = in_buf;
      bufs.avail_in += n_bytes;
    }

    // Process current iteration
    res = job.iter(&bufs);
    if (res != RS_DONE && res != RS_BLOCKED) {
      return -1;
    }

    // Drain output buffer, if there is data
    assert(bufs.next_out >= out_buf);
    size_t present = (size_t)(bufs.next_out - out_buf);
    if (present > 0 || res == RS_DONE) {
      assert(present <= common::BUFFER_SIZE);
      int ret = common::send_message(sock, out_buf, present,
                                     (res == RS_DONE) ? 1 : 0);
      if (ret == -1) {
        return -1;
      }

      bufs.next_out = out_buf;
      bufs.avail_out = common::BUFFER_SIZE;
    }
  } while (res != RS_DONE);

  return 0;
}
