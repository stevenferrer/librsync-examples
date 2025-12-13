#include <cstdlib>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>

#include <boost/asio.hpp>

#include "common.hpp"
#include "librsyncw.hpp"

using namespace boost;
namespace fs = std::filesystem;

static std::vector<char> in_buf(common::BUFFER_SIZE * 2),
    out_buf(common::BUFFER_SIZE);

static int recv_sign(asio::ip::tcp::socket &, rsw::Sig &sig);
static int send_delta(asio::ip::tcp::socket &, rsw::Sig &sig,
                      const fs::path &fpath);
static int send_metadata(asio::ip::tcp::socket &, const fs::path &fpath);

int main(int argc, char *argv[]) {
  const fs::path fpath = (argc >= 2) ? argv[1] : nullptr;

  using namespace boost::asio::ip;

  try {
    std::cout << "Waiting for connection...\n";

    asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), common::PORT));

    tcp::socket socket(io_context);
    acceptor.accept(socket);

    std::cout << "Sending metadata...\n";
    auto ret = send_metadata(socket, fpath);
    if (ret == -1) {
      std::cout << "Failed.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Receiving signature...\n";
    rsw::Sig sig;
    ret = recv_sign(socket, sig);
    if (ret == -1) {
      std::cout << "Failed.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Sending delta...\n";
    ret = send_delta(socket, sig, fpath);
    if (ret == -1) {
      std::cout << "Failed.\n";
      return EXIT_FAILURE;
    }
    std::cout << "Done.\n";
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

static int send_metadata(asio::ip::tcp::socket &sock, const fs::path &fpath) {
  auto s = fpath.string();
  return common::send_message(sock, s.c_str(), s.length(), 1);
}

static int recv_sign(asio::ip::tcp::socket &sock, rsw::Sig &sig) {
  using namespace rsw;

  // calculate signature of a file
  Job job;
  job.loadsig_begin(sig);

  // setup buffers
  rs::rs_buffers_t bufs = {0};

  rs::rs_result res;
  do {
    if ((bufs.eof_in == 0) && (bufs.avail_in <= common::BUFFER_SIZE)) {
      if (bufs.avail_in > 0) {
        /* Leftover tail data, move it to front */
        std::memmove(in_buf.data(), bufs.next_in, bufs.avail_in);
      }

      size_t n_bytes;
      int ret = common::recv_message(sock, in_buf.data() + bufs.avail_in,
                                     n_bytes, bufs.eof_in);
      if (ret == -1) {
        return -1;
      }

      bufs.next_in = in_buf.data();
      bufs.avail_in += n_bytes;
    }

    res = job.iter(&bufs);
    if (res != rs::RS_DONE && res != rs::RS_BLOCKED) {
      return -1;
    }

    // The job should take care of draining the buffers
  } while (res != rs::RS_DONE);

  return 0;
}

static int send_delta(asio::ip::tcp::socket &sock, rsw::Sig &sig,
                      const fs::path &fpath) {
  using namespace rsw;

  // initialize signature hashtable
  rs::rs_result res = rs::rs_build_hash_table(sig.__sig);
  if (res != rsw::rs::RS_DONE) {
    return -1;
  }

  // Open up-to-date file
  std::ifstream infile(fpath, std::ios::binary);
  if (!infile.is_open()) {
    std::cerr << "Error opening file\n";
    return -1;
  }

  // calculate delta between a signature and a new file
  Job job;
  job.delta_begin(sig);

  // setup buffers
  rs::rs_buffers_t bufs = {0};
  bufs.next_out = out_buf.data();
  // We cannot send more in one message
  bufs.avail_out = common::BUFFER_SIZE;

  do {
    // TODO: Understand buffers more
    // std::cout << "avail_in: " << bufs.avail_in
    //           << ", avail_out: " << bufs.avail_out
    //           << ", eof_in: " << bufs.eof_in << "\n";

    if ((bufs.eof_in == 0) && (bufs.avail_in < sizeof(in_buf))) {
      if (bufs.avail_in > 0) {
        // Left over tail data, move to front
        std::memmove(in_buf.data(), bufs.next_in, bufs.avail_in);
      }

      // fill input buffer
      infile.read(in_buf.data() + bufs.avail_in,
                  sizeof(in_buf) - bufs.avail_in);
      auto n_bytes = infile.gcount();
      if (n_bytes == 0) {
        if (infile.fail() && !infile.eof()) {
          std::cerr << "Failed to read file.\n";
          return -1;
        }
        bufs.eof_in = infile.eof();
      }

      bufs.next_in = in_buf.data();
      bufs.avail_in += n_bytes;
    }

    // Process current iteration
    res = job.iter(&bufs);
    if (res != rs::RS_DONE && res != rs::RS_BLOCKED) {
      return -1;
    }

    // Drain output buffer, if there is data
    assert(bufs.next_out >= out_buf.data());
    size_t present = (size_t)(bufs.next_out - out_buf.data());
    if (present > 0 || res == rs::RS_DONE) {
      assert(present <= common::BUFFER_SIZE);
      int ret = common::send_message(sock, out_buf.data(), present,
                                     (res == rs::RS_DONE) ? 1 : 0);
      if (ret == -1) {
        return -1;
      }

      bufs.next_out = out_buf.data();
      bufs.avail_out = common::BUFFER_SIZE;
    }
  } while (res != rs::RS_DONE);

  return 0;
}
