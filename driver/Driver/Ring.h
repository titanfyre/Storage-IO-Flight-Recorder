#pragma once
#include "StorageRecorder.h"
typedef struct SR_RING_BUFFER { SR_EVENT Events[SR_RING];SR_U32 Head,Count;SR_U64 Dropped; } SR_RING_BUFFER;
static __inline void SR_Push(SR_RING_BUFFER* ring,const SR_EVENT* event) {
 SR_U32 index=(ring->Head+ring->Count)%SR_RING;
 if(ring->Count==SR_RING){ring->Head=(ring->Head+1)%SR_RING;++ring->Dropped;}else ++ring->Count;
 ring->Events[index]=*event;
}
static __inline SR_U32 SR_Pop(SR_RING_BUFFER* ring,SR_EVENT* events,SR_U32 maximum) {
 SR_U32 i,count=ring->Count<maximum?ring->Count:maximum;
 for(i=0;i<count;++i)events[i]=ring->Events[(ring->Head+i)%SR_RING];
 ring->Head=(ring->Head+count)%SR_RING;ring->Count-=count;return count;
}
