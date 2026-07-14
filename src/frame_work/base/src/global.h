#ifndef DAQ_GLOBAL_H
#define DAQ_GLOBAL_H

#include"build_cfg.h"

enum{
	NO_ERR,
    INIT_ERROR,
    WRONG_PARAMS,
    WRONG_DATA,
    NULL_POINTER,
    SOME_ERROR
};

using pack_id_t = int;
using msg_id_t = int;
#define  PACK_ID_TYPE_BIT_SIZE  (sizeof(pack_id_t)*8)
#define  MSG_ID_TYPE_BIT_SIZE    (sizeof(msg_id_t)*8)
/* Maximum positive value of the underlying signed integer type (INT_MAX equivalent).
 * NOTE: pack_id_t / msg_id_t MUST be a signed integer scalar (int, int32_t, int64_t …).
 *       These macros are NOT valid if the typedef is changed to a struct or unsigned type. */
#define  PKT_ID_INVALID     ((pack_id_t)(~((pack_id_t)1 << (PACK_ID_TYPE_BIT_SIZE - 1))))
#define  MSG_ID_INVALID     ((msg_id_t)(~((msg_id_t)1 << (MSG_ID_TYPE_BIT_SIZE - 1))))


/* Calculate number of bytes needed for X bits */
#define BITS_TO_BYTES_CEIL(x) (( (x)/8 ) + (( (x)%8 )?1:0))
#define BIT_MASK_BEFORE_BIT_U8( x )  (( ( 1 << (x) ) - 1  )&0xff)
#define BIT_MASK_AFTER_BIT_U8( x )   ((~BIT_MASK_BEFORE_BIT_U8( (x) ))&0xff)

#define MASK_OFF_LEN( offset,len )   ((((1<<(len))-1)&0xff) << (offset) )

/* src_off,dest_off should be 0:7
   bitnum 1:8
*/
#define MOVE_SRC_TO_DEST_BITS_IN_BYTE( src,dest,src_off,dest_off,bitnum )  if( bitnum ){\
    dest &= ( ~MASK_OFF_LEN( (dest_off), (bitnum) ) );/*Clear destination bits*/\
    dest |= ( ( (src)&MASK_OFF_LEN( (src_off), (bitnum) )<<(src_off) ) >> (dest_off) );\
    }

#endif
