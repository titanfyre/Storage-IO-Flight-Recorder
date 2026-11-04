#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include "StorageRecorder.h"
#include "Ring.h"

typedef struct _FILTER {
 WDFDEVICE Device; SR_U64 Id,Epoch;BOOLEAN Registered,Counted;
 SR_U16 PdoName[SR_NAME];SR_STATS Stats;SR_RING_BUFFER Ring;
} FILTER;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILTER,FilterContext)
typedef struct _TRACK {BOOLEAN Observed;SR_U64 Epoch;SR_EVENT Event;} TRACK;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TRACK,TrackContext)
static WDFSPINLOCK StateLock;
static WDFWAITLOCK LifecycleLock;
static FILTER* Filters[SR_DEVICES];
static FILTER* volatile Active;
static WDFDEVICE Control;
static ULONG DeviceCount;
static BOOLEAN OwnerOpen;
static FILTER* CurrentActive(VOID) {return (FILTER*)InterlockedCompareExchangePointer((PVOID volatile*)&Active,NULL,NULL);}
static SR_U64 NextId,Frequency;
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD AddDevice;
EVT_WDF_OBJECT_CONTEXT_CLEANUP FilterCleanup;
EVT_WDF_IO_QUEUE_IO_DEFAULT Forward;
EVT_WDF_REQUEST_COMPLETION_ROUTINE Completed;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL Command;
EVT_WDF_DEVICE_FILE_CREATE Open;
EVT_WDF_FILE_CLEANUP Close;

