#include "SDK/StorageRecorder.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

std::unique_ptr<StorageRecorder::Session> session;
StorageRecorder::Session& Get(){if(!session)session=std::make_unique<StorageRecorder::Session>();return *session;}

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto stats=FromWire<SR_STATS>(a.at("status"));return {{"valid",StorageRecorder::Valid(stats)},{"value",EncodeWire(stats)}};}
if(op=="disks"){Json result=Json::array();for(const auto& disk:StorageRecorder::Disks())result.push_back({{"instance",Utf8(disk.instance)},{"name",Utf8(disk.name)},{"pdo",Utf8(disk.pdo)},{"number",disk.number},{"eligible",disk.eligible},{"attached",disk.attached},{"reason",Utf8(disk.reason)}});return result;}
if(op=="devices")return VectorWire(Get().Devices());
if(op=="start"){Get().Start(Identifier(a,"device_id"));return Json::object();}
if(op=="stop"){if(session)session->Stop();return Json::object();}
if(op=="status")return EncodeWire(Get().Status());
if(op=="read")return EncodeWire(Get().Read());
if(op=="duration"){auto event=FromWire<SR_EVENT>(a.at("event"));return {{"microseconds",StorageRecorder::Microseconds(event,Identifier(a,"frequency"))}};}

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned storage_io_flight_recorder_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned storage_io_flight_recorder_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* storage_io_flight_recorder_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void storage_io_flight_recorder_json_free(char* response)noexcept{delete[] response;}
