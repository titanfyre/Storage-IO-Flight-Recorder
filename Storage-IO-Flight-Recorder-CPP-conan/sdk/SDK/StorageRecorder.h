#pragma once
typedef unsigned long SR_U32;
typedef signed long SR_I32;
typedef unsigned short SR_U16;
typedef unsigned __int64 SR_U64;
#define SR_VERSION 1u
#define SR_DEVICES 16u
#define SR_NAME 96u
#define SR_RING 256u
#define SR_READ 32u
#define SR_IOCTL(n) ((0x8000u<<16)|(3u<<14)|((n)<<2))
#define SR_ENUMERATE SR_IOCTL(0x850u)
#define SR_START SR_IOCTL(0x851u)
#define SR_STOP SR_IOCTL(0x852u)
#define SR_STATUS SR_IOCTL(0x853u)
#define SR_RECEIVE SR_IOCTL(0x854u)
#define SR_OP_READ 1u
#define SR_OP_WRITE 2u
typedef struct SR_REQUEST { SR_U32 Version,Size; SR_U64 DeviceId; } SR_REQUEST;
typedef struct SR_DEVICE { SR_U64 Id; SR_U32 Flags,Reserved; SR_U16 PdoName[SR_NAME]; } SR_DEVICE;
typedef struct SR_INVENTORY { SR_U32 Version,Size,Count,Reserved; SR_DEVICE Devices[SR_DEVICES]; } SR_INVENTORY;
typedef struct SR_EVENT {
 SR_U64 Sequence,StartedQpc,CompletedQpc,ByteOffset;
 SR_U64 RequestedBytes,TransferredBytes;
 SR_I32 Status; SR_U32 Operation,InflightAtStart,Reserved;
} SR_EVENT;
typedef struct SR_STATS {
 SR_U32 Version,Size,Active,Available;
 SR_U64 DeviceId,Generation,Frequency,Started,Completed,Errors,Dropped,ReadBytes,WriteBytes,Inflight,PeakInflight;
} SR_STATS;
typedef struct SR_BATCH { SR_STATS Stats; SR_U32 Count,Reserved[3]; SR_EVENT Events[SR_READ]; } SR_BATCH;
static __inline int SR_ValidRequest(const SR_REQUEST* r) {return r->Version==SR_VERSION&&r->Size==sizeof(*r);}
#ifndef _KERNEL_MODE
#include <windows.h>
#include <memory>
#include <string>
#include <vector>
static_assert(sizeof(SR_REQUEST)==16&&sizeof(SR_EVENT)==64&&sizeof(SR_STATS)==104,"Unexpected protocol packing");
namespace StorageRecorder {
struct Disk {std::wstring instance,name,pdo;unsigned number;bool eligible,attached;std::wstring reason;};
std::vector<Disk> Disks(); // Read-only inventory; eligibility is rechecked by setup.
bool Valid(const SR_STATS&) noexcept;
bool Valid(const SR_BATCH&) noexcept;
bool Valid(const SR_INVENTORY&) noexcept;
double Microseconds(const SR_EVENT&,SR_U64 frequency) noexcept;
std::string Utf8(const std::wstring&);
class Session {
 struct State;std::unique_ptr<State> state_;
 void Command(unsigned,const void*,DWORD,void*,DWORD);
public:
 Session();~Session();Session(const Session&)=delete;Session& operator=(const Session&)=delete;
 std::vector<SR_DEVICE> Devices();
 void Start(SR_U64 deviceId);
 SR_STATS Status();
 SR_BATCH Read();
 void Stop() noexcept;
};
}
#endif
