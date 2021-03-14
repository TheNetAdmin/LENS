#ifndef PRINT_INFO_H
#define PRINT_INFO_H

#include <stdio.h>

#define kr_info(string, args...)                                               \
	do {                                                                   \
		printf(string "\n", ##args);                                   \
	} while (0)

#ifdef NVRAM_COVERT_DEBUG_PRINT
	#define debug_printf(args...) do { printf(##args); } while(0)
#else
	#define debug_printf(args...) do {} while(0)
#endif /* NVRAM_COVERT_DEBUG_PRINT */

#endif /* PRINT_INFO_H */
