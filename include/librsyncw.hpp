#ifndef LIBRSYNCW_H
#define LIBRSYNCW_H

#include <cassert>
#include <cstddef>
#include <cstdio>

namespace rsw {
  namespace rs {
    extern "C" {
    // FIXME: How to indent this?
#include "librsync.h"
    }
  } // namespace rs

  enum Result { Done = rs::RS_DONE, Blocked = rs::RS_BLOCKED };

  class Sig {
  public:
    // FIXME: ideally, _sig would be private, but for now,
    // let's pretend it is by prefixing with underscore
    // FIXME: use smart pointers?
    rs::rs_signature_t *__sig;
  };

  // class Buffers {
  // public:
  //   Buffers() { bufs = {0}; }

  // private:
  //   rs_buffers_t bufs;
  // };

  class Job {
  public:
    // constructors and destructors
    ~Job() {
      // rs_job_t can own rs_signature_t(sumset),
      // when job_name is "signature", in that case
      // rs_signature is also freed
      rs::rs_job_free(job);
    }

    // public functions

    // FIXME: Is this the best way to do this?
    void loadsig_begin(Sig &sig) {
      // TODO: Make sure job isn't initialized previousy

      job = rs::rs_loadsig_begin(&sig.__sig);
      // FIXME: is this the proper way to do this?
      assert(job != NULL);
    }

    void delta_begin(Sig &sig) {
      // TODO: Make sure job isn't initialized previousy

      job = rs::rs_delta_begin(sig.__sig);
      // FIXME: should we compare against nullptr instead?
      assert(job != NULL);
    }

    void sig_begin(size_t block_len, size_t strong_len,
                   rs::rs_magic_number sig_magic) {
      job = rs::rs_sig_begin(block_len, strong_len, sig_magic);
      assert(job != NULL);
    }

    void patch_begin(FILE *file) {
      job = rs::rs_patch_begin(rs::rs_file_copy_cb, file);
      assert(job != NULL);
    }

    rs::rs_result iter(rs::rs_buffers_t *bufs) {
      rs::rs_result res = rs::rs_job_iter(job, bufs);
      return res;
    }

  private:
    rs::rs_job_t *job;
  };
} // namespace rsw

#endif