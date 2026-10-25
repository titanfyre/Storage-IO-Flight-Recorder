#include "Setup.h"
#include <setupapi.h>
#include <newdev.h>
#include <devguid.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <mscat.h>
#include <sddl.h>
#include <objbase.h>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <set>
#include <algorithm>
#include <string_view>
namespace fs=std::filesystem;
namespace {
constexpr wchar_t Service[]=L"TitanStorageFlightRecorder";
struct Devices {HDEVINFO h=INVALID_HANDLE_VALUE;~Devices(){if(h!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(h);}};
std::wstring Message(DWORD e){wchar_t* s=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,e,0,reinterpret_cast<LPWSTR>(&s),0,nullptr);std::wstring r=s?s:L"Operation failed";if(s)LocalFree(s);return r+L" (Windows error "+std::to_wstring(e)+L")";}
void Require(bool ok){if(!ok)throw std::system_error(static_cast<int>(GetLastError()?GetLastError():ERROR_GEN_FAILURE),std::system_category());}
bool MicrosoftCatalog(const fs::path& path) {
    WINTRUST_FILE_INFO file{}; file.cbStruct = sizeof(file); file.pcwszFilePath = path.c_str();
    WINTRUST_DATA data{}; data.cbStruct = sizeof(data); data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE; data.dwUnionChoice = WTD_CHOICE_FILE; data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY; data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(reinterpret_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data); bool accepted = false;
    if (status == ERROR_SUCCESS) {
        auto provider = WTHelperProvDataFromStateData(data.hWVTStateData);
        auto signer = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;
        if (signer && signer->csCertChain && signer->pChainContext) {
            wchar_t name[256]{}, organization[256]{};
            auto certificate = signer->pasCertChain[0].pCert;
            CertGetNameStringW(certificate, CERT_NAME_ATTR_TYPE, 0, const_cast<char*>(szOID_COMMON_NAME), name, 256);
            CertGetNameStringW(certificate, CERT_NAME_ATTR_TYPE, 0, const_cast<char*>(szOID_ORGANIZATION_NAME), organization, 256);
            CERT_CHAIN_POLICY_PARA policy{}; policy.cbSize = sizeof(policy);
            CERT_CHAIN_POLICY_STATUS verdict{}; verdict.cbSize = sizeof(verdict);
            // Root public-key policy, not a subject-name check. No test-root flag.
            bool microsoftRoot = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_MICROSOFT_ROOT,
                signer->pChainContext, &policy, &verdict) && verdict.dwError == ERROR_SUCCESS;
            if (!microsoftRoot) {
                policy.dwFlags = MICROSOFT_ROOT_CERT_CHAIN_POLICY_CHECK_APPLICATION_ROOT_FLAG;
                verdict.dwError = ERROR_SUCCESS;
                microsoftRoot = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_MICROSOFT_ROOT,
                    signer->pChainContext, &policy, &verdict) && verdict.dwError == ERROR_SUCCESS;
            }
            accepted = microsoftRoot && !_wcsicmp(name, L"Microsoft Windows Hardware Compatibility Publisher") &&
                !_wcsicmp(organization, L"Microsoft Corporation");
        }
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust(reinterpret_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);
    return accepted;
}
bool CatalogMember(const fs::path& directory,const wchar_t* name){
    const auto path=directory/name;const auto catalog=directory/L"TitanStorageFlightRecorder.cat";
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    struct File{HANDLE h;~File(){CloseHandle(h);}} close{file};
    HCATADMIN admin=nullptr;if(!CryptCATAdminAcquireContext2(&admin,nullptr,L"SHA256",nullptr,0))return false;
    struct Admin{HCATADMIN h;~Admin(){CryptCATAdminReleaseContext(h,0);}} release{admin};
    BYTE hash[64]{};DWORD bytes=sizeof(hash);if(!CryptCATAdminCalcHashFromFileHandle2(admin,file,&bytes,hash,0)||bytes>64)return false;
    wchar_t tag[129]{};const wchar_t* hex=L"0123456789ABCDEF";for(DWORD i=0;i<bytes;++i){tag[i*2]=hex[hash[i]>>4];tag[i*2+1]=hex[hash[i]&15];}
    WINTRUST_CATALOG_INFO member{};member.cbStruct=sizeof(member);member.pcwszCatalogFilePath=catalog.c_str();member.pcwszMemberTag=tag;member.pcwszMemberFilePath=path.c_str();member.hMemberFile=file;member.pbCalculatedFileHash=hash;member.cbCalculatedFileHash=bytes;member.hCatAdmin=admin;
    WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.dwUnionChoice=WTD_CHOICE_CATALOG;data.pCatalog=&member;data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL|WTD_REVOCATION_CHECK_NONE;
    GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;LONG result=WinVerifyTrust(nullptr,&action,&data);data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&action,&data);return result==ERROR_SUCCESS;
}
struct Temporary {
    fs::path directory;
    bool owned=false;
    ~Temporary(){if(!owned)return;for(auto name:{L"TitanStorageFlightRecorder.inf",L"TitanStorageFlightRecorder.cat",L"TitanStorageFlightRecorder.sys"})DeleteFileW((directory/name).c_str());RemoveDirectoryW(directory.c_str());}
    void Create(){
        wchar_t windows[MAX_PATH]{};DWORD length=GetWindowsDirectoryW(windows,MAX_PATH);Require(length>0&&length<MAX_PATH);GUID guid{};Require(SUCCEEDED(CoCreateGuid(&guid)));wchar_t id[40]{};StringFromGUID2(guid,id,40);
        // Use a system-owned parent, not the initiating user's writable Temp tree.
        directory=fs::path(windows)/L"Temp"/(std::wstring(L"TitanStorageFlightRecorder-")+id);
        PSECURITY_DESCRIPTOR descriptor=nullptr;Require(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;SY)(A;;FA;;;BA)",SDDL_REVISION_1,&descriptor,nullptr)!=FALSE);
        SECURITY_ATTRIBUTES security{sizeof(security),descriptor,FALSE};bool made=CreateDirectoryW(directory.c_str(),&security)!=FALSE;DWORD error=GetLastError();LocalFree(descriptor);if(!made){SetLastError(error);Require(false);}
        owned=true;
        const wchar_t* names[]{L"TitanStorageFlightRecorder.inf",L"TitanStorageFlightRecorder.cat",L"TitanStorageFlightRecorder.sys"};
        for(int i=0;i<3;++i){
            HRSRC resource=FindResourceW(nullptr,MAKEINTRESOURCEW(101+i),RT_RCDATA);Require(resource!=nullptr);HGLOBAL loaded=LoadResource(nullptr,resource);Require(loaded!=nullptr);DWORD size=SizeofResource(nullptr,resource);void* bytes=LockResource(loaded);Require(bytes&&size&&size<16*1024*1024);
            HANDLE f=CreateFileW((directory/names[i]).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);Require(f!=INVALID_HANDLE_VALUE);DWORD written=0;bool ok=WriteFile(f,bytes,size,&written,nullptr)&&written==size;error=GetLastError();CloseHandle(f);if(!ok){SetLastError(error);Require(false);}
        }
    }
};
bool Field(HINF inf,const wchar_t* section,const wchar_t* key,DWORD index,const wchar_t* expected){INFCONTEXT line{};wchar_t value[1024]{};return SetupFindFirstLineW(inf,section,key,&line)&&SetupGetStringFieldW(&line,index,value,1024,nullptr)&&!_wcsicmp(value,expected);}
bool Number(HINF inf,const wchar_t* section,const wchar_t* key,DWORD index,int expected){INFCONTEXT line{};int n=0;return SetupFindFirstLineW(inf,section,key,&line)&&SetupGetIntField(&line,index,&n)&&n==expected;}
bool CanonicalServiceBinary(const fs::path& path){
 // SetupAPI expands DIRID 13 differently for source and published primitive INFs
 // (including system32\\unknown). Validate the exact raw directive, not that path.
 HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 struct File{HANDLE h;~File(){CloseHandle(h);}} close{file};LARGE_INTEGER size{};
 if(!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart>262144)return false;
 std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));DWORD read=0;
 if(!ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr)||read!=bytes.size())return false;
 std::wstring text;text.reserve(bytes.size());
 if(bytes.size()>=2&&bytes[0]==0xff&&bytes[1]==0xfe){
  if(bytes.size()%2)return false;
  for(size_t i=2;i<bytes.size();i+=2){const wchar_t c=static_cast<wchar_t>(bytes[i]|(static_cast<unsigned>(bytes[i+1])<<8));if(!c)return false;text.push_back(c);}
 }else{
  const size_t start=bytes.size()>=3&&bytes[0]==0xef&&bytes[1]==0xbb&&bytes[2]==0xbf?3:0;
  for(size_t i=start;i<bytes.size();++i){if(!bytes[i])return false;text.push_back(bytes[i]);}
 }
 auto trim=[](std::wstring_view value){const auto start=value.find_first_not_of(L" \t\r");if(start==value.npos)return std::wstring_view{};return value.substr(start,value.find_last_not_of(L" \t\r")-start+1);};
 auto equal=[](std::wstring_view a,std::wstring_view b){return a.size()==b.size()&&_wcsnicmp(a.data(),b.data(),a.size())==0;};
 bool inService=false;unsigned sections=0,binaries=0;
 for(size_t offset=0;offset<text.size();){
  const auto end=text.find(L'\n',offset);auto line=trim(std::wstring_view(text).substr(offset,end==text.npos?text.size()-offset:end-offset));offset=end==text.npos?text.size():end+1;
  // Restrict this one directive to its single canonical, uncommented form.
  if(line.empty()||line.front()==L';')continue;
  if(line.front()==L'['){inService=equal(line,L"[Service]");if(inService&&++sections!=1)return false;continue;}
  if(!inService)continue;const auto separator=line.find(L'=');if(separator==line.npos)continue;
  if(equal(trim(line.substr(0,separator)),L"ServiceBinary")){
   if(++binaries!=1||!equal(trim(line.substr(separator+1)),L"%13%\\TitanStorageFlightRecorder.sys"))return false;
  }
 }
 return sections==1&&binaries==1;
}
bool OwnPackage(const fs::path& path){
 GUID cls{};wchar_t name[128]{};if(!SetupDiGetINFClassW(path.c_str(),&cls,name,128,nullptr)||!IsEqualGUID(cls,GUID_DEVCLASS_SYSTEM))return false;
 HINF handle=SetupOpenInfFileW(path.c_str(),nullptr,INF_STYLE_WIN4,nullptr);if(handle==INVALID_HANDLE_VALUE)return false;struct Inf{HINF h;~Inf(){SetupCloseInfFile(h);}} inf{handle};
 const wchar_t* allowed[]={L"Version",L"DestinationDirs",L"SourceDisksNames",L"SourceDisksFiles",L"DefaultInstall.NTamd64",L"DriverFiles",L"DefaultInstall.NTamd64.Services",L"Service",L"DefaultInstall.NTamd64.Wdf",L"Wdf",L"Strings"};
 for(UINT i=0;;++i){wchar_t section[256]{};if(!SetupEnumInfSectionsW(handle,i,section,256,nullptr)){if(GetLastError()!=ERROR_NO_MORE_ITEMS)return false;break;}if(i>=64||std::none_of(std::begin(allowed),std::end(allowed),[&](const wchar_t* value){return !_wcsicmp(section,value);}))return false;}
 return Field(handle,L"Version",L"Class",1,L"System")&&Field(handle,L"Version",L"Provider",1,L"Titanfyre")&&Field(handle,L"Version",L"CatalogFile",1,L"TitanStorageFlightRecorder.cat")&&Number(handle,L"Version",L"PnpLockdown",1,1)&&
  SetupGetLineCountW(handle,L"DefaultInstall.NTamd64")==1&&SetupGetLineCountW(handle,L"DefaultInstall.NTamd64.Services")==1&&SetupGetLineCountW(handle,L"Service")==5&&
  SetupGetLineCountW(handle,L"Manufacturer")==-1&&Field(handle,L"DefaultInstall.NTamd64",L"CopyFiles",1,L"DriverFiles")&&
  SetupGetLineCountW(handle,L"DriverFiles")==1&&Field(handle,L"DriverFiles",nullptr,1,L"TitanStorageFlightRecorder.sys")&&Number(handle,L"DestinationDirs",L"DriverFiles",1,13)&&
  Field(handle,L"DefaultInstall.NTamd64.Services",L"AddService",1,Service)&&Field(handle,L"DefaultInstall.NTamd64.Services",L"AddService",3,L"Service")&&
  Number(handle,L"Service",L"ServiceType",1,SERVICE_KERNEL_DRIVER)&&Number(handle,L"Service",L"StartType",1,SERVICE_DEMAND_START)&&CanonicalServiceBinary(path);
}
std::vector<fs::path> Packages(){
 wchar_t windows[MAX_PATH]{};DWORD n=GetWindowsDirectoryW(windows,MAX_PATH);Require(n>0&&n<MAX_PATH);fs::path directory=fs::path(windows)/L"INF";std::vector<fs::path> result;
 WIN32_FIND_DATAW entry{};HANDLE find=FindFirstFileW((directory/L"oem*.inf").c_str(),&entry);if(find==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_NOT_FOUND)return result;Require(false);}struct Search{HANDLE h;~Search(){FindClose(h);}} search{find};unsigned count=0;
 do{if(++count>16384)throw std::runtime_error("Driver store scan exceeded its bound.");auto path=directory/entry.cFileName;if(OwnPackage(path))result.push_back(path);}while(FindNextFileW(find,&entry));if(GetLastError()!=ERROR_NO_MORE_FILES)Require(false);return result;
}
bool TrustedInstalled(){
 for(const auto& published:Packages()){
  wchar_t store[32768]{};DWORD needed=0;if(!SetupGetInfDriverStoreLocationW(published.c_str(),nullptr,nullptr,store,32768,&needed))continue;
  auto directory=fs::path(store).parent_path();if(MicrosoftCatalog(directory/L"TitanStorageFlightRecorder.cat")&&CatalogMember(directory,L"TitanStorageFlightRecorder.inf")&&CatalogMember(directory,L"TitanStorageFlightRecorder.sys"))return true;
 }
 return false;
}
bool ValidInstance(const std::wstring& id){return !id.empty()&&id.size()<2048&&std::all_of(id.begin(),id.end(),[](wchar_t c){return c>=32&&c!=L'"'&&c!=L'\x7f';});}
struct Selected{Devices set;SP_DEVINFO_DATA data{};Selected(const std::wstring& id){Require(ValidInstance(id));set.h=SetupDiCreateDeviceInfoList(&GUID_DEVCLASS_DISKDRIVE,nullptr);Require(set.h!=INVALID_HANDLE_VALUE);data.cbSize=sizeof(data);Require(SetupDiOpenDeviceInfoW(set.h,id.c_str(),nullptr,0,&data)!=FALSE);Require(IsEqualGUID(data.ClassGuid,GUID_DEVCLASS_DISKDRIVE)!=FALSE);}};
std::vector<std::wstring> Filters(HDEVINFO set,SP_DEVINFO_DATA& device){
 DWORD type=0,bytes=0;if(!SetupDiGetDeviceRegistryPropertyW(set,&device,SPDRP_UPPERFILTERS,&type,nullptr,0,&bytes)){
  auto error=GetLastError();if(error==ERROR_INVALID_DATA||error==ERROR_FILE_NOT_FOUND)return {};if(error!=ERROR_INSUFFICIENT_BUFFER)Require(false);
 }
 if(type!=REG_MULTI_SZ||bytes<4||bytes>65536||bytes%2){SetLastError(ERROR_INVALID_DATA);Require(false);}
 std::vector<wchar_t> value(bytes/2+2,0);Require(SetupDiGetDeviceRegistryPropertyW(set,&device,SPDRP_UPPERFILTERS,&type,reinterpret_cast<BYTE*>(value.data()),bytes,nullptr)!=FALSE);
 if(value[bytes/2-1]||value[bytes/2-2]){SetLastError(ERROR_INVALID_DATA);Require(false);}std::vector<std::wstring> result;
 const wchar_t* end=value.data()+bytes/2;const wchar_t* p=value.data();
 for(;p<end&&*p;p+=wcslen(p)+1)result.emplace_back(p);
 for(;p<end;++p)if(*p){SetLastError(ERROR_INVALID_DATA);Require(false);}
 return result;
}
void WriteFilters(HDEVINFO set,SP_DEVINFO_DATA& device,const std::vector<std::wstring>& list,const std::vector<std::wstring>& expected){
 if(Filters(set,device)!=expected){SetLastError(ERROR_RETRY);Require(false);}
 std::vector<wchar_t> raw;for(const auto& filter:list){raw.insert(raw.end(),filter.begin(),filter.end());raw.push_back(0);}raw.push_back(0);if(raw.size()==1)raw.push_back(0);
 Require(SetupDiSetDeviceRegistryPropertyW(set,&device,SPDRP_UPPERFILTERS,list.empty()?nullptr:reinterpret_cast<const BYTE*>(raw.data()),list.empty()?0:static_cast<DWORD>(raw.size()*sizeof(wchar_t)))!=FALSE);
 auto verify=Filters(set,device);if(verify!=list){SetLastError(ERROR_WRITE_FAULT);Require(false);}
}
Setup::Result Association(const std::wstring& instance,bool adding){
 if(!ValidInstance(instance))return {ERROR_INVALID_PARAMETER,false,L"Provide one exact DiskDrive device instance ID."};
 if(adding){
  bool found=false;for(const auto& disk:StorageRecorder::Disks())if(!_wcsicmp(disk.instance.c_str(),instance.c_str())){found=true;if(!disk.eligible)return {ERROR_ACCESS_DENIED,false,disk.reason};}
  if(!found)return {ERROR_NOT_FOUND,false,L"Exact present scratch disk not found. Nothing was changed."};
  if(!TrustedInstalled())return {ERROR_INVALID_IMAGE_HASH,false,L"No matching installed package passed product identity, Microsoft catalog and INF/SYS membership checks. Run Install package with the matching signed setup, then retry attachment."};
 }
 Selected selected(instance);auto original=Filters(selected.set.h,selected.data);auto changed=original;
 changed.erase(std::remove_if(changed.begin(),changed.end(),[](const std::wstring& f){return !_wcsicmp(f.c_str(),Service);}),changed.end());if(adding)changed.push_back(Service);
 if(changed==original)return {0,false,adding?L"Recorder is already associated with this exact disk.":L"Recorder is not associated with this disk."};
 try{WriteFilters(selected.set.h,selected.data,changed,original);}catch(...){
  std::vector<std::wstring> current;
  try{current=Filters(selected.set.h,selected.data);}catch(...){return {ERROR_RETRY,true,L"The selected disk's filter list could not be read after an update attempt. Its state is unknown; no rollback was attempted. Close other installers, inspect UpperFilters and restart before retrying."};}
  if(current==changed){try{WriteFilters(selected.set.h,selected.data,original,changed);}catch(...){return {ERROR_WRITE_FAULT,true,L"Association failed and rollback could not be confirmed. Another installer may be editing this node. No further changes were attempted; inspect its per-device UpperFilters and restart Windows before retrying."};}}
  else if(current!=original)return {ERROR_RETRY,true,L"The selected disk's filters changed concurrently. They were retained rather than overwritten. Close other installers, inspect UpperFilters and restart before retrying."};
  throw;
 }
 return {0,true,adding?L"Recorder association added only to the selected scratch disk. Existing filters were preserved. Close applications and restart Windows to load the filter. No device was restarted automatically.":L"Recorder association removed only from the selected disk. Existing filters were preserved. Restart Windows to finish detaching; disk partitioning does not prevent detachment."};
}
Setup::Result Install(){
 Temporary temp;temp.Create();if(!OwnPackage(temp.directory/L"TitanStorageFlightRecorder.inf"))return {ERROR_INVALID_DATA,false,L"Embedded package identity is invalid."};
 if(!MicrosoftCatalog(temp.directory/L"TitanStorageFlightRecorder.cat")||!CatalogMember(temp.directory,L"TitanStorageFlightRecorder.inf")||!CatalogMember(temp.directory,L"TitanStorageFlightRecorder.sys"))return {ERROR_INVALID_IMAGE_HASH,false,L"The embedded package must have a trusted Microsoft production catalog and matching INF/SYS membership. Unsigned and test-signed packages are refused."};
 BOOL reboot=FALSE;if(!DiInstallDriverW(nullptr,(temp.directory/L"TitanStorageFlightRecorder.inf").c_str(),0,&reboot))return {GetLastError(),reboot!=FALSE,L"Primitive driver installation failed. "+Message(GetLastError())};
 return {0,reboot!=FALSE,L"Driver package installed. No disk was attached. Explicitly choose one unpartitioned disposable scratch disk and attach the recorder."};
}
Setup::Result Uninstall(){
 Devices devices;devices.h=SetupDiGetClassDevsW(&GUID_DEVCLASS_DISKDRIVE,nullptr,nullptr,0);Require(devices.h!=INVALID_HANDLE_VALUE);unsigned removed=0;
 struct Plan{SP_DEVINFO_DATA device;std::vector<std::wstring> before,after;};std::vector<Plan> plans;
 // Preflight every selected filter list before the first mutation.
 for(DWORD i=0;;++i){SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);if(!SetupDiEnumDeviceInfo(devices.h,i,&device)){if(GetLastError()!=ERROR_NO_MORE_ITEMS)Require(false);break;}auto before=Filters(devices.h,device),after=before;after.erase(std::remove_if(after.begin(),after.end(),[](const std::wstring& f){return !_wcsicmp(f.c_str(),Service);}),after.end());if(before!=after)plans.push_back({device,std::move(before),std::move(after)});}
 for(auto& plan:plans){
  try{WriteFilters(devices.h,plan.device,plan.after,plan.before);++removed;}
  catch(const std::system_error& error){return {static_cast<DWORD>(error.code().value()),true,L"Uninstall stopped while updating a disk association; "+std::to_wstring(removed)+L" earlier association(s) were confirmed removed. The latest attempt may have changed the association before verification failed. Refresh inventory and restart before retrying. "+Message(static_cast<DWORD>(error.code().value()))};}
  catch(...){return {ERROR_GEN_FAILURE,true,L"Uninstall stopped after "+std::to_wstring(removed)+L" confirmed association changes. The latest update state is unknown. Refresh disk inventory and restart before retrying."};}
 }
 if(removed)return {0,true,L"Recorder associations removed from "+std::to_wstring(removed)+L" disk(s); unrelated filters preserved. Restart Windows, then run Uninstall again to remove the unused package."};
 BOOL restart=FALSE;for(const auto& inf:Packages()){BOOL reboot=FALSE;if(!DiUninstallDriverW(nullptr,inf.c_str(),0,&reboot))return {GetLastError(),true,L"Windows retained the in-use driver package. Close recorder apps, restart and run Uninstall again. "+Message(GetLastError())};restart=restart||reboot;}
 return {0,restart!=FALSE,L"Unused recorder package removed. No disk contents or unrelated packages were changed."};
}
}
bool Setup::ServiceInstalled() noexcept {SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);if(!manager)return false;SC_HANDLE service=OpenServiceW(manager,Service,SERVICE_QUERY_STATUS);bool found=service!=nullptr;if(service)CloseServiceHandle(service);CloseServiceHandle(manager);return found;}
bool Setup::PackageMatches(const std::wstring& path) noexcept{try{return OwnPackage(path);}catch(...){return false;}}
bool Setup::CatalogTrusted(const std::wstring& path) noexcept{try{return MicrosoftCatalog(path);}catch(...){return false;}}
Setup::Result Setup::Execute(Action action,const std::wstring& instance) noexcept {
 HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Global\\TitanStorageFlightRecorderSetup");if(!mutex)return {GetLastError(),false,L"Cannot create setup lock."};DWORD wait=WaitForSingleObject(mutex,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);return {ERROR_BUSY,false,L"Another recorder installation is running."};}
 Result result;try{switch(action){case Action::Install:result=Install();break;case Action::Attach:result=Association(instance,true);break;case Action::Detach:result=Association(instance,false);break;case Action::Uninstall:result=Uninstall();break;}}
 catch(const std::system_error& error){result={static_cast<DWORD>(error.code().value()),false,Message(static_cast<DWORD>(error.code().value()))};}
 catch(const std::exception&){result={ERROR_GEN_FAILURE,false,L"Setup could not complete. Read the operation result and refresh disk inventory before retrying."};}
 ReleaseMutex(mutex);CloseHandle(mutex);return result;
}
bool Setup::Restart() noexcept {
 HANDLE token{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return false;TOKEN_PRIVILEGES privilege{};privilege.PrivilegeCount=1;
 bool good=LookupPrivilegeValueW(nullptr,SE_SHUTDOWN_NAME,&privilege.Privileges[0].Luid)!=FALSE;privilege.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
 if(good){SetLastError(0);good=AdjustTokenPrivileges(token,FALSE,&privilege,0,nullptr,nullptr)&&GetLastError()==ERROR_SUCCESS;}CloseHandle(token);
 return good&&ExitWindowsEx(EWX_REBOOT,SHTDN_REASON_MAJOR_APPLICATION|SHTDN_REASON_MINOR_INSTALLATION|SHTDN_REASON_FLAG_PLANNED);
}
