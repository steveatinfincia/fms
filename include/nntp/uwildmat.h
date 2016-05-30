#ifndef _uwildmat_
#define _uwildmat_

#ifdef _WIN32
	#if (_MSC_VER >= 1700)
		#include <stdint.h>
	#else
		#include "../pstdint.h"
	#endif
#endif

/*
**  WILDMAT MATCHING
*/
enum uwildmat {
    UWILDMAT_FAIL   = 0,
    UWILDMAT_MATCH  = 1,
    UWILDMAT_POISON
};

bool uwildmat(const char *text, const char *pat);

#endif	// _uwildmat_