static FILTER* Find(SR_U64 id) {
 ULONG i;for(i=0;i<SR_DEVICES;++i)if(Filters[i]&&Filters[i]->Id==id)return Filters[i];return NULL;
}
static VOID Snapshot(FILTER* f,SR_STATS* output) {
 *output=f->Stats;output->Active=(CurrentActive()==f);output->Available=f->Ring.Count;output->Dropped=f->Ring.Dropped;
}
static VOID Record(WDFREQUEST request,NTSTATUS status,ULONG_PTR bytes) {
 TRACK* t=TrackContext(request);FILTER* f=FilterContext(WdfIoQueueGetDevice(WdfRequestGetIoQueue(request)));
 if(t->Observed){
  LARGE_INTEGER now=KeQueryPerformanceCounter(NULL);
  WdfSpinLockAcquire(StateLock);
  if(t->Epoch==f->Epoch){
   t->Event.CompletedQpc=(SR_U64)now.QuadPart;t->Event.Status=status;
   t->Event.TransferredBytes=(SR_U64)bytes;
   ++f->Stats.Completed;if(!NT_SUCCESS(status))++f->Stats.Errors;
   if(f->Stats.Inflight)--f->Stats.Inflight;
   if(t->Event.Operation==SR_OP_READ)f->Stats.ReadBytes+=t->Event.TransferredBytes;
   if(t->Event.Operation==SR_OP_WRITE)f->Stats.WriteBytes+=t->Event.TransferredBytes;
   SR_Push(&f->Ring,&t->Event);
  }
  WdfSpinLockRelease(StateLock);t->Observed=FALSE;
 }
}
VOID Completed(WDFREQUEST request,WDFIOTARGET target,PWDF_REQUEST_COMPLETION_PARAMS result,WDFCONTEXT context) {
 UNREFERENCED_PARAMETER(target);UNREFERENCED_PARAMETER(context);
 Record(request,result->IoStatus.Status,result->IoStatus.Information);
 WdfRequestCompleteWithInformation(request,result->IoStatus.Status,result->IoStatus.Information);
}
VOID Forward(WDFQUEUE queue,WDFREQUEST request) {
 WDFDEVICE device=WdfIoQueueGetDevice(queue);FILTER* f=FilterContext(device);
 WDF_REQUEST_PARAMETERS parameters;TRACK* t=TrackContext(request);ULONG operation=0;SR_U64 length=0,offset=0;
 WDF_REQUEST_SEND_OPTIONS options;
 t->Observed=FALSE;
 if(InterlockedCompareExchangePointer((PVOID volatile*)&Active,NULL,NULL)==f){
  WDF_REQUEST_PARAMETERS_INIT(&parameters);WdfRequestGetParameters(request,&parameters);
  if(parameters.Type==WdfRequestTypeRead){operation=SR_OP_READ;length=(SR_U64)parameters.Parameters.Read.Length;offset=parameters.Parameters.Read.DeviceOffset;}
  else if(parameters.Type==WdfRequestTypeWrite){operation=SR_OP_WRITE;length=(SR_U64)parameters.Parameters.Write.Length;offset=parameters.Parameters.Write.DeviceOffset;}
  if(operation){
   WdfSpinLockAcquire(StateLock);
   if(CurrentActive()==f){
    RtlZeroMemory(&t->Event,sizeof(t->Event));t->Observed=TRUE;t->Epoch=f->Epoch;
    t->Event.Sequence=++f->Stats.Started;t->Event.Operation=operation;t->Event.ByteOffset=offset;t->Event.RequestedBytes=length;
    ++f->Stats.Inflight;if(f->Stats.Inflight>f->Stats.PeakInflight)f->Stats.PeakInflight=f->Stats.Inflight;
    t->Event.InflightAtStart=(f->Stats.Inflight>MAXULONG)?MAXULONG:(ULONG)f->Stats.Inflight;
    t->Event.StartedQpc=(SR_U64)KeQueryPerformanceCounter(NULL).QuadPart;
   }
   WdfSpinLockRelease(StateLock);
  }
 }
 WdfRequestFormatRequestUsingCurrentType(request);
 if(t->Observed){
  WdfRequestSetCompletionRoutine(request,Completed,NULL);
  if(!WdfRequestSend(request,WdfDeviceGetIoTarget(device),WDF_NO_SEND_OPTIONS)){
   NTSTATUS status=WdfRequestGetStatus(request);Record(request,status,0);WdfRequestComplete(request,status);
  }
 }else{
  WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
  if(!WdfRequestSend(request,WdfDeviceGetIoTarget(device),&options))WdfRequestComplete(request,WdfRequestGetStatus(request));
 }
}
VOID Open(WDFDEVICE device,WDFREQUEST request,WDFFILEOBJECT file) {
 UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(file);
 WdfSpinLockAcquire(StateLock);OwnerOpen=TRUE;WdfSpinLockRelease(StateLock);WdfRequestComplete(request,STATUS_SUCCESS);
}
VOID Close(WDFFILEOBJECT file) {
 ULONG i;WDFDEVICE oldControl=NULL;UNREFERENCED_PARAMETER(file);
 // The control device explicitly uses ExecutionLevelPassive.
 NT_ASSERT(KeGetCurrentIrql()==PASSIVE_LEVEL);_Analysis_assume_(KeGetCurrentIrql()==PASSIVE_LEVEL);
 WdfWaitLockAcquire(LifecycleLock,NULL);WdfSpinLockAcquire(StateLock);OwnerOpen=FALSE;
 InterlockedExchangePointer((PVOID volatile*)&Active,NULL);
 for(i=0;i<SR_DEVICES;++i)if(Filters[i]){
  ++Filters[i]->Epoch;RtlZeroMemory(&Filters[i]->Stats,sizeof(Filters[i]->Stats));RtlZeroMemory(&Filters[i]->Ring,sizeof(Filters[i]->Ring));
  Filters[i]->Stats.Version=SR_VERSION;Filters[i]->Stats.Size=sizeof(SR_STATS);Filters[i]->Stats.Frequency=Frequency;Filters[i]->Stats.DeviceId=Filters[i]->Id;
 }
 if(!DeviceCount){oldControl=Control;Control=NULL;}
 WdfSpinLockRelease(StateLock);if(oldControl)WdfObjectDelete(oldControl);WdfWaitLockRelease(LifecycleLock);
}
VOID Command(WDFQUEUE queue,WDFREQUEST request,size_t outputLength,size_t inputLength,ULONG code) {
 SR_REQUEST* input;PVOID output=NULL;FILTER* f;NTSTATUS status;size_t information=0,required=0;ULONG i;
 UNREFERENCED_PARAMETER(queue);
 if(inputLength!=sizeof(SR_REQUEST)){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
 status=WdfRequestRetrieveInputBuffer(request,sizeof(SR_REQUEST),(PVOID*)&input,NULL);
 if(!NT_SUCCESS(status)||!SR_ValidRequest(input)){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
 if(code==SR_ENUMERATE)required=sizeof(SR_INVENTORY);else if(code==SR_STATUS)required=sizeof(SR_STATS);else if(code==SR_RECEIVE)required=sizeof(SR_BATCH);
 else if(code!=SR_START&&code!=SR_STOP){WdfRequestComplete(request,STATUS_INVALID_DEVICE_REQUEST);return;}
 if(required){if(outputLength<required){WdfRequestComplete(request,STATUS_BUFFER_TOO_SMALL);return;}status=WdfRequestRetrieveOutputBuffer(request,required,&output,NULL);if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}}
 WdfSpinLockAcquire(StateLock);
 if(code==SR_ENUMERATE){
  SR_INVENTORY* inventory=(SR_INVENTORY*)output;RtlZeroMemory(output,required);inventory->Version=SR_VERSION;inventory->Size=sizeof(*inventory);
  for(i=0;i<SR_DEVICES;++i)if(Filters[i]){SR_DEVICE* d=&inventory->Devices[inventory->Count++];d->Id=Filters[i]->Id;RtlCopyMemory(d->PdoName,Filters[i]->PdoName,sizeof(d->PdoName));}
  information=required;status=STATUS_SUCCESS;
 }else if(code==SR_STOP){InterlockedExchangePointer((PVOID volatile*)&Active,NULL);status=STATUS_SUCCESS;}
 else if((f=Find(input->DeviceId))==NULL)status=STATUS_NO_SUCH_DEVICE;
 else if(code==SR_START){
  InterlockedExchangePointer((PVOID volatile*)&Active,NULL);++f->Epoch;RtlZeroMemory(&f->Stats,sizeof(f->Stats));RtlZeroMemory(&f->Ring,sizeof(f->Ring));
  f->Stats.Version=SR_VERSION;f->Stats.Size=sizeof(f->Stats);f->Stats.DeviceId=f->Id;f->Stats.Generation=f->Epoch;f->Stats.Frequency=Frequency;
  InterlockedExchangePointer((PVOID volatile*)&Active,f);status=STATUS_SUCCESS;
 }else{
  RtlZeroMemory(output,required);
  if(code==SR_STATUS)Snapshot(f,(SR_STATS*)output);
  else {SR_BATCH* batch=(SR_BATCH*)output;batch->Count=SR_Pop(&f->Ring,batch->Events,SR_READ);Snapshot(f,&batch->Stats);}
  information=required;status=STATUS_SUCCESS;
 }
 WdfSpinLockRelease(StateLock);WdfRequestCompleteWithInformation(request,status,information);
}
static NTSTATUS CreateControl(WDFDRIVER driver) {
 PWDFDEVICE_INIT init;WDFDEVICE device;WDF_IO_QUEUE_CONFIG queue;WDF_FILEOBJECT_CONFIG files;WDF_OBJECT_ATTRIBUTES attributes;NTSTATUS status;
 DECLARE_CONST_UNICODE_STRING(name,L"\\Device\\TitanStorageFlightRecorder");DECLARE_CONST_UNICODE_STRING(link,L"\\DosDevices\\TitanStorageFlightRecorder");
 DECLARE_CONST_UNICODE_STRING(security,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
 init=WdfControlDeviceInitAllocate(driver,&security);if(!init)return STATUS_INSUFFICIENT_RESOURCES;
 status=WdfDeviceInitAssignName(init,&name);if(!NT_SUCCESS(status)){WdfDeviceInitFree(init);return status;}
 WdfDeviceInitSetDeviceType(init,0x8000);
 WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN,FALSE);WdfDeviceInitSetExclusive(init,TRUE);WdfDeviceInitSetIoType(init,WdfDeviceIoBuffered);
 WDF_FILEOBJECT_CONFIG_INIT(&files,Open,WDF_NO_EVENT_CALLBACK,Close);WdfDeviceInitSetFileObjectConfig(init,&files,WDF_NO_OBJECT_ATTRIBUTES);
 WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status)){if(init)WdfDeviceInitFree(init);return status;}
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchSequential);queue.PowerManaged=WdfFalse;queue.EvtIoDeviceControl=Command;
 status=WdfIoQueueCreate(device,&queue,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
 if(NT_SUCCESS(status))status=WdfDeviceCreateSymbolicLink(device,&link);
 if(!NT_SUCCESS(status)){WdfObjectDelete(device);return status;}
 Control=device;WdfControlFinishInitializing(device);return STATUS_SUCCESS;
}
VOID FilterCleanup(WDFOBJECT object) {
 FILTER* f=FilterContext(object);ULONG i;WDFDEVICE oldControl=NULL;
 // The device explicitly sets ExecutionLevelPassive; this cleanup is never dispatched at DISPATCH_LEVEL.
 NT_ASSERT(KeGetCurrentIrql()==PASSIVE_LEVEL);
 _Analysis_assume_(KeGetCurrentIrql()==PASSIVE_LEVEL);
 if(!f->Counted)return;
 WdfWaitLockAcquire(LifecycleLock,NULL);WdfSpinLockAcquire(StateLock);
 if(f->Registered){for(i=0;i<SR_DEVICES;++i)if(Filters[i]==f)Filters[i]=NULL;f->Registered=FALSE;}
 if(CurrentActive()==f)InterlockedExchangePointer((PVOID volatile*)&Active,NULL);
 if(f->Counted){f->Counted=FALSE;NT_ASSERT(DeviceCount>0);if(DeviceCount)--DeviceCount;}
 if(!DeviceCount&&!OwnerOpen){oldControl=Control;Control=NULL;}
 WdfSpinLockRelease(StateLock);if(oldControl)WdfObjectDelete(oldControl);WdfWaitLockRelease(LifecycleLock);
}
NTSTATUS AddDevice(WDFDRIVER driver,PWDFDEVICE_INIT init) {
 WDFDEVICE device;FILTER* f;WDF_OBJECT_ATTRIBUTES attributes,requests;WDF_IO_QUEUE_CONFIG queue;NTSTATUS status;ULONG i,bytes=0;
 WdfFdoInitSetFilter(init);WdfDeviceInitSetIoType(init,WdfDeviceIoDirect);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&requests,TRACK);WdfDeviceInitSetRequestAttributes(init,&requests);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,FILTER);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.EvtCleanupCallback=FilterCleanup;
 status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status))return status;
 f=FilterContext(device);f->Device=device;
 status=WdfDeviceQueryProperty(device,DevicePropertyPhysicalDeviceObjectName,sizeof(f->PdoName),f->PdoName,&bytes);
 if(!NT_SUCCESS(status)||bytes<sizeof(WCHAR)||bytes>sizeof(f->PdoName))RtlZeroMemory(f->PdoName,sizeof(f->PdoName));
 f->PdoName[SR_NAME-1]=0;
 WdfWaitLockAcquire(LifecycleLock,NULL);++DeviceCount;f->Counted=TRUE;
 // A diagnostic control device failure must never block the disk stack.
 // A later filter re-add retries publication; all data requests still pass through.
 if(!Control){status=CreateControl(driver);UNREFERENCED_PARAMETER(status);}
 WdfSpinLockAcquire(StateLock);
 for(i=0;i<SR_DEVICES;++i)if(!Filters[i]){f->Id=++NextId;Filters[i]=f;f->Registered=TRUE;break;}
 f->Stats.Version=SR_VERSION;f->Stats.Size=sizeof(f->Stats);f->Stats.DeviceId=f->Id;f->Stats.Frequency=Frequency;
 WdfSpinLockRelease(StateLock);WdfWaitLockRelease(LifecycleLock);
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchParallel);queue.EvtIoDefault=Forward;queue.PowerManaged=WdfFalse;
 WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ExecutionLevel=WdfExecutionLevelDispatch;attributes.SynchronizationScope=WdfSynchronizationScopeNone;
 return WdfIoQueueCreate(device,&queue,&attributes,WDF_NO_HANDLE);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING registry) {
 WDF_DRIVER_CONFIG config;WDFDRIVER driver;WDF_OBJECT_ATTRIBUTES attributes;NTSTATUS status;LARGE_INTEGER frequency;
 KeQueryPerformanceCounter(&frequency);Frequency=(SR_U64)frequency.QuadPart;
 WDF_DRIVER_CONFIG_INIT(&config,AddDevice);status=WdfDriverCreate(object,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,&driver);if(!NT_SUCCESS(status))return status;
 WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=driver;
 status=WdfSpinLockCreate(&attributes,&StateLock);if(!NT_SUCCESS(status))return status;
 return WdfWaitLockCreate(&attributes,&LifecycleLock);
}
