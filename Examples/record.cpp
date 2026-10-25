#include "../SDK/StorageRecorder.h"
#include <iostream>
int main(){try{
 std::cout<<"Record read/write metadata from an explicitly attached Titan Storage Fault Lab disk for 10 seconds. No disk data is read by this example.\n";
 StorageRecorder::Session recorder;auto devices=recorder.Devices();
 if(devices.empty()){std::cout<<"Attach a disposable scratch disk with the signed installer first.\n";return 1;}
 recorder.Start(devices.front().Id);
 for(auto end=GetTickCount64()+10000;GetTickCount64()<end;Sleep(20))
  for(auto batch=recorder.Read();batch.Count;batch=recorder.Read())
   for(unsigned i=0;i<batch.Count;++i){const auto& e=batch.Events[i];std::cout<<e.Sequence<<" op="<<e.Operation<<" bytes="<<e.TransferredBytes<<" duration_us="<<StorageRecorder::Microseconds(e,batch.Stats.Frequency)<<" status="<<e.Status<<'\n';}
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
