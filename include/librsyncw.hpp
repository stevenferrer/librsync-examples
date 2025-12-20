#ifndef LIBRSYNCW_H
#define LIBRSYNCW_H

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <utility>

namespace rsw {
  namespace rs {
    extern "C" {
    // FIXME: How to indent this?
#include "librsync.h"
    }
  } // namespace rs

  enum Result { Done = rs::RS_DONE, Blocked = rs::RS_BLOCKED };

  class Signature {
  private:
    // TODO: smart ptr here also?
    rs::rs_signature_t *sig;

  public:
    // allow Job class to access member sig
    friend class Job;

    ~Signature() { rs::rs_free_sumset(sig); }

    rs::rs_result build_hash_table() {
      auto res = rs::rs_build_hash_table(sig);
      return res;
    }
  };

  class Job {
  private:
    using JobPtr =
        std::unique_ptr<rs::rs_job_t, std::function<void(rs::rs_job_t *)>>;

    JobPtr jptr;

    // private so we cannot stack allocate, only thru factory methods
    explicit Job(JobPtr &jptr) : jptr{std::move(jptr)} {};

    static Job make_job(rs::rs_job_t *j) {
      JobPtr jptr(j, rs::rs_job_free);
      return Job{jptr};
    }

  public:
    rs::rs_result iter(rs::rs_buffers_t *bufs) {
      rs::rs_result res = rs::rs_job_iter(jptr.get(), bufs);
      return res;
    }

    // job factory methods

    static Job loadsig_begin(Signature &sig) {
      auto job = rs::rs_loadsig_begin(&sig.sig);
      assert(job != NULL);

      return make_job(job);
    }

    static Job delta_begin(Signature &sig) {
      auto job = rs::rs_delta_begin(sig.sig);
      assert(job != NULL);

      return make_job(job);
    }

    static Job sig_begin(size_t block_len, size_t strong_len,
                         rs::rs_magic_number sig_magic) {
      auto job = rs::rs_sig_begin(block_len, strong_len, sig_magic);
      assert(job != NULL);

      return make_job(job);
    }

    static Job patch_begin(FILE *file) {
      auto job = rs::rs_patch_begin(rs::rs_file_copy_cb, file);
      assert(job != NULL);

      return make_job(job);
    }
  };
} // namespace rsw

#endif