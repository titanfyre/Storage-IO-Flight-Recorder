#pragma once
#include "../SDK/StorageRecorder.h"
namespace Setup {
enum class Action {Install,Attach,Detach,Uninstall};
struct Result{DWORD error=0;bool reboot=false;std::wstring detail;};
Result Execute(Action,const std::wstring& instance={}) noexcept;
bool ServiceInstalled() noexcept;
bool PackageMatches(const std::wstring&) noexcept;
bool CatalogTrusted(const std::wstring&) noexcept;
bool Restart() noexcept;
}
