#include "Setup.h"
#include <iostream>
int wmain(int argc,wchar_t** argv){
 if(argc==2&&wcscmp(argv[1],L"/status")==0){std::wcout<<(Setup::ServiceInstalled()?L"Recorder service package is installed. A selected disk association/restart is also required.\n":L"Recorder driver is not installed.\n");return 0;}
 Setup::Action action;std::wstring instance;
 if(argc==2&&wcscmp(argv[1],L"/install")==0)action=Setup::Action::Install;
 else if(argc==2&&wcscmp(argv[1],L"/uninstall")==0)action=Setup::Action::Uninstall;
 else if(argc==3&&wcscmp(argv[1],L"/attach")==0){action=Setup::Action::Attach;instance=argv[2];}
 else if(argc==3&&wcscmp(argv[1],L"/detach")==0){action=Setup::Action::Detach;instance=argv[2];}
 else{std::wcout<<L"StorageRecorderSetupConsole /install | /status | /attach \"exact DiskDrive instance\" | /detach \"exact DiskDrive instance\" | /uninstall\nAttach accepts only explicitly selected unpartitioned disposable scratch disks. It never restarts a disk or Windows automatically.\n";return argc==1?0:2;}
 auto result=Setup::Execute(action,instance);std::wcout<<result.detail<<L"\n";if(result.error)std::wcout<<L"Windows error "<<result.error<<L"\n";if(result.reboot)std::wcout<<L"Restart required.\n";
 return result.error?static_cast<int>(result.error):result.reboot?3010:0;
}
