#ifndef __DEBUG_H__
#define __DEBUG_H__

#if __MP_DEBUG_BUILD__
#include <cstdio>
    #define DBG_ERR(fmt,args...)    fprintf(stderr, "[Err %s#%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
    #define DBG_LOG(fmt,args...)    fprintf(stdout, "[Log %s#%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#else
    #define DBG_ERR(fmt,args...)
    #define DBG_LOG(fmt,args...)
#endif //__MP_DEBUG_BUILD__

#endif //__DEBUG_H__