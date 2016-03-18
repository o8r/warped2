// -*- C++ -*-
/** Termination status which will be returned from main().
 * @author O'HARA Mamoru
 * @date 2016 Mar 18
 */
#ifndef WARPED_TERMINATIONSTATUS_HPP_
#define WARPED_TERMINATIONSTATUS_HPP_

namespace warped {

enum TerminationStatus {
  TS_NOT_TERMINATED = -1,
  TS_NORMAL = 0, // Success
  TS_PAUSED = 1,  // paused for rejuvenation
  TS_ERROR = 666,  // general error 
};

}

#endif
