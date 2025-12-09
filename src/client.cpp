#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <boost/asio.hpp>

#include "common.hpp"
#include "librsyncw.hpp"

using namespace boost;
namespace fs = std::filesystem;

inline const std::string server_ip = "127.0.0.1";

static std::vector<char> in_buf(common::BUFFER_SIZE * 2),
    out_buf(common::BUFFER_SIZE);

static int send_signature(asio::ip::tcp::socket &sock, const fs::path &path);
static int recv_delta_and_patch_file(asio::ip::tcp::socket &sock,
                                     const fs::path &fpath);
static int recv_metadata(asio::ip::tcp::socket &sock, fs::path &fpath);
static int create_file_if_not_exist(const fs::path &fpath);

int main(int argc, char *argv[]) {

  using namespace boost::asio::ip;

  try {
    asio::io_context io_context;
    tcp::socket socket(io_context);
    socket.connect(tcp::endpoint(make_address(server_ip), common::PORT));

    std::cout << "Connected to server at " << server_ip << '\n';

    fs::path fpath;
    std::cout << "Receiving metadata...\n";
    auto ret = recv_metadata(socket, fpath);
    if (ret == -1) {
      std::cout << "Failed.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Create file if not exist...\n";
    ret = create_file_if_not_exist(fpath);
    if (ret == -1) {
      std::cout << "Failed.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Sending signature...\n";
    ret = send_signature(socket, fpath);
    if (ret == -1) {
      std::cout << "Failed.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Receiving delta and patching file...\n";
    ret = recv_delta_and_patch_file(socket, fpath);
    if (ret == -1) {
      std::cout << "Failed.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Done.\n";
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

static int recv_metadata(asio::ip::tcp::socket &sock, fs::path &fpath) {
  std::string msg;
  size_t len;
  int eof;
  auto ret = common::recv_message(sock, msg.data(), &len, &eof);
  if (ret == -1) {
    return -1;
  }

  // (optional) indicate that the file is a copy
  fpath = std::format("(copy) {}", msg.data());

  return 0;
}

static int create_file_if_not_exist(const fs::path &fpath) {
  std::ofstream ofs(fpath);
  if (!ofs.is_open()) {
    std::cerr << "Error opening file.\n";
    return -1;
  }

  return 0;
}

static int send_signature(asio::ip::tcp::socket &sock, const fs::path &fpath) {
  using namespace rsw;

  // open basis file
  std::ifstream infile(fpath, std::ios::binary);
  if (!infile.is_open()) {
    std::cerr << "Error opening file\n";
    return -1;
  }

  // get file size
  rs::rs_long_t fsize = fs::file_size(fpath);

  // get recommended arguments
  auto sig_magic = static_cast<rs::rs_magic_number>(0);
  size_t block_len = 0, strong_len = 0;
  rs::rs_result res =
      rs::rs_sig_args(fsize, &sig_magic, &block_len, &strong_len);
  if (res != rs::RS_DONE) {
    return -1;
  }

  // start generating signature
  rsw::Job job;
  job.sig_begin(block_len, strong_len, sig_magic);

  // setup buffers
  rs::rs_buffers_t bufs = {0};
  bufs.next_out = out_buf.data();
  // We cannot send more in one message
  bufs.avail_out = common::BUFFER_SIZE;

  size_t tot_bytes_sent = 0;

  // generate signature
  do {
    if ((bufs.eof_in == 0) && (bufs.avail_in < sizeof(in_buf))) {
      if (bufs.avail_in > 0) {
        // Leftover tail data, move it to front
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
        // eof reached
        bufs.eof_in = infile.eof();
      }

      bufs.next_in = in_buf.data();
      bufs.avail_in += n_bytes;
    }

    // Process current iteration
    res = job.iter(&bufs);
    if (res != rsw::rs::RS_DONE && res != rsw::rs::RS_BLOCKED) {
      return -1;
    }

    assert(bufs.next_out >= out_buf.data());
    size_t present = (size_t)(bufs.next_out - out_buf.data());
    if (present > 0 || res == rs::RS_DONE) {
      // drain output buffer
      assert(present <= common::BUFFER_SIZE);
      bool eof = (res == rs::RS_DONE) ? 1 : 0;
      int ret = common::send_message(sock, out_buf.data(), present, eof);
      if (ret == -1) {
        return -1;
      }

      tot_bytes_sent += present;
      bufs.next_out = out_buf.data();
      bufs.avail_out = common::BUFFER_SIZE;
    }
  } while (res != rs::RS_DONE);

  std::cout << std::format("Sent {} bytes\n", tot_bytes_sent);

  return 0;
}

static int recv_delta_and_patch_file(asio::ip::tcp::socket &sock,
                                     const fs::path &fpath) {
  using namespace rsw;

  fs::path fpath_new(std::format("{}.tmp", fpath.string()));

  // open new file
  std::ofstream ofs(fpath_new, std::ios::binary);
  if (!ofs.is_open()) {
    std::cerr << "Error opening file\n";
    return -1;
  }

  // use C stdio file for the basis file
  FILE *f_old = rs::rs_file_open(fpath.c_str(), "rb", 0);
  assert(f_old != NULL);

  // start job
  Job job;
  job.patch_begin(f_old);

  // QQ: What's the difference between
  // sizeof(out_buf) and sizeof(out_buf.data())
  rs::rs_buffers_t bufs = {0};
  bufs.next_out = out_buf.data();
  bufs.avail_out = sizeof(out_buf);

  size_t tot_bytes_received = 0;

  rs::rs_result res;
  do {
    if ((bufs.eof_in == 0) && (bufs.avail_in < common::BUFFER_SIZE)) {
      if (bufs.avail_in > 0) {
        // Left over tail data, move to front
        std::memmove(in_buf.data(), bufs.next_in, bufs.avail_in);
      }

      size_t n_bytes;
      int ret = common::recv_message(sock, in_buf.data() + bufs.avail_in,
                                     &n_bytes, &bufs.eof_in);
      if (ret == -1) {
        return -1;
      }

      tot_bytes_received += n_bytes;
      bufs.next_in = in_buf.data();
      bufs.avail_in += n_bytes;
    }

    res = job.iter(&bufs);
    if (res != rs::RS_DONE && res != rs::RS_BLOCKED) {
      return -1;
    }

    // Drain output buffer, if there is data
    assert(bufs.next_out >= out_buf.data());
    size_t present = (size_t)(bufs.next_out - out_buf.data());
    if (present > 0) {
      try {
        ofs.write(out_buf.data(), present);
      } catch (const std::ios_base::failure &e) {
        std::cerr << std::format("Failed to write to file: {}\n", e.what());
        return -1;
      }

      bufs.next_out = out_buf.data();
      bufs.avail_out = sizeof(out_buf);
    }
  } while (res != rsw::rs::RS_DONE);

  std::cout << std::format("Received {} bytes.\n", tot_bytes_received);

  // swap the patched file
  std::error_code ec;
  fs::rename(fpath_new, fpath, ec);
  if (ec) {
    std::cout << std::format(
        "Failed to swap basis file with patched file: {}\n", ec.message());
    return -1;
  }

  return 0;
}