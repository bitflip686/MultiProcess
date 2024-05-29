/*
 File: ContFramePool.C
 
 Author: Oliver Carver
 Date  : 02/11/2024
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

 const unsigned int BYTE = 8;
 const unsigned int BYTE_SHIFT = 3;

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool* ContFramePool::head = nullptr;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    next = head;
    head = this;
    
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;

    unsigned long n_info_frames = needed_info_frames(_n_frames);  
    
    // If _info_frame_no is zero then we keep management info in the first
    //frame, else we use the provided frame to keep management info
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }
    
    // Everything ok. Proceed to mark all frame as free.
    for(unsigned long fno = 0; fno < _n_frames; fno++) {
        set_state(fno, FrameState::Free);
    }
    
    // Mark the first frame as being used if it is being used
    if(_info_frame_no == 0) {
        for (unsigned long fno = 0; fno < n_info_frames; fno++) {
            set_state(fno, FrameState::Used);
        }
        nFreeFrames -= n_info_frames;
        set_state(0, FrameState::HoS);
    }
    
    Console::puts("Frame Pool initialized\n");
}

// Possible states
// free = 00
// used = 10
// hos = 11
ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
    // 2 bits so multiply by 2
    unsigned long frame_no = _frame_no * 2;
    // efficient / 8 
    unsigned long bit_slot = frame_no >> BYTE_SHIFT;
    // efficient % 8 
    unsigned char bit_mask_1 = 1 << (frame_no & (BYTE - 1));
    // we stay in the isolated byte because 2n % 2m = 2(n % m), so bit_mask_1 isolates bit 0,2,4,6 
    unsigned char bit_mask_2 = bit_mask_1 << 1;

    return !(bitmap[bit_slot] & bit_mask_1) ? FrameState::Free : (bitmap[bit_slot] & bit_mask_2 ? FrameState::HoS : FrameState::Used);
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{
    unsigned long frame_no = _frame_no * 2;
    unsigned long bit_slot = frame_no >> BYTE_SHIFT;
    unsigned long bit_shift = frame_no & (BYTE - 1);

    // zero out the bits
    bitmap[bit_slot] &= ~(0b11 << bit_shift);

    switch(_state) {
        case FrameState::Free:
            // bits already zeroed so break
            break;
        case FrameState::Used:
            // set first bit
            bitmap[bit_slot] |= 1 << bit_shift;
            break;
        case FrameState::HoS:
            // set both bits
            bitmap[bit_slot] |= 0b11 << bit_shift;
            break;
    }

    return;
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    if (_n_frames > nFreeFrames) {
        Console::puts("ERROR!\n File: cont_frame_pool.c\n Function: get_frames\n Message: Requested ");
        Console::puti(_n_frames);
        Console::puts(" but only ");
        Console::puti(nFreeFrames);
        Console::puts(" available\n");
        return 0;
    }

    unsigned long start = 0;
    unsigned long free = 0;
    
    for (unsigned long fno = 0; fno < nframes; fno++) {
        get_state(fno) == FrameState::Free ? free++ : free = 0;

        if (free == _n_frames) {
            start = fno - free + 1;
            break;
        }
    }

    if (free != _n_frames) {
        Console::puts("ERROR!\n File: cont_frame_pool.c\n Function: get_frames\n Message: No free sequence of frames large enough to hold requested frame amount of ");
        Console::puti(_n_frames);
        Console::puts("\n");
        return 0;
    }

    for (unsigned long fno = start; fno < start + _n_frames; fno++) {
        set_state(fno, FrameState::Used);
    }
    
    set_state(start, FrameState::HoS);
    nFreeFrames -= _n_frames;

    return (start + base_frame_no);
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // Mark all frames in the range as being used.
    for (unsigned long fno = _base_frame_no; fno < _base_frame_no + _n_frames; fno++) {
        set_state(fno - this->base_frame_no, FrameState::Used);
    }
    set_state(_base_frame_no, FrameState::HoS);

    return;
}

void ContFramePool::_release_frames(unsigned long _first_frame_no)
{
    unsigned long fno = _first_frame_no;
    if (get_state(fno) != FrameState::HoS) {
        Console::puts("ERROR!\n File: cont_frame_pool.c\n Function: release_frames\n Message: Attempted release_frames with non-HoS first frame ");
        Console::puti(_first_frame_no);
        Console::puts("\n");
        return;
    }

    // loop until we hit either Free/HoS or we hit the end of this pool
    do {
        set_state(fno++, FrameState::Free);
        nFreeFrames++;
    } while (get_state(fno) == FrameState::Used && fno < nframes);

    return;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    ContFramePool* cursor = head;

    while (cursor) {
        // see if it falls into pools range
        unsigned long adjusted_fno = _first_frame_no - cursor->base_frame_no;
        if (adjusted_fno < cursor->nframes) {
            // pass adjusted index to the pool's release_frames
            cursor->_release_frames(adjusted_fno);
            return;
        } else {
            cursor = cursor->next;
        }
    }

    Console::puts("ERROR!\n File: cont_frame_pool.c\n Function: release_frames\n Message: Could not find for release the frame ");
    Console::puti(_first_frame_no);
    Console::puts("\n");

    return;
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    return _n_frames / INFO_FRAME_CAPACITY + (_n_frames % INFO_FRAME_CAPACITY > 0 ? 1 : 0);
}
