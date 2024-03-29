#ifndef _CHASING_CONFIGS_H_
#define _CHASING_CONFIGS_H_

#include <stdint.h>

#ifndef CHASING_FENCE_STRATEGY_ID
#define CHASING_FENCE_STRATEGY_ID 0
#endif

#ifndef CHASING_FENCE_FREQ_ID
#define CHASING_FENCE_FREQ_ID 1
#endif

#ifndef CHASING_FLUSH_AFTER_LOAD
#define CHASING_FLUSH_AFTER_LOAD 1
#endif

#ifndef CHASING_FLUSH_AFTER_STORE
#define CHASING_FLUSH_AFTER_STORE 1
#endif

#ifndef CHASING_FLUSH_L1
#define CHASING_FLUSH_L1 0
#endif

#ifndef CHASING_RECORD_TIMING
#define CHASING_RECORD_TIMING 1
#endif

#ifndef COVERT_CHASING_STORE
#define COVERT_CHASING_STORE 0
#endif

#ifndef COVERT_CHASING_LOAD
#define COVERT_CHASING_LOAD 1
#endif

#endif /* _CHASING_CONFIGS_H_ */
