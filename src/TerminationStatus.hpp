// -*- C++ -*-
/** Termination status which will be returned from main().
 * @author O'HARA Mamoru
 * @date 2016 Mar 18
 */
#ifndef WARPED_TERMINATIONSTATUS_HPP_
#define WARPED_TERMINATIONSTATUS_HPP_

namespace warped {

enum TerminationStatus {
  TS_NORMAL = -1, // Success
  TS_NOT_TERMINATED = 0,
  TS_PAUSED = EINTR,  // paused for rejuvenation
  TS_ERROR = EINVAL,  // general error 
};

}

#endif
