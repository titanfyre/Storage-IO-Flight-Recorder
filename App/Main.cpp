#include "../SDK/StorageRecorder.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <array>
#include <deque>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace {
constexpr COLORREF Navy=RGB(17,29,49),Blue=RGB(40,106,238),Paper=RGB(242,245,250),Muted=RGB(104,118,140);
HWND window{},picker{},start{},stop{},list{},refresh{},setup{},exportButton{};HFONT body{},heading{},captionFont{};HBRUSH paper{},white{};
StorageRecorder::Session recorder;std::vector<SR_DEVICE> devices;std::deque<SR_EVENT> events;std::deque<double> history;SR_STATS stats{};bool active=false,preview=false;std::wstring note=L"Choose an explicitly attached Titan Storage Fault Lab disk. Recording is initially inactive.";
std::wstring Wide(const char* value){int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,-1,nullptr,0);if(count<=1)return L"Operation failed.";std::wstring text(static_cast<size_t>(count),0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,-1,text.data(),count);text.pop_back();return text;}
std::wstring Number(double value){wchar_t text[64]{};swprintf_s(text,L"%.2f",value);return text;}
void Error(const std::exception& e){note=Wide(e.what());InvalidateRect(window,nullptr,FALSE);}
void Text(HDC dc,int x,int y,const std::wstring& value,COLORREF color,HFONT font=nullptr){SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);auto old=SelectObject(dc,font?font:body);TextOutW(dc,x,y,value.c_str(),static_cast<int>(value.size()));SelectObject(dc,old);}
void Box(HDC dc,RECT rect,COLORREF color){HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&rect,brush);DeleteObject(brush);}
void End(){KillTimer(window,1);recorder.Stop();active=false;EnableWindow(start,!preview&&!devices.empty());EnableWindow(stop,FALSE);EnableWindow(picker,!preview);EnableWindow(refresh,!preview);note=L"Stopped. Recorder owner closed and queued metadata cleared. Disk I/O continues normally.";InvalidateRect(window,nullptr,FALSE);}
void Refresh(){
 recorder.Stop();devices.clear();SendMessageW(picker,CB_RESETCONTENT,0,0);
 try{devices=recorder.Devices();for(const auto& d:devices){std::wstring label=L"Filter "+std::to_wstring(d.Id)+L" | "+reinterpret_cast<const wchar_t*>(d.PdoName);SendMessageW(picker,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}recorder.Stop();if(!devices.empty())SendMessageW(picker,CB_SETCURSEL,0,0);note=devices.empty()?L"No attached recorder filters. Use Driver setup to explicitly attach a disposable scratch disk.":L"Attached scratch disk filters found. Choose one and start recording.";}
 catch(const std::exception& e){recorder.Stop();Error(e);}EnableWindow(start,!devices.empty());InvalidateRect(window,nullptr,FALSE);
}
void Event(const SR_EVENT& e){
 if(events.size()==500)events.pop_front();events.push_back(e);
 const auto duration=StorageRecorder::Microseconds(e,stats.Frequency);if(history.size()==80)history.pop_front();history.push_back(duration);
 std::array<std::wstring,7> values{std::to_wstring(e.Sequence),e.Operation==SR_OP_READ?L"Read":e.Operation==SR_OP_WRITE?L"Write":L"Other",std::to_wstring(e.ByteOffset),std::to_wstring(e.RequestedBytes),std::to_wstring(e.TransferredBytes),Number(duration),L""};
 wchar_t code[24]{};swprintf_s(code,L"0x%08lX",static_cast<unsigned long>(e.Status));values[6]=code;
 if(ListView_GetItemCount(list)>=500)ListView_DeleteItem(list,0);LVITEMW row{};row.mask=LVIF_TEXT;row.iItem=ListView_GetItemCount(list);row.pszText=values[0].data();int i=ListView_InsertItem(list,&row);for(int col=1;col<7;++col)ListView_SetItemText(list,i,col,values[static_cast<size_t>(col)].data());
}
void Export(){
 wchar_t path[1024]=L"storage-recorder.csv";OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFilter=L"CSV metadata (*.csv)\0*.csv\0";dialog.lpstrFile=path;dialog.nMaxFile=1024;dialog.Flags=OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR;dialog.lpstrDefExt=L"csv";if(!GetSaveFileNameW(&dialog))return;
 std::ofstream file(path,std::ios::binary);if(!file){note=L"Could not create export file.";return;}file<<"sequence,operation,offset,requested_bytes,transferred_bytes,ntstatus,duration_us,inflight_at_start\n";for(const auto& e:events)file<<e.Sequence<<','<<e.Operation<<','<<e.ByteOffset<<','<<e.RequestedBytes<<','<<e.TransferredBytes<<','<<e.Status<<','<<StorageRecorder::Microseconds(e,stats.Frequency)<<','<<e.InflightAtStart<<'\n';note=L"Exported only the displayed completion metadata. No disk contents or file names were captured.";InvalidateRect(window,nullptr,FALSE);
}
void Paint(HDC dc,int width,int height){
 Box(dc,{0,0,width,height},Paper);Box(dc,{0,0,width,92},Navy);Text(dc,26,17,L"Storage I/O Flight Recorder",RGB(255,255,255),heading);Text(dc,28,57,preview?L"OFFLINE PREVIEW / synthetic metadata / no driver or disk access":L"Scratch-disk diagnostics / native C++ SDK / read-only metadata",RGB(194,208,231),captionFont);
 Box(dc,{24,170,width-24,258},RGB(255,255,255));
 Text(dc,40,183,L"COMPLETED",Muted,captionFont);Text(dc,40,211,std::to_wstring(stats.Completed),Navy,heading);
 Text(dc,270,183,L"IN FLIGHT / PEAK",Muted,captionFont);Text(dc,270,211,std::to_wstring(stats.Inflight)+L" / "+std::to_wstring(stats.PeakInflight),Navy,heading);
 Text(dc,520,183,L"ERRORS",Muted,captionFont);Text(dc,520,211,std::to_wstring(stats.Errors),Navy,heading);
 Text(dc,750,183,L"OVERWRITTEN LOGS",Muted,captionFont);Text(dc,750,211,std::to_wstring(stats.Dropped),Navy,heading);
 Box(dc,{24,274,width-24,414},RGB(255,255,255));Text(dc,40,286,L"Observed request duration at this filter boundary (microseconds)",Navy,captionFont);
 double maximum=1;for(double x:history)maximum=(std::max)(maximum,x);Text(dc,40,315,history.empty()?L"No samples":Number(maximum)+L" us",Muted,captionFont);
 HPEN pen=CreatePen(PS_SOLID,2,Blue);auto old=SelectObject(dc,pen);if(!history.empty()){size_t i=0;for(double value:history){int x=145+static_cast<int>(i*(static_cast<size_t>((std::max)(1,width-200)))/79);int y=393-static_cast<int>(65*value/maximum);if(!i)MoveToEx(dc,x,y,nullptr);else LineTo(dc,x,y);++i;}}SelectObject(dc,old);DeleteObject(pen);
 RECT message{28,height-84,width-28,height-40};SetTextColor(dc,Navy);SetBkMode(dc,TRANSPARENT);SelectObject(dc,captionFont);DrawTextW(dc,note.c_str(),-1,&message,DT_WORDBREAK|DT_END_ELLIPSIS);
 Text(dc,28,height-30,L"Queue dispatch -> lower completion. No per-driver attribution, payload logging or hardware/end-to-end latency claim.",Muted,captionFont);
}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_CREATE:{body=CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");heading=CreateFontW(26,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");captionFont=CreateFontW(15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");paper=CreateSolidBrush(Paper);white=CreateSolidBrush(RGB(255,255,255));
  picker=CreateWindowW(WC_COMBOBOXW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,24,112,430,200,h,reinterpret_cast<HMENU>(7),nullptr,nullptr);
  start=CreateWindowW(L"BUTTON",L"Start recording",WS_CHILD|WS_VISIBLE|WS_TABSTOP,468,108,145,40,h,reinterpret_cast<HMENU>(1),nullptr,nullptr);
  stop=CreateWindowW(L"BUTTON",L"Stop",WS_CHILD|WS_VISIBLE|WS_TABSTOP,624,108,80,40,h,reinterpret_cast<HMENU>(2),nullptr,nullptr);
  refresh=CreateWindowW(L"BUTTON",L"Refresh",WS_CHILD|WS_VISIBLE|WS_TABSTOP,716,108,90,40,h,reinterpret_cast<HMENU>(3),nullptr,nullptr);
  setup=CreateWindowW(L"BUTTON",L"Driver setup",WS_CHILD|WS_VISIBLE|WS_TABSTOP,818,108,115,40,h,reinterpret_cast<HMENU>(4),nullptr,nullptr);
  exportButton=CreateWindowW(L"BUTTON",L"Export CSV",WS_CHILD|WS_VISIBLE|WS_TABSTOP,946,108,110,40,h,reinterpret_cast<HMENU>(5),nullptr,nullptr);
  list=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL,24,430,1032,235,h,nullptr,nullptr,nullptr);ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
  std::array<std::pair<const wchar_t*,int>,7> columns{{{L"Sequence",90},{L"Operation",105},{L"Byte offset",160},{L"Requested",140},{L"Transferred",140},{L"Duration (us)",150},{L"NTSTATUS",150}}};for(unsigned i=0;i<columns.size();++i){LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.cx=columns[i].second;col.pszText=const_cast<wchar_t*>(columns[i].first);ListView_InsertColumn(list,static_cast<int>(i),&col);}
  for(HWND child:{picker,start,stop,refresh,setup,exportButton,list})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(body),TRUE);EnableWindow(stop,FALSE);
  if(preview){SendMessageW(picker,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Synthetic Fault Lab disk | offline preview"));SendMessageW(picker,CB_SETCURSEL,0,0);for(HWND c:{picker,start,stop,refresh,setup})EnableWindow(c,FALSE);stats.Version=SR_VERSION;stats.Size=sizeof(stats);stats.DeviceId=1;stats.Frequency=10000000;stats.Completed=4680;stats.Started=4682;stats.Inflight=2;stats.PeakInflight=12;for(unsigned i=0;i<28;++i){SR_EVENT e{};e.Sequence=i+1;e.Operation=i%3==0?SR_OP_WRITE:SR_OP_READ;e.StartedQpc=100000+i*1000;e.CompletedQpc=e.StartedQpc+static_cast<SR_U64>(160+700*(.5+.5*std::sin(i*.8)));e.ByteOffset=i*4096;e.RequestedBytes=e.TransferredBytes=4096;Event(e);}note=L"Synthetic design preview only. Start the signed driver on an explicitly selected Fault Lab disk for real observations.";}
  else PostMessageW(h,WM_APP+1,0,0);return 0;}
 case WM_APP+1:Refresh();return 0;
 case WM_COMMAND:if(HIWORD(w)==BN_CLICKED){try{switch(LOWORD(w)){case 1:{auto i=SendMessageW(picker,CB_GETCURSEL,0,0);if(i==CB_ERR||static_cast<size_t>(i)>=devices.size())break;recorder.Start(devices[static_cast<size_t>(i)].Id);stats=recorder.Status();events.clear();history.clear();ListView_DeleteAllItems(list);active=true;SetTimer(h,1,125,nullptr);EnableWindow(start,FALSE);EnableWindow(stop,TRUE);EnableWindow(picker,FALSE);EnableWindow(refresh,FALSE);note=L"Recording scratch-disk I/O completion metadata. A full log overwrites old records; it does not queue disk traffic.";break;}case 2:End();break;case 3:Refresh();break;case 4:{wchar_t path[32768]{};DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)break;std::wstring file(path);file=file.substr(0,file.find_last_of(L"\\/"))+L"\\StorageRecorderSetup.exe";if(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES)MessageBoxW(h,L"The signed driver setup is not included in this app/SDK preview. It will be supplied after Microsoft signing and package validation.",L"Driver setup",MB_OK|MB_ICONINFORMATION);else ShellExecuteW(h,L"runas",file.c_str(),nullptr,nullptr,SW_SHOW);break;}case 5:Export();break;}}catch(const std::exception& e){End();Error(e);}InvalidateRect(h,nullptr,FALSE);}return 0;
 case WM_TIMER:if(active)try{for(unsigned n=0;n<8;++n){auto batch=recorder.Read();stats=batch.Stats;for(unsigned i=0;i<batch.Count;++i)Event(batch.Events[i]);if(batch.Count<SR_READ)break;}InvalidateRect(h,nullptr,FALSE);}catch(const std::exception& e){End();Error(e);}return 0;
 case WM_SIZE:{int width=LOWORD(l),height=HIWORD(l);if(list)MoveWindow(list,24,430,(std::max)(100,width-48),(std::max)(60,height-530),TRUE);InvalidateRect(h,nullptr,FALSE);return 0;}
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={1120,820};return 0;
 case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(memory,bitmap);Paint(memory,r.right,r.bottom);BitBlt(dc,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);EndPaint(h,&ps);return 0;}
 case WM_PRINTCLIENT:{RECT bounds{};GetClientRect(h,&bounds);Paint(reinterpret_cast<HDC>(w),bounds.right,bounds.bottom);return 0;}
 case WM_ERASEBKGND:return 1;
 case WM_CTLCOLORSTATIC:SetTextColor(reinterpret_cast<HDC>(w),Navy);SetBkColor(reinterpret_cast<HDC>(w),Paper);return reinterpret_cast<LRESULT>(paper);
 case WM_SETCURSOR:if(reinterpret_cast<HWND>(w)==start||reinterpret_cast<HWND>(w)==stop||reinterpret_cast<HWND>(w)==refresh||reinterpret_cast<HWND>(w)==setup||reinterpret_cast<HWND>(w)==exportButton){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
 case WM_CLOSE:End();DestroyWindow(h);return 0;
 case WM_DESTROY:recorder.Stop();DeleteObject(body);DeleteObject(heading);DeleteObject(captionFont);DeleteObject(paper);DeleteObject(white);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,m,w,l);}
