#ifndef LIBRSYNCW_H
#define LIBRSYNCW_H

#include <cassert>

namespace rsyncw {
  extern "C" {
#include "librsync.h"
  }

  enum Result { Done = RS_DONE, Blocked = RS_BLOCKED };

  class Sig {
  public:
    // FIXME: ideally, _sig would be private,
    // let's pretend it is by using underscore
    rs_signature_t *__sig;
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
      rs_job_free(job);
    }

    // public functions

    // FIXME: Is this the best way to do this?
    void loadsig_begin(Sig &sig) {
      // TODO: Make sure job isn't initialized previousy

      job = rs_loadsig_begin(&sig.__sig);
      // FIXME: is this the proper way to do this?
      assert(job != NULL);
    }

    void delta_begin(Sig &sig) {
      // TODO: Make sure job isn't initialized previousy

      job = rs_delta_begin(sig.__sig);
      // FIXME: should we compare against nullptr instead?
      assert(job != NULL);
    }

    rs_result iter(rs_buffers_t *bufs) {
      rs_result res = rs_job_iter(job, bufs);
      return res;
    }

  private:
    rs_job_t *job;
  };
} // namespace rsyncw

#endif