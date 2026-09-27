#pragma once
#include "platform.hpp"
#include "i18n.hpp"

// Shared drawing for menus, modal cards and secondary controls.
namespace modern {
constexpr COLORREF surface=RGB(253,254,252),ink=RGB(39,55,51),muted=RGB(112,127,120),accent=RGB(42,108,90),soft=RGB(238,245,240),edge=RGB(219,229,221),danger=RGB(173,67,60);
inline LRESULT CALLBACK controlProc(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(m==WM_MOUSEMOVE){if(!GetPropW(h,L"ModernHot")){SetPropW(h,L"ModernHot",(HANDLE)1);InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);}
    if(m==WM_MOUSELEAVE){RemovePropW(h,L"ModernHot");InvalidateRect(h,nullptr,FALSE);}
    if(m==WM_SETFOCUS||m==WM_KILLFOCUS){InvalidateRect(h,nullptr,FALSE);InvalidateRect(GetParent(h),nullptr,FALSE);}
    if(m==WM_NCDESTROY)RemovePropW(h,L"ModernHot");
    return DefSubclassProc(h,m,w,l);
}
inline void button(DRAWITEMSTRUCT* d,bool primary=false,bool destructive=false){
    HWND h=d->hwndItem;RECT r=d->rcItem;bool hot=GetPropW(h,L"ModernHot")!=nullptr,down=d->itemState&ODS_SELECTED;
    fill(d->hDC,r,surface);InflateRect(&r,-px(h,1),-px(h,1));
    COLORREF base=primary?(destructive?danger:accent):soft;
    COLORREF bg=down?mixColor(base,ink,12):hot?mixColor(base,primary?RGB(255,255,255):edge,12):base;
    roundFill(d->hDC,r,px(h,9),bg);
    if(!primary)roundOutline(d->hDC,r,px(h,9),edge);
    label(d->hDC,h,windowText(h),r,13,primary?RGB(255,255,255):ink,FW_SEMIBOLD,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(d->itemState&ODS_FOCUS){InflateRect(&r,-px(h,3),-px(h,3));roundOutline(d->hDC,r,px(h,6),primary?mixColor(bg,RGB(255,255,255),65):accent);}
}
inline HWND addButton(HWND h,const wchar_t* text,int id,RECT r,HFONT f){
    HWND b=CreateWindowW(L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,r.left,r.top,r.right-r.left,r.bottom-r.top,h,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(b,WM_SETFONT,(WPARAM)f,TRUE);SetWindowSubclass(b,controlProc,91,0);return b;
}
struct MenuItem {HMENU menu=nullptr;UINT position=0,type=0;ULONG_PTR data=0;std::wstring original,text,shortcut;bool submenu=false,destructive=false;int width=0;};
struct MenuState {HWND owner=nullptr;HBRUSH background=nullptr;std::vector<std::unique_ptr<MenuItem>> items;std::vector<std::pair<HMENU,MENUINFO>> menus;};
inline MenuItem* ownItem(MenuState* state,ULONG_PTR data){for(auto& i:state->items)if((ULONG_PTR)i.get()==data)return i.get();return nullptr;}
inline void prepareMenu(MenuState& state,HMENU menu){
    MENUINFO old{};old.cbSize=sizeof(old);old.fMask=MIM_BACKGROUND|MIM_STYLE|MIM_MAXHEIGHT;GetMenuInfo(menu,&old);state.menus.push_back({menu,old});
    MENUINFO info=old;info.hbrBack=state.background;info.dwStyle|=MNS_NOCHECK;POINT p{};GetCursorPos(&p);RECT work=workArea(p);info.cyMax=work.bottom-work.top-px(state.owner,16);SetMenuInfo(menu,&info);
    HDC dc=GetDC(state.owner);HFONT f=font(state.owner,13);auto oldFont=SelectObject(dc,f);
    for(int i=0;i<GetMenuItemCount(menu);++i){
        MENUITEMINFOW mi{};mi.cbSize=sizeof(mi);mi.fMask=MIIM_FTYPE|MIIM_DATA|MIIM_SUBMENU|MIIM_STRING;GetMenuItemInfoW(menu,i,TRUE,&mi);
        if(mi.hSubMenu)prepareMenu(state,mi.hSubMenu);
        if(mi.fType&(MFT_SEPARATOR|MFT_OWNERDRAW))continue;
        std::vector<wchar_t> name(mi.cch+1);mi.dwTypeData=name.data();mi.cch=UINT(name.size());GetMenuItemInfoW(menu,i,TRUE,&mi);
        auto item=std::make_unique<MenuItem>();item->menu=menu;item->position=i;item->type=mi.fType;item->data=mi.dwItemData;item->original=name.data();item->text=item->original;item->submenu=mi.hSubMenu!=nullptr;
        auto tab=item->text.find(L'\t');if(tab!=std::wstring::npos){item->shortcut=item->text.substr(tab+1);item->text.resize(tab);}
        item->destructive=item->text.find(L"Удалить")==0||item->text.find(L"Delete")==0;
        SelectObject(dc,f);SIZE a{},b{};GetTextExtentPoint32W(dc,item->text.c_str(),int(item->text.size()),&a);GetTextExtentPoint32W(dc,item->shortcut.c_str(),int(item->shortcut.size()),&b);
        item->width=std::clamp(int(a.cx+b.cx)+px(state.owner,item->shortcut.empty()?60:85),px(state.owner,158),std::min(px(state.owner,470),int(work.right-work.left)-px(state.owner,30)));
        MENUITEMINFOW set{};set.cbSize=sizeof(set);set.fMask=MIIM_FTYPE|MIIM_DATA;set.fType=mi.fType|MFT_OWNERDRAW;set.dwItemData=(ULONG_PTR)item.get();SetMenuItemInfoW(menu,i,TRUE,&set);state.items.push_back(std::move(item));
    }
    SelectObject(dc,oldFont);DeleteObject(f);ReleaseDC(state.owner,dc);
}
inline void restoreMenu(MenuState& state){
    for(auto& item:state.items){MENUITEMINFOW mi{};mi.cbSize=sizeof(mi);mi.fMask=MIIM_FTYPE|MIIM_DATA|MIIM_STRING;mi.fType=item->type;mi.dwItemData=item->data;mi.dwTypeData=const_cast<wchar_t*>(item->original.c_str());SetMenuItemInfoW(item->menu,item->position,TRUE,&mi);}
    for(auto& menu:state.menus)SetMenuInfo(menu.first,&menu.second);
}
inline LRESULT CALLBACK menuProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR ref){
    auto* state=(MenuState*)ref;
    if(msg==WM_MEASUREITEM){auto* d=(MEASUREITEMSTRUCT*)l;if(d->CtlType==ODT_MENU)if(auto* item=ownItem(state,d->itemData)){d->itemWidth=item->width;d->itemHeight=px(h,33);return TRUE;}}
    if(msg==WM_DRAWITEM){auto* d=(DRAWITEMSTRUCT*)l;if(d->CtlType==ODT_MENU)if(auto* item=ownItem(state,d->itemData)){
        RECT r=d->rcItem;fill(d->hDC,r,surface);bool active=d->itemState&ODS_SELECTED,disabled=d->itemState&(ODS_DISABLED|ODS_GRAYED);
        if(active){RECT hover=r;InflateRect(&hover,-px(h,4),-px(h,2));roundFill(d->hDC,hover,px(h,6),item->destructive?RGB(251,237,234):soft);}
        COLORREF color=disabled?mixColor(muted,surface,42):item->destructive?danger:ink;
        int end=r.right-px(h,25);if(!item->shortcut.empty()){
            RECT key{r.left,r.top,end,r.bottom};label(d->hDC,h,item->shortcut,key,11,disabled?mixColor(muted,surface,42):muted,FW_NORMAL,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
            HDC dc=d->hDC;HFONT f=font(h,11);auto old=SelectObject(dc,f);SIZE size{};GetTextExtentPoint32W(dc,item->shortcut.c_str(),int(item->shortcut.size()),&size);SelectObject(dc,old);DeleteObject(f);end-=size.cx+px(h,20);
        }
        label(d->hDC,h,item->text,{r.left+px(h,32),r.top,end,r.bottom},13,color);
        if(d->itemState&ODS_CHECKED)glyph(d->hDC,{r.left+px(h,10),r.top+px(h,8),r.left+px(h,26),r.bottom-px(h,8)},10,disabled?muted:accent,px(h,1));
        // Windows draws submenu arrows in the reserved trailing gutter.
        return TRUE;
    }}
    if(msg==WM_MENUCHAR){HMENU menu=(HMENU)l;UINT match=0,count=0;wchar_t key=towlower(LOWORD(w));for(auto& i:state->items)if(i->menu==menu&&!i->text.empty()&&towlower(i->text[0])==key){match=i->position;++count;}if(count)return MAKELRESULT(match,count==1?MNC_EXECUTE:MNC_SELECT);}
    return DefSubclassProc(h,msg,w,l);
}
struct Card {
    std::wstring title,text,value;UINT flags=0;bool input=false,destructive=false;
    HFONT font=nullptr;HWND field=nullptr;int height=0,bodyHeight=0;bool closeHot=false;
    int positive()const{return flags&MB_YESNO?IDYES:IDOK;}
    int negative()const{return flags&MB_YESNO?IDNO:input?IDCANCEL:IDOK;}
};
inline int textHeight(HWND h,const std::wstring& text,int width,HFONT f){
    HDC dc=GetDC(h);auto old=SelectObject(dc,f);RECT r{0,0,width,0};DrawTextW(dc,text.c_str(),int(text.size()),&r,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);ReleaseDC(h,dc);return r.bottom;
}
inline void center(HWND h,HWND owner,int width,int height){
    RECT at{};if(owner)GetWindowRect(owner,&at);else{POINT p{};GetCursorPos(&p);at=workArea(p);}RECT r=fitRect({(at.left+at.right-width)/2,(at.top+at.bottom-height)/2,(at.left+at.right+width)/2,(at.top+at.bottom+height)/2});
    SetWindowPos(h,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,0);
}
inline INT_PTR CALLBACK cardProc(HWND h,UINT m,WPARAM w,LPARAM l){
    auto* p=(Card*)GetWindowLongPtrW(h,DWLP_USER);
    if(m==WM_INITDIALOG){p=(Card*)l;SetWindowLongPtrW(h,DWLP_USER,l);SetWindowTextW(h,p->title.c_str());p->font=font(h,14);
        RECT work{};HWND owner=GetWindow(h,GW_OWNER);if(owner)GetWindowRect(owner,&work);else {POINT cursor{};GetCursorPos(&cursor);work.left=cursor.x;work.top=cursor.y;}RECT screen=workArea({work.left,work.top});
        int width=std::min(px(h,434),int(screen.right-screen.left)-px(h,24));
        p->bodyHeight=p->input?px(h,67):std::clamp(textHeight(h,p->text,width-px(h,48),p->font)+px(h,8),px(h,42),std::max(px(h,42),int(screen.bottom-screen.top)-px(h,210)));
        p->height=px(h,150)+p->bodyHeight;center(h,owner,width,p->height);roundedRegion(h,28);
        if(p->input){p->field=CreateWindowW(L"EDIT",p->value.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,px(h,35),px(h,102),width-px(h,70),px(h,23),h,(HMENU)10,GetModuleHandleW(nullptr),nullptr);SendMessageW(p->field,EM_SETLIMITTEXT,160,0);SendMessageW(p->field,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);SetWindowSubclass(p->field,controlProc,91,0);}
        else {bool scroll=textHeight(h,p->text,width-px(h,48),p->font)>p->bodyHeight;p->field=CreateWindowW(L"EDIT",p->text.c_str(),WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|(scroll?WS_VSCROLL|ES_AUTOVSCROLL:0),px(h,24),px(h,80),width-px(h,48),p->bodyHeight,h,(HMENU)10,GetModuleHandleW(nullptr),nullptr);SendMessageW(p->field,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);}
        SendMessageW(p->field,WM_SETFONT,(WPARAM)p->font,TRUE);
        bool two=p->input||(p->flags&MB_YESNO);int bottom=p->height-px(h,22),top=bottom-px(h,37);
        addButton(h,p->input?listki::langText(L"Сохранить",L"Save"):p->destructive?listki::langText(L"Удалить",L"Delete"):two?listki::langText(L"Да",L"Yes"):listki::langText(L"Понятно",L"OK"),IDOK,{width-px(h,144),top,width-px(h,24),bottom},p->font);
        if(two)addButton(h,p->input||p->destructive?listki::langText(L"Отмена",L"Cancel"):listki::langText(L"Нет",L"No"),IDCANCEL,{width-px(h,272),top,width-px(h,152),bottom},p->font);
        bool cancelDefault=(p->flags&MB_DEFBUTTON2)!=0;SendMessageW(h,DM_SETDEFID,cancelDefault?IDCANCEL:IDOK,0);
        SetFocus(p->input?p->field:GetDlgItem(h,cancelDefault?IDCANCEL:IDOK));if(p->input)SendMessageW(p->field,EM_SETSEL,0,-1);return FALSE;
    }
    if(!p)return FALSE;
    switch(m){
    case WM_COMMAND:if(LOWORD(w)==IDOK){if(p->input){p->value=trim(windowText(p->field));if(p->value.empty()){SetFocus(p->field);MessageBeep(MB_ICONINFORMATION);return TRUE;}}EndDialog(h,p->positive());return TRUE;}if(LOWORD(w)==IDCANCEL){EndDialog(h,p->negative());return TRUE;}break;
    case WM_CLOSE:EndDialog(h,p->negative());return TRUE;
    case WM_NCHITTEST:{POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&pt);RECT r{};GetClientRect(h,&r);if(pt.y<px(h,62)&&pt.x<r.right-px(h,50)){SetWindowLongPtrW(h,DWLP_MSGRESULT,HTCAPTION);return TRUE;}return FALSE;}
    case WM_MOUSEMOVE:{RECT r{};GetClientRect(h,&r);bool hot=GET_X_LPARAM(l)>r.right-px(h,50)&&GET_Y_LPARAM(l)<px(h,58);if(hot!=p->closeHot){p->closeHot=hot;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);break;}
    case WM_MOUSELEAVE:p->closeHot=false;InvalidateRect(h,nullptr,FALSE);break;
    case WM_LBUTTONUP:if(p->closeHot){EndDialog(h,p->negative());return TRUE;}break;
    case WM_DRAWITEM:{auto* d=(DRAWITEMSTRUCT*)l;button(d,d->CtlID==IDOK,p->destructive);return TRUE;}
    case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:{bool field=p->input&&(HWND)l==p->field;COLORREF bg=field?soft:surface;SetTextColor((HDC)w,ink);SetBkColor((HDC)w,bg);SetDCBrushColor((HDC)w,bg);return (INT_PTR)GetStockObject(DC_BRUSH);}
    case WM_ERASEBKGND:return TRUE;
    case WM_PAINT:{Canvas c(h);fill(c.dc,c.rect,surface);int ww=c.rect.right;
        roundFill(c.dc,{px(h,22),px(h,22),px(h,56),px(h,56)},px(h,10),p->destructive?RGB(251,237,234):soft);glyph(c.dc,{px(h,28),px(h,28),px(h,50),px(h,50)},p->input?3:p->destructive?8:1,p->destructive?danger:accent,px(h,1));
        label(c.dc,h,p->title,{px(h,67),px(h,20),ww-px(h,52),px(h,59)},17,ink,FW_SEMIBOLD);
        RECT close{ww-px(h,46),px(h,24),ww-px(h,20),px(h,50)};if(p->closeHot)roundFill(c.dc,close,px(h,7),soft);glyph(c.dc,close,4,muted,px(h,1));
        if(p->input){label(c.dc,h,listki::langText(L"Название блокнота",L"Notebook name"),{px(h,24),px(h,70),ww-px(h,24),px(h,90)},11,muted);RECT field{px(h,24),px(h,93),ww-px(h,24),px(h,136)};roundFill(c.dc,field,px(h,9),soft);roundOutline(c.dc,field,px(h,9),GetFocus()==p->field?mixColor(accent,surface,35):edge);}
        roundOutline(c.dc,c.rect,px(h,13),edge);return TRUE;
    }
    case WM_DESTROY:if(p->font)DeleteObject(p->font);break;
    }
    return FALSE;
}
inline INT_PTR showCard(HWND owner,Card& p){
    struct Template {DLGTEMPLATE dialog;WORD menu,klass,title;} t{};t.dialog.style=WS_POPUP|WS_SYSMENU|WS_CLIPCHILDREN|DS_CENTER;t.dialog.dwExtendedStyle=WS_EX_TOOLWINDOW;t.dialog.cx=270;t.dialog.cy=150;
    return DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&t.dialog,owner,cardProc,(LPARAM)&p);
}
}
inline UINT styledPopupMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND owner,const RECT* rect){
    modern::MenuState state;state.owner=owner;state.background=CreateSolidBrush(modern::surface);modern::prepareMenu(state,menu);
    UINT_PTR id=(UINT_PTR)&state;SetWindowSubclass(owner,modern::menuProc,id,(DWORD_PTR)&state);
    UINT result=TrackPopupMenu(menu,flags,x,y,reserved,owner,rect);
    RemoveWindowSubclass(owner,modern::menuProc,id);modern::restoreMenu(state);DeleteObject(state.background);return result;
}
inline bool prompt(HWND owner,const std::wstring& title,std::wstring& value){
    modern::Card p;p.title=title;p.value=value;p.input=true;auto result=modern::showCard(owner,p);if(result==IDOK){value=p.value;return true;}return false;
}
inline int messageCard(HWND owner,const wchar_t* text,const wchar_t* title,UINT flags){
    modern::Card p;p.title=title;p.text=text;p.flags=flags;p.destructive=(flags&MB_YESNO)&&(p.title.find(L"Удаление")!=std::wstring::npos||p.title.find(L"Delete")!=std::wstring::npos);
    int result=int(modern::showCard(owner,p));return result==-1?MessageBoxW(owner,text,title,flags):result;
}
