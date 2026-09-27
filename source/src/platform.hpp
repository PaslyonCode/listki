#pragma once
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <richedit.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include <functional>
#include <cwctype>
#include "i18n.hpp"
#include "model.hpp"

inline std::string utf8(const std::wstring& s) {
    if(s.empty())return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    std::string r(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),&r[0],n,nullptr,nullptr);return r;
}
inline std::wstring wide(const std::string& s) {
    if(s.empty())return {};
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
    std::wstring r(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),&r[0],n);return r;
}
inline std::wstring windowText(HWND h) {
    int n=GetWindowTextLengthW(h);std::wstring s(n+1,0);int got=GetWindowTextW(h,&s[0],n+1);s.resize(got);return s;
}
inline std::wstring trim(std::wstring s) {
    auto a=s.find_first_not_of(L" \t\r\n"),b=s.find_last_not_of(L" \t\r\n");
    return a==std::wstring::npos?L"":s.substr(a,b-a+1);
}
inline std::string makeId() {
    GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot generate identifier");
    const BYTE* p=reinterpret_cast<BYTE*>(&id);std::string s;const char* hex="0123456789abcdef";
    for(int i=0;i<16;++i){s+=hex[p[i]>>4];s+=hex[p[i]&15];}return s;
}
inline uint64_t nowTime() {FILETIME f{};GetSystemTimeAsFileTime(&f);return (uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;}
inline std::wstring dateText(uint64_t n) {
    FILETIME f{DWORD(n),DWORD(n>>32)},local{};SYSTEMTIME s{};FileTimeToLocalFileTime(&f,&local);FileTimeToSystemTime(&local,&s);
    wchar_t b[64];swprintf(b,64,L"%02u.%02u.%04u  %02u:%02u",s.wDay,s.wMonth,s.wYear,s.wHour,s.wMinute);return b;
}
inline bool exists(const std::wstring& path){return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;}
inline std::wstring sysError(DWORD code=GetLastError()) {
    LPWSTR p=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,(LPWSTR)&p,0,nullptr);
    std::wstring s=p?p:listki::langString(L"Ошибка Windows",L"Windows error");if(p)LocalFree(p);return trim(s);
}
inline std::string readBytes(const std::wstring& path,size_t max=64*1024*1024) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open file");
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(f,&size)||size.QuadPart<0||uint64_t(size.QuadPart)>max){CloseHandle(f);throw std::runtime_error("Invalid file size");}
    std::string s(size_t(size.QuadPart),0);DWORD got=0;
    BOOL ok=s.empty()||ReadFile(f,&s[0],DWORD(s.size()),&got,nullptr);CloseHandle(f);
    if(!ok||(!s.empty()&&got!=s.size()))throw std::runtime_error("Cannot read file");return s;
}
inline bool atomicWrite(const std::wstring& path,const std::string& s,bool backup=true) {
    auto temp=path+L".tmp";
    HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return false;
    DWORD n=0;BOOL ok=WriteFile(f,s.data(),DWORD(s.size()),&n,nullptr)&&n==s.size();
    if(ok)ok=FlushFileBuffers(f);DWORD err=GetLastError();CloseHandle(f);
    if(!ok){DeleteFileW(temp.c_str());SetLastError(err);return false;}
    if(exists(path)){
        if(backup){
            auto bak=path+L".bak";
            if(!CopyFileW(path.c_str(),bak.c_str(),FALSE)){err=GetLastError();DeleteFileW(temp.c_str());SetLastError(err);return false;}
        }
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
    }else if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH))return false;
    return true;
}
inline int dpi(HWND h) {
    using Fn=UINT(WINAPI*)(HWND);static auto fn=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
    return fn&&h?int(fn(h)):96;
}
inline int px(HWND h,int x){return MulDiv(x,dpi(h),96);}
inline RECT workArea(POINT p) {MONITORINFO m{};m.cbSize=sizeof(m);GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&m);return m.rcWork;}
inline RECT fitRect(RECT r) {
    RECT w=workArea({r.left+(r.right-r.left)/2,r.top+(r.bottom-r.top)/2});
    int width=std::min(r.right-r.left,w.right-w.left),height=std::min(r.bottom-r.top,w.bottom-w.top);
    r.left=std::clamp<int>(r.left,w.left,w.right-width);r.top=std::clamp<int>(r.top,w.top,w.bottom-height);
    r.right=r.left+width;r.bottom=r.top+height;return r;
}
inline HFONT font(HWND h,int size=14,int weight=FW_NORMAL) {
    return CreateFontW(-px(h,size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
}
inline void label(HDC dc,HWND h,const std::wstring& s,RECT r,int size,COLORREF color,int weight=FW_NORMAL,UINT align=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS) {
    HFONT f=font(h,size,weight);auto old=SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
    DrawTextW(dc,s.c_str(),int(s.size()),&r,align|DT_NOPREFIX);SelectObject(dc,old);DeleteObject(f);
}
inline void fill(HDC dc,RECT r,COLORREF color){auto b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
inline void line(HDC dc,int x,int y,int x2,int y2,COLORREF c,int width=1){auto p=CreatePen(PS_SOLID,width,c);auto old=SelectObject(dc,p);MoveToEx(dc,x,y,nullptr);LineTo(dc,x2,y2);SelectObject(dc,old);DeleteObject(p);}
inline void roundedRegion(HWND h,int radius=16){RECT r{};GetClientRect(h,&r);SetWindowRgn(h,CreateRoundRectRgn(0,0,r.right+1,r.bottom+1,px(h,radius),px(h,radius)),TRUE);}
inline Gdiplus::Color gc(COLORREF c,int a=255){return Gdiplus::Color(a,GetRValue(c),GetGValue(c),GetBValue(c));}
inline COLORREF mixColor(COLORREF a,COLORREF b,int percentB) {
    auto mix=[&](int x,int y){return (x*(100-percentB)+y*percentB)/100;};
    return RGB(mix(GetRValue(a),GetRValue(b)),mix(GetGValue(a),GetGValue(b)),mix(GetBValue(a),GetBValue(b)));
}
inline void circle(HDC dc,RECT r,COLORREF color,COLORREF outline) {
    Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush brush(gc(color));Gdiplus::Pen pen(gc(outline),1.f);
    auto bounds=Gdiplus::RectF(float(r.left)+.5f,float(r.top)+.5f,float(r.right-r.left)-1,float(r.bottom-r.top)-1);
    g.FillEllipse(&brush,bounds);g.DrawEllipse(&pen,bounds);
}
inline void roundFill(HDC dc,RECT r,int radius,COLORREF c) {
    Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::GraphicsPath p;
    float d=float(radius*2),x=float(r.left),y=float(r.top),w=float(r.right-r.left),h=float(r.bottom-r.top);
    p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();Gdiplus::SolidBrush b(gc(c));g.FillPath(&b,&p);
}
inline void roundOutline(HDC dc,RECT r,int radius,COLORREF color) {
    Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::GraphicsPath p;
    float d=float(radius*2),x=float(r.left)+.5f,y=float(r.top)+.5f,w=float(r.right-r.left)-1,h=float(r.bottom-r.top)-1;
    p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();Gdiplus::Pen pen(gc(color),1.f);g.DrawPath(&pen,&p);
}
struct Canvas {
    HWND h;PAINTSTRUCT ps{};HDC screen,dc;HBITMAP bitmap,old;RECT rect;
    explicit Canvas(HWND h):h(h){screen=BeginPaint(h,&ps);GetClientRect(h,&rect);dc=CreateCompatibleDC(screen);bitmap=CreateCompatibleBitmap(screen,std::max(1L,rect.right),std::max(1L,rect.bottom));old=(HBITMAP)SelectObject(dc,bitmap);}
    ~Canvas(){BitBlt(screen,0,0,rect.right,rect.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(h,&ps);}
};
inline void glyph(HDC dc,RECT r,int kind,COLORREF color,int thick=2) {
    Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen pen(gc(color),float(thick));pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);
    float cx=(r.left+r.right)/2.f,cy=(r.top+r.bottom)/2.f,s=std::min(r.right-r.left,r.bottom-r.top)*.30f;
    if(kind==0){g.DrawLine(&pen,cx-s,cy,cx+s,cy);g.DrawLine(&pen,cx,cy-s,cx,cy+s);}
    if(kind==1){g.DrawRectangle(&pen,cx-s,cy-s*.85f,s*2,s*1.9f);g.DrawLine(&pen,cx-s*.35f,cy-s,cx+s*.35f,cy-s);g.DrawLine(&pen,cx-s*.5f,cy,cx+s*.5f,cy);g.DrawLine(&pen,cx-s*.5f,cy+s*.5f,cx+s*.3f,cy+s*.5f);}
    if(kind==2){g.DrawEllipse(&pen,cx-s,cy-s,s*2,s*2);g.DrawLine(&pen,cx,cy-s*.5f,cx,cy);g.DrawLine(&pen,cx,cy,cx+s*.5f,cy+s*.2f);}
    if(kind==3){g.DrawRectangle(&pen,cx-s,cy-s,s*2,s*2);g.DrawLine(&pen,cx-s*.45f,cy-s,cx-s*.45f,cy+s);g.DrawLine(&pen,cx,cy-s*.3f,cx+s*.6f,cy-s*.3f);}
    if(kind==4){g.DrawLine(&pen,cx-s*.7f,cy-s*.7f,cx+s*.7f,cy+s*.7f);g.DrawLine(&pen,cx+s*.7f,cy-s*.7f,cx-s*.7f,cy+s*.7f);}
    if(kind==5){g.DrawLine(&pen,cx-s*.65f,cy-s,cx+s*.65f,cy-s);g.DrawLine(&pen,cx-s*.5f,cy-s,cx-s*.5f,cy);g.DrawLine(&pen,cx+s*.5f,cy-s,cx+s*.5f,cy);g.DrawLine(&pen,cx-s,cy,cx+s,cy);g.DrawLine(&pen,cx,cy,cx,cy+s);}
    if(kind==6){Gdiplus::SolidBrush b(gc(color));for(int i=-1;i<=1;++i)g.FillEllipse(&b,cx+i*s*.85f-1.5f,cy-1.5f,3.f,3.f);}
    if(kind==7){g.DrawLine(&pen,cx+s*.4f,cy-s*.7f,cx-s*.4f,cy);g.DrawLine(&pen,cx-s*.4f,cy,cx+s*.4f,cy+s*.7f);}
    if(kind==8){g.DrawLine(&pen,cx-s,cy-s*.65f,cx+s,cy-s*.65f);g.DrawLine(&pen,cx-s*.35f,cy-s,cx+s*.35f,cy-s);g.DrawLine(&pen,cx-s*.7f,cy-s*.65f,cx-s*.55f,cy+s);g.DrawLine(&pen,cx+s*.7f,cy-s*.65f,cx+s*.55f,cy+s);g.DrawLine(&pen,cx-s*.55f,cy+s,cx+s*.55f,cy+s);g.DrawLine(&pen,cx-s*.2f,cy-s*.15f,cx-s*.2f,cy+s*.55f);g.DrawLine(&pen,cx+s*.2f,cy-s*.15f,cx+s*.2f,cy+s*.55f);}
    if(kind==9){g.DrawLine(&pen,cx-s*.7f,cy-s*.25f,cx,cy+s*.4f);g.DrawLine(&pen,cx,cy+s*.4f,cx+s*.7f,cy-s*.25f);}
    if(kind==11){g.DrawEllipse(&pen,cx-s,cy-s,s*1.5f,s*1.5f);g.DrawLine(&pen,cx+s*.3f,cy+s*.3f,cx+s,cy+s);}
    if(kind==10){g.DrawLine(&pen,cx-s*.7f,cy,cx-s*.15f,cy+s*.55f);g.DrawLine(&pen,cx-s*.15f,cy+s*.55f,cx+s*.8f,cy-s*.65f);}
}
