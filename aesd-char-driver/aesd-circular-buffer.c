/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    struct aesd_buffer_entry *return_buffer_entry = NULL;
    uint8_t index = buffer->out_offs; 
    struct aesd_buffer_entry *entryptr;
    size_t current_offset = 0;
    uint8_t count = 0;
    //Start loop at index=out_offs and if max index reached and loop is still on then restart index at 0
    //Using count as an isolated counter to end the loop if all elements are iterated
    while(count < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
        if(index >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
            index = 0;
        }
        entryptr=&((buffer)->entry[index]);
        current_offset += entryptr->size;
        if(current_offset > char_offset){
            return_buffer_entry = entryptr;
            *entry_offset_byte_rtn = char_offset - (current_offset - entryptr->size);
            break;
        }
        index++;
        count++;
    }
    return return_buffer_entry;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    // Add a new entry at the value of in_offs
    buffer->entry[buffer->in_offs] = *add_entry;
    // Increment the value of in_offs
    buffer->in_offs++;
    // if the value reached the max already then restart at 0 again
    if(buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
        buffer->in_offs = 0;
    }
    
    //after in_offs has a new value in_offs == out_offs then the buffer is now full
    if(buffer->in_offs == buffer->out_offs){
        buffer->full = true;
    }
    //buffer was already full then we need to move out_offs to be equal in_offs
    else if(buffer->full && buffer->in_offs > buffer->out_offs){
        buffer->out_offs = buffer->in_offs;
    }

}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
