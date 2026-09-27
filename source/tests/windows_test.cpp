#define LISTKI_TEST
#include "../src/app.cpp"
#include <iostream>
static int checks=0;
void check(bool condition,const char* name){if(!condition){std::cerr<<"FAIL: "<<name<<"\n";ExitProcess(2);}++checks;std::cout<<"PASS: "<<name<<"\n";}
void pump(){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 1;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Gdiplus::GdiplusStartupInput gd;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gd,nullptr);
    app.instance=GetModuleHandleW(nullptr);app.data=argv[1];CreateDirectoryW(app.data.c_str(),nullptr);app.icon=LoadIconW(nullptr,IDI_APPLICATION);
    check(LoadLibraryW(L"Msftedit.dll")!=nullptr,"system RichEdit loads");INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ic);
    registerClass(L"ListkiTestHost",hostProc,0);registerClass(L"ListkiHub",hubProc);registerClass(L"ListkiPetal",petalProc);registerClass(L"ListkiNote",noteProc);registerClass(L"ListkiPopup",popupProc);registerClass(L"ListkiFormatBar",formatBarProc);
    app.host=CreateWindowW(L"ListkiTestHost",L"test",WS_POPUP,0,0,1,1,nullptr,nullptr,app.instance,nullptr);
    app.hub=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED,L"ListkiHub",L"Test Hub",WS_POPUP,1280,400,72,72,app.host,nullptr,app.instance,nullptr);
    for(int i=0;i<4;++i)app.petals[i]=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_NOACTIVATE,L"ListkiPetal",L"",WS_POPUP,0,0,124,51,app.hub,nullptr,app.instance,(void*)(INT_PTR)i);
    app.loading=false;ShowWindow(app.hub,SW_SHOWNOACTIVATE);SetCursorPos(920,300);newNote();pump();
    check(app.state.notes.size()==1,"new note created");auto id=app.state.notes[0].id;auto* nw=app.windows[id].get();check(nw&&nw->h&&nw->edit,"note and editor created");
    setLanguage(Language::English);check(noteTitle(*app.note(id))==L"New note"&&std::wstring(langText(L"Русский",L"English"))==L"English","English language switches note placeholder and labels");
    setLanguage(Language::Russian);check(noteTitle(*app.note(id))==L"Новая записка"&&std::wstring(langText(L"Русский",L"English"))==L"Русский","Russian language switches back");
    check(nw->titleEdit&&IsWindowVisible(nw->titleEdit),"title is an inline edit control");
    check(windowText(nw->titleEdit).empty()&&windowText(nw->h)==L"Новая записка","untitled note uses an empty editable field with default caption");
    beginTitleEdit(nw);SetWindowTextW(nw->titleEdit,L"  План на завтра  ");
    check(app.note(id)->title==utf8(L"  План на завтра  ")&&nw->titleDirty&&!nw->dirty,"typing a title updates metadata without dirtying RTF");
    SendMessageW(nw->titleEdit,WM_KEYDOWN,VK_RETURN,0);check(windowText(nw->titleEdit)==L"План на завтра"&&GetFocus()==nw->edit,"Enter commits inline title and returns to body");
    check(saveState()&&!nw->titleDirty,"inline title metadata saved");auto titleState=decode(readBytes(app.data+L"\\index.dat"));check(titleState.notes[0].title==utf8(L"План на завтра"),"custom title survives metadata serialization");
    beginTitleEdit(nw);SetWindowTextW(nw->titleEdit,L"Отменённое изменение");SendMessageW(nw->titleEdit,WM_KEYDOWN,VK_ESCAPE,0);
    check(app.note(id)->title==utf8(L"План на завтра")&&windowText(nw->titleEdit)==L"План на завтра","Escape restores title from start of editing");
    beginTitleEdit(nw);SetWindowTextW(nw->titleEdit,L"   ");SendMessageW(nw->titleEdit,WM_KEYDOWN,VK_RETURN,0);
    check(app.note(id)->title.empty()&&windowText(nw->h)==L"Новая записка","clearing the title restores the default");
    HMENU menu=CreatePopupMenu(),submenu=CreatePopupMenu();AppendMenuW(submenu,MF_STRING|MF_CHECKED,902,L"Подменю");AppendMenuW(menu,MF_STRING,901,L"Копировать\tCtrl+C");AppendMenuW(menu,MF_POPUP,(UINT_PTR)submenu,L"Размер");
    modern::MenuState menuState;menuState.owner=nw->h;menuState.background=CreateSolidBrush(modern::surface);modern::prepareMenu(menuState,menu);
    check(GetMenuItemID(menu,0)==901&&GetSubMenu(menu,1)==submenu&&GetMenuItemID(submenu,0)==902,"menu styling retains commands and nested menus");
    check(GetMenuState(submenu,0,MF_BYPOSITION)&MF_CHECKED,"menu styling retains check state");modern::restoreMenu(menuState);wchar_t menuText[80]{};GetMenuStringW(menu,0,menuText,80,MF_BYPOSITION);
    check(std::wstring(menuText)==L"Копировать\tCtrl+C","menu strings survive styling and restoration");DeleteObject(menuState.background);DestroyMenu(menu);
    std::wstring content=L"Русская записка — проверка 😊\r\nВторая строка\r\nThird line";SetWindowTextW(nw->edit,content.c_str());auto expected=windowText(nw->edit);pump();check(nw->dirty,"text edits mark content dirty");
    updatePreview(nw);check(windowText(nw->h)==L"Новая записка","typing does not change the default title");
    SendMessageW(nw->edit,EM_SETSEL,0,7);updateFormatBar(nw);check(nw->formatBar&&IsWindowVisible(nw->formatBar),"selection shows formatting toolbar");
    check(SendMessageW(nw->formatBar,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE,"toolbar preserves editor activation");
    formatBarAction(nw,2);CHARRANGE selection{};SendMessageW(nw->edit,EM_EXGETSEL,0,(LPARAM)&selection);check(selection.cpMin==0&&selection.cpMax==7,"toolbar retains exact selected range");
    check((currentFormat(nw).dwEffects&CFE_ITALIC)!=0,"toolbar italic applies to selection");formatBarAction(nw,2);
    SendMessageW(nw->edit,EM_SETSEL,8,8);updateFormatBar(nw);check(!IsWindowVisible(nw->formatBar),"empty selection hides formatting toolbar");
    noteCommand(nw,CMD_COLOR+2);check(app.note(id)->color==2,"header color commands update the note");
    SendMessageW(nw->edit,EM_SETSEL,0,7);noteCommand(nw,CMD_BOLD);noteCommand(nw,CMD_SIZE+6);check((currentFormat(nw).dwEffects&CFE_BOLD)!=0,"bold selection applied");check(currentFormat(nw).yHeight==400,"20-point selection applied");
    check(flushAll(),"RTF and metadata saved");auto rtf=readBytes(app.path(id));check(rtf.find("\\b")!=std::string::npos,"RTF contains rich formatting");check(exists(app.path(id)+L".bak"),"previous RTF saved as backup");
    SendMessageW(nw->edit,EM_SETSEL,-1,-1);noteCommand(nw,CMD_ITALIC);SendMessageW(nw->edit,EM_SETSEL,0,7);check((currentFormat(nw).dwEffects&CFE_ITALIC)!=0,"no-selection formatting applies to whole note");
    pinNote(nw);check((GetWindowLongPtrW(nw->h,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0,"pin is truly topmost");pinNote(nw);check((GetWindowLongPtrW(nw->h,GWL_EXSTYLE)&WS_EX_TOPMOST)==0,"unpin removes topmost");
    MoveWindow(nw->h,220,160,420,340,TRUE);pump();check(app.note(id)->x==220&&app.note(id)->w==420,"move and resize update metadata");
    app.state.books.push_back({makeId(),utf8(L"Наука 🧭")});std::string book=app.state.books.back().id;noteCommand(nw,CMD_BOOK);check(app.note(id)->book==book,"note assigned to notebook");
    SendMessageW(nw->h,WM_CLOSE,0,0);check(!IsWindowVisible(nw->h)&&!app.note(id)->visible,"close hides note without deleting it");showNote(id);check(app.windows.size()==1&&IsWindowVisible(nw->h),"reopen reuses note window");
    check(OpenClipboard(app.hub)!=0,"clipboard opened");EmptyClipboard();const wchar_t* clip=L"Из буфера — 中文 — emoji 🌊";size_t sz=(wcslen(clip)+1)*sizeof(wchar_t);HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,sz);void* dst=GlobalLock(mem);memcpy(dst,clip,sz);GlobalUnlock(mem);SetClipboardData(CF_UNICODETEXT,mem);CloseClipboard();newNote(true);pump();
    check(app.state.notes.size()==2,"clipboard creates second note");auto clipId=app.state.notes.back().id;check(windowText(app.windows[clipId]->edit)==clip,"clipboard Unicode preserved");
    openPopup(true);pump();check(app.popup&&app.popup->ids.size()==2,"recent popup lists notes");SetWindowTextW(app.popup->search,L"буфера");pump();check(app.popup->ids.size()==1&&app.popup->ids[0]==clipId,"popup search filters correctly");selectPopup(0);pump();check(!app.popup&&IsWindowVisible(app.windows[clipId]->h),"select closes popup and opens note");
    openPopup(false,book);pump();check(app.popup&&app.popup->ids.size()==1&&app.popup->ids[0]==id,"notebook lists its notes");closePopup();
    removeBook(app.state,book);check(app.state.notes.size()==2&&app.note(id)->book.empty(),"notebook removal retains notes");
    for(POINT pos:std::vector<POINT>{{0,0},{1350,0},{0,800},{1350,800},{680,410}}){closeMenu();SetWindowPos(app.hub,nullptr,pos.x,pos.y,0,0,SWP_NOSIZE|SWP_NOZORDER);showMenu();RECT work=workArea(pos);for(int i=0;i<4;++i){RECT r{};GetWindowRect(app.petals[i],&r);check(r.left>=work.left&&r.top>=work.top&&r.right<=work.right&&r.bottom<=work.bottom,"radial button stays onscreen");for(int j=0;j<i;++j){RECT other{},inter{};GetWindowRect(app.petals[j],&other);check(!IntersectRect(&inter,&r,&other),"radial buttons do not overlap");}}}closeMenu();
    check(flushAll(),"final flush before restart");auto saved=readBytes(app.data+L"\\index.dat");app.exiting=true;for(auto& p:app.windows)if(p.second->h)DestroyWindow(p.second->h);app.windows.clear();app.state=State{};loadState();app.exiting=false;
    check(app.state.notes.size()==2,"metadata reload finds both notes");showNote(id,false);nw=app.windows[id].get();check(windowText(nw->edit)==expected,"formatted Cyrillic text survives reopen");SendMessageW(nw->edit,EM_SETSEL,0,7);check((currentFormat(nw).dwEffects&CFE_BOLD)!=0&&currentFormat(nw).yHeight==400,"format survives RTF reopen");check(app.note(id)->x==220&&app.note(id)->w==420,"geometry survives reload");
    auto originalData=app.data;app.data=originalData+L"\\missing\\unwritable";nw->dirty=true;app.saveError=true;check(!flushNote(nw)&&nw->dirty,"failed save preserves pending content");app.data=originalData;check(flushAll(),"save retries after directory recovers");
    atomicWrite(app.data+L"\\index.dat","corrupt",false);app.state=State{};check(loadState(),"corrupt metadata restores backup");check(app.state.notes.size()==2,"backup contains both notes");
    std::cout<<"ALL "<<checks<<" WINDOWS INTEGRATION CHECKS PASSED\n";
    app.exiting=true;for(auto& p:app.windows)if(p.second->h)DestroyWindow(p.second->h);app.windows.clear();for(auto h:app.petals)DestroyWindow(h);DestroyWindow(app.hub);DestroyWindow(app.host);Gdiplus::GdiplusShutdown(token);CoUninitialize();return 0;
}
