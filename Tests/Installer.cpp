#include "../Installer/Setup.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
const char* valid=R"inf([Version]
Signature="$WINDOWS NT$"
Class=System
ClassGuid={4D36E97D-E325-11CE-BFC1-08002BE10318}
Provider="Titanfyre"
CatalogFile=TitanStorageFlightRecorder.cat
DriverVer=10/25/2026,0.1.0.0
PnpLockdown=1
[DestinationDirs]
DriverFiles=13
[DefaultInstall.NTamd64]
CopyFiles=DriverFiles
[DriverFiles]
TitanStorageFlightRecorder.sys
[DefaultInstall.NTamd64.Services]
AddService=TitanStorageFlightRecorder,,Service
[Service]
DisplayName="Titan Storage I/O Flight Recorder"
ServiceType=1
StartType=3
ErrorControl=1
ServiceBinary=%13%\TitanStorageFlightRecorder.sys
)inf";
void Require(bool value){if(!value)throw std::runtime_error("Installer identity assertion failed.");}
std::string Replace(std::string text,const std::string& before,const std::string& after){auto pos=text.find(before);Require(pos!=std::string::npos);text.replace(pos,before.size(),after);return text;}
}
int wmain(){wchar_t folder[MAX_PATH]{},path[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,folder)||!GetTempFileNameW(folder,L"srt",0,path))return 1;
try{
 unsigned index=0;auto check=[&](const std::string& text,bool expected){++index;std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<text;file.close();if(Setup::PackageMatches(path)!=expected)throw std::runtime_error("Installer identity assertion failed at case "+std::to_string(index));};
 check(valid,true);
 check(Replace(valid,"TitanStorageFlightRecorder.cat","Other.cat"),false);
 check(Replace(valid,"Class=System","Class=DiskDrive"),false);
 check(Replace(valid,"StartType=3","StartType=0"),false);
 check(Replace(valid,"ServiceType=1","ServiceType=2"),false);
 check(Replace(valid,"DriverFiles=13","DriverFiles=12"),false);
 check(Replace(valid,"TitanStorageFlightRecorder.sys\n[DefaultInstall","Different.sys\n[DefaultInstall"),false);
 check(Replace(valid,"CopyFiles=DriverFiles","CopyFiles=DriverFiles\nAddReg=Unexpected"),false);
 check(std::string(valid)+"\n[DefaultInstall.NTamd64.10.0]\nAddReg=Unexpected\n",false);
 check(std::string(valid)+"\n[Manufacturer]\nSomething=Other\n",false);
 check(Replace(valid,"%13%\\TitanStorageFlightRecorder.sys","C:\\Windows\\system32\\unknown\\TitanStorageFlightRecorder.sys"),false);
 check(Replace(valid,"%13%\\TitanStorageFlightRecorder.sys","C:\\Outside\\TitanStorageFlightRecorder.sys"),false);
 check(Replace(valid,"%13%\\TitanStorageFlightRecorder.sys","%13%\\..\\TitanStorageFlightRecorder.sys"),false);
 check(Replace(valid,"%13%\\TitanStorageFlightRecorder.sys","%13%\\Nested\\TitanStorageFlightRecorder.sys"),false);
 check(Replace(valid,"%13%\\TitanStorageFlightRecorder.sys","%12%\\TitanStorageFlightRecorder.sys"),false);
 check(Replace(valid,"%13%\\TitanStorageFlightRecorder.sys","%13%\\Different.sys"),false);
 check(Replace(valid,"ServiceBinary=%13%\\TitanStorageFlightRecorder.sys","ServiceBinary=%13%\\TitanStorageFlightRecorder.sys\nServiceBinary=%13%\\TitanStorageFlightRecorder.sys"),false);
 check(std::string("\xef\xbb\xbf")+valid,false); // SetupAPI refuses UTF-8 BOM INFs.
 {std::string utf16="\xff\xfe";for(const unsigned char c:std::string(valid)){utf16.push_back(static_cast<char>(c));utf16.push_back('\0');}check(utf16,true);}
 Require(!Setup::CatalogTrusted(path));
 DeleteFileW(path);std::cout<<"20 installer identity/signature refusal checks passed. No installation or registry change occurred.\n";return 0;
}catch(const std::exception& error){DeleteFileW(path);std::cerr<<error.what()<<'\n';return 1;}}