bool RenderPreview(HWND handle,const wchar_t* path){
 RECT rect{};GetClientRect(handle,&rect);if(rect.right<=0||rect.bottom<=0||rect.right>4096||rect.bottom>4096)return false;
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=rect.right;info.bmiHeader.biHeight=-rect.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 HDC source=GetDC(handle),memory=CreateCompatibleDC(source);void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(source,&info,DIB_RGB_COLORS,&pixels,nullptr,0);ReleaseDC(handle,source);
 if(!bitmap||!memory){if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);return false;}auto old=SelectObject(memory,bitmap);
 SendMessageW(handle,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_ERASEBKGND);
 for(HWND child=GetWindow(handle,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
  if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))continue;RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,handle,reinterpret_cast<POINT*>(&bounds),2);int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;if(width<=0||height<=0)continue;
  HDC target=CreateCompatibleDC(memory);HBITMAP surface=CreateCompatibleBitmap(memory,width,height);if(!target||!surface){if(target)DeleteDC(target);if(surface)DeleteObject(surface);continue;}auto previous=SelectObject(target,surface);RECT fill{0,0,width,height};FillRect(target,&fill,GetSysColorBrush(COLOR_WINDOW));SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(target),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
  if(child==picker){auto selected=SendMessageW(child,CB_GETCURSEL,0,0);if(selected!=CB_ERR){wchar_t label[512]{};auto length=SendMessageW(child,CB_GETLBTEXTLEN,static_cast<WPARAM>(selected),0);if(length>=0&&length<512){SendMessageW(child,CB_GETLBTEXT,static_cast<WPARAM>(selected),reinterpret_cast<LPARAM>(label));RECT text{3,3,width-22,height-3};FillRect(target,&text,GetSysColorBrush(COLOR_WINDOW));auto oldFont=SelectObject(target,body);SetBkMode(target,TRANSPARENT);SetTextColor(target,Muted);DrawTextW(target,label,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);SelectObject(target,oldFont);}}}
  BitBlt(memory,bounds.left,bounds.top,width,height,target,0,0,SRCCOPY);SelectObject(target,previous);DeleteObject(surface);DeleteDC(target);
 }
 GdiFlush();for(size_t i=3;i<static_cast<size_t>(rect.right)*static_cast<size_t>(rect.bottom)*4;i+=4)static_cast<BYTE*>(pixels)[i]=255;
 Gdiplus::GdiplusStartupInput startup;ULONG_PTR token=0;bool saved=false;
 if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)==Gdiplus::Ok){
  {UINT count=0,bytes=0;Gdiplus::GetImageEncodersSize(&count,&bytes);if(count&&bytes&&bytes<1024*1024){std::vector<BYTE> data(bytes);auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(data.data());if(Gdiplus::GetImageEncoders(count,bytes,codecs)==Gdiplus::Ok)for(UINT i=0;i<count;++i)if(!wcscmp(codecs[i].MimeType,L"image/png")){
   IStream* stream=nullptr;if(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))){Gdiplus::Bitmap image(bitmap,nullptr);if(image.Save(stream,&codecs[i].Clsid,nullptr)==Gdiplus::Ok){STATSTG stat{};HGLOBAL global=nullptr;if(SUCCEEDED(stream->Stat(&stat,STATFLAG_NONAME))&&stat.cbSize.QuadPart>0&&stat.cbSize.QuadPart<64*1024*1024&&SUCCEEDED(GetHGlobalFromStream(stream,&global))){void* encoded=GlobalLock(global);if(encoded){HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){DWORD written=0;saved=WriteFile(file,encoded,static_cast<DWORD>(stat.cbSize.QuadPart),&written,nullptr)&&written==stat.cbSize.QuadPart;CloseHandle(file);if(!saved)DeleteFileW(path);}GlobalUnlock(global);}}}stream->Release();}break;
  }}}
  Gdiplus::GdiplusShutdown(token);
 }
 SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);return saved;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 1;std::wstring output;
 preview=argc==2&&!wcscmp(argv[1],L"--preview");if(argc==3&&!wcscmp(argv[1],L"--render-preview")){preview=true;output=argv[2];}
 bool valid=argc==1||preview;LocalFree(argv);if(!valid)return 2;
 SetProcessDPIAware();INITCOMMONCONTROLSEX c{sizeof(c),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&c);WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=instance;cls.lpfnWndProc=Proc;cls.lpszClassName=L"StorageRecorderWindow";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);if(!RegisterClassExW(&cls))return 1;window=CreateWindowExW(0,cls.lpszClassName,L"Storage I/O Flight Recorder",WS_OVERLAPPEDWINDOW,100,100,1120,820,nullptr,nullptr,instance,nullptr);if(!window)return 1;
 if(!output.empty()){bool rendered=RenderPreview(window,output.c_str());DestroyWindow(window);return rendered?0:1;}
 ShowWindow(window,show);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}return 0;
}
