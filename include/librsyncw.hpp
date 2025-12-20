#ifndef LIBRSYNCW_H
#define LIBRSYNCW_H

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <memory>

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
  public:
    // job unique ptr alias
    typedef std::unique_ptr<Job> Ptr;

    ~Job() {
      // rs_job_t can own rs_signature_t(sumset),
      // when job_name is "signature", in that case
      // rs_signature is also freed
      rs::rs_job_free(job);
    }

    rs::rs_result iter(rs::rs_buffers_t *bufs) {
      rs::rs_result res = rs::rs_job_iter(job, bufs);
      return res;
    }

    // job factory methods

    static Ptr loadsig_begin(Signature &sig) {
      auto job = rs::rs_loadsig_begin(&sig.sig);
      assert(job != NULL);

      auto j = std::unique_ptr<Job>(new Job(job));
      return j;
    }

    static Ptr delta_begin(Signature &sig) {
      auto job = rs::rs_delta_begin(sig.sig);
      assert(job != NULL);

      auto j = std::unique_ptr<Job>(new Job(job));
      return j;
    }

    static Ptr sig_begin(size_t block_len, size_t strong_len,
                         rs::rs_magic_number sig_magic) {
      auto job = rs::rs_sig_begin(block_len, strong_len, sig_magic);
      assert(job != NULL);

      auto j = std::unique_ptr<Job>(new Job(job));
      return j;
    }

    static Ptr patch_begin(FILE *file) {
      auto job = rs::rs_patch_begin(rs::rs_file_copy_cb, file);
      assert(job != NULL);

      auto j = std::unique_ptr<Job>(new Job(job));
      return j;
    }

  private:
    rs::rs_job_t *job;

    // private so we cannot stack allocate, only thru factory methods
    Job(rs::rs_job_t *job) : job{job} {};
  };
} // namespace rsw

#endif