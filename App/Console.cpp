#include "../SDK/StorageRecorder.h"
#include <iostream>
#include <iomanip>
int wmain(int argc,wchar_t** argv){try{
 if(argc==2&&wcscmp(argv[1],L"--disks")==0){for(const auto& d:StorageRecorder::Disks())std::wcout<<L"Disk "<<d.number<<L" | "<<d.name<<L"\n"<<d.instance<<L"\n"<<d.reason<<(d.attached?L" [recorder attached]":L"")<<L"\n\n";return 0;}
 if(argc==2&&wcscmp(argv[1],L"--preview")==0){std::cout<<"Storage I/O Flight Recorder - offline preview\nNo driver opened, no disk access, no synthetic values presented as measurements.\n";return 0;}
 if(argc==4&&wcscmp(argv[1],L"--record")==0){
  size_t used=0;auto id=std::stoull(argv[2],&used);if(used!=wcslen(argv[2])||!id)throw std::invalid_argument("Invalid device ID.");
  auto seconds=std::stoul(argv[3],&used);if(used!=wcslen(argv[3])||!seconds||seconds>3600)throw std::invalid_argument("Duration must be 1-3600 seconds.");
  StorageRecorder::Session recorder;recorder.Start(id);std::cout<<"sequence,operation,offset,requested,transferred,status,duration_us,inflight_at_start\n";
  for(auto end=GetTickCount64()+seconds*1000;GetTickCount64()<end;Sleep(20)){
   auto b=recorder.Read();for(unsigned i=0;i<b.Count;++i){auto& e=b.Events[i];std::cout<<e.Sequence<<','<<e.Operation<<','<<e.ByteOffset<<','<<e.RequestedBytes<<','<<e.TransferredBytes<<','<<e.Status<<','<<StorageRecorder::Microseconds(e,b.Stats.Frequency)<<','<<e.InflightAtStart<<'\n';}
  }
  auto s=recorder.Status();std::cerr<<"completed="<<s.Completed<<" overwritten_logs="<<s.Dropped<<"\n";return 0;
 }
 if(argc==2&&wcscmp(argv[1],L"--devices")==0){StorageRecorder::Session recorder;for(const auto& d:recorder.Devices())std::wcout<<d.Id<<L" "<<reinterpret_cast<const wchar_t*>(d.PdoName)<<L'\n';return 0;}
 std::cout<<"Usage: StorageRecorderCLI --disks | --devices | --record <device-id> <seconds> | --preview\nPreview attachment is limited to the owned Titan Storage Fault Lab disk. Other disks are read-only inventory. Metadata timestamps are at this filter boundary, not hardware or application end-to-end latency.\n";return argc==1?0:2;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
