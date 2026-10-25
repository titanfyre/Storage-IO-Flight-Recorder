#include "../SDK/StorageRecorder.h"
#include <iostream>
#include <stdexcept>
#include <random>
void Require(bool b){if(!b)throw std::runtime_error("Offline assertion failed");}
int main(){try{
 SR_REQUEST request{SR_VERSION,sizeof(request),4};Require(SR_ValidRequest(&request));request.Size++;Require(!SR_ValidRequest(&request));
 SR_STATS stats{};stats.Version=SR_VERSION;stats.Size=sizeof(stats);stats.DeviceId=1;stats.Frequency=10000000;Require(StorageRecorder::Valid(stats));
 SR_BATCH batch{};batch.Stats=stats;Require(StorageRecorder::Valid(batch));batch.Count=1;batch.Events[0]={1,100,130,512,512,512,0,SR_OP_READ,1,0};batch.Stats.Started=batch.Stats.Completed=1;Require(StorageRecorder::Valid(batch));Require(StorageRecorder::Microseconds(batch.Events[0],10000000)==3.0);
 batch.Events[0].CompletedQpc=99;Require(!StorageRecorder::Valid(batch));batch.Events[0].CompletedQpc=130;
 std::mt19937 random(1234);for(unsigned i=0;i<1000000;++i){auto corrupt=batch;switch(i%6){case 0:corrupt.Stats.Size=random()|0x80000000;break;case 1:corrupt.Count=SR_READ+1+(random()%1024);break;case 2:corrupt.Events[0].Reserved=random()|1;break;case 3:corrupt.Events[0].Operation=4+(random()%100);break;case 4:corrupt.Events[0].Sequence=0;break;default:corrupt.Stats.Completed=corrupt.Stats.Started+1;}Require(!StorageRecorder::Valid(corrupt));}
 SR_INVENTORY inventory{};inventory.Version=SR_VERSION;inventory.Size=sizeof(inventory);inventory.Count=1;inventory.Devices[0].Id=1;Require(StorageRecorder::Valid(inventory));inventory.Count=2;inventory.Devices[1].Id=1;Require(!StorageRecorder::Valid(inventory));
 std::cout<<"Offline protocol, timestamps and one million malformed-record cases passed. No disk was opened or modified.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
