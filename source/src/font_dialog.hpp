#pragma once
#include "modern.hpp"
#include <dlgs.h>

// Keep the system font picker (installed fonts, preview, custom colors and scripts),
// while applying the same card frame, typography and buttons as the other dialogs.
namespace modern {
struct FontDialog {HFONT font=nullptr;};
inline BOOL CALLBACK restyleFontChild(HWND child,LPARAM param){
    auto* state=(FontDialog*)param;if(GetDlgCtrlID(child)!=stc5)SendMessageW(child,WM_SETFONT,(WPARAM)state->font,TRUE);
    if(GetDlgCtrlID(child)==IDOK||GetDlgCtrlID(child)==IDCANCEL){LONG_PTR style=GetWindowLongPtrW(child,GWL_STYLE);SetWindowLongPtrW(child,GWL_STYLE,(style&~BS_TYPEMASK)|BS_OWNERDRAW);SetWindowSubclass(child,controlProc,91,0);}
    return TRUE;
}
inline LRESULT CALLBACK fontWindowProc(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR ref){
    auto* state=(FontDialog*)ref;
    if(m==WM_NCHITTEST){POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&point);RECT r{};GetClientRect(h,&r);if(point.y<px(h,48)&&point.x<r.right-px(h,44))return HTCAPTION;}
    if(m==WM_COMMAND&&LOWORD(w)==901){SendMessageW(h,WM_COMMAND,IDCANCEL,0);return 0;}
    if(m==WM_DRAWITEM){auto* d=(DRAWITEMSTRUCT*)l;if(d->CtlType==ODT_BUTTON&&(d->CtlID==IDOK||d->CtlID==IDCANCEL||d->CtlID==901)){if(d->CtlID==901){fill(d->hDC,d->rcItem,surface);if(GetPropW(d->hwndItem,L"ModernHot"))roundFill(d->hDC,d->rcItem,px(h,7),soft);glyph(d->hDC,d->rcItem,4,muted,px(h,1));}else button(d,d->CtlID==IDOK);return TRUE;}}
    if(m==WM_CTLCOLORSTATIC&&GetDlgCtrlID((HWND)l)==stc5)return DefSubclassProc(h,m,w,l);
    if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX||m==WM_CTLCOLORDLG){SetTextColor((HDC)w,ink);SetBkColor((HDC)w,surface);SetDCBrushColor((HDC)w,surface);return (LRESULT)GetStockObject(DC_BRUSH);}
    if(m==WM_PAINT){auto result=DefSubclassProc(h,m,w,l);HDC dc=GetDC(h);RECT r{};GetClientRect(h,&r);
        // Preserve the common dialog's sample rendering; paint only the added chrome.
        fill(dc,{0,0,r.right,px(h,52)},surface);label(dc,h,listki::langText(L"Шрифт и цвет текста",L"Text font and color"),{px(h,22),px(h,10),r.right-px(h,52),px(h,48)},17,ink,FW_SEMIBOLD);roundOutline(dc,r,px(h,13),edge);ReleaseDC(h,dc);return result;}
    if(m==WM_NCDESTROY){RemoveWindowSubclass(h,fontWindowProc,93);if(state->font){DeleteObject(state->font);state->font=nullptr;}}
    return DefSubclassProc(h,m,w,l);
}
inline UINT_PTR CALLBACK fontHook(HWND h,UINT m,WPARAM,LPARAM l){
    if(m!=WM_INITDIALOG)return FALSE;auto* cf=(CHOOSEFONTW*)l;auto* state=(FontDialog*)cf->lCustData;state->font=font(h,12);
    RECT old{};GetClientRect(h,&old);
    // Move direct child controls; nested controls keep their own coordinates.
    for(HWND child=GetWindow(h,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,h,(POINT*)&r,2);SetWindowPos(child,nullptr,r.left+px(h,12),r.top+px(h,53),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
    LONG_PTR style=GetWindowLongPtrW(h,GWL_STYLE);SetWindowLongPtrW(h,GWL_STYLE,(style&~(WS_CAPTION|WS_BORDER|DS_MODALFRAME))|WS_CLIPCHILDREN);
    SetWindowLongPtrW(h,GWL_EXSTYLE,GetWindowLongPtrW(h,GWL_EXSTYLE)&~(WS_EX_DLGMODALFRAME|WS_EX_CLIENTEDGE));
    SetWindowPos(h,nullptr,0,0,old.right+px(h,24),old.bottom+px(h,65),SWP_NOMOVE|SWP_NOZORDER|SWP_FRAMECHANGED);
    center(h,cf->hwndOwner,old.right+px(h,24),old.bottom+px(h,65));roundedRegion(h,28);EnumChildWindows(h,restyleFontChild,(LPARAM)state);
    RECT r{};GetClientRect(h,&r);addButton(h,listki::langText(L"Закрыть",L"Close"),901,{r.right-px(h,44),px(h,18),r.right-px(h,18),px(h,44)},state->font);
    SetWindowSubclass(h,fontWindowProc,93,(DWORD_PTR)state);SendMessageW(h,DM_SETDEFID,IDOK,0);return FALSE;
}
}
