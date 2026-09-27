#include "platform.hpp"
#include "note_ui.hpp"
#include "i18n.hpp"
#include "modern.hpp"
#include "font_dialog.hpp"
#include <cstdio>

using namespace listki;
constexpr COLORREF INK=modern::ink,MUTED=modern::muted,TEAL=modern::accent,PAPER=modern::surface;
const COLORREF noteColors[]={RGB(255,248,219),RGB(230,244,233),RGB(229,240,250),RGB(244,232,245),RGB(249,247,242)};
constexpr UINT MSG_TRAY=WM_APP+1,MSG_SHOW=WM_APP+2;
constexpr int CMD_NEW=100,CMD_CLIP=101,CMD_RECENT=102,CMD_BOOKS=103,CMD_SHOW=104,CMD_HIDE=105,CMD_AUTORUN=106,CMD_EXIT=107,CMD_FOLDER=108,CMD_RESET=109,CMD_ABOUT=110;
constexpr int CMD_UNDO=200,CMD_REDO=201,CMD_CUT=202,CMD_COPY=203,CMD_PASTE=204,CMD_SELECT=205,CMD_FONT=206,CMD_BOLD=207,CMD_ITALIC=208,CMD_UNDERLINE=209,CMD_PIN=210,CMD_RENAME=211,CMD_DELETE=212,CMD_NEWBOOK=213,CMD_NOGROUP=214,CMD_CLOSE=215;
constexpr int CMD_SIZE=300,CMD_COLOR=400,CMD_LANG_RU=620,CMD_LANG_EN=621,CMD_BOOK=1000;
const int fontSizes[]={10,11,12,14,16,18,20,24,28,32,40};
struct NoteWindow {
    std::string id;
    HWND h=nullptr,edit=nullptr,titleEdit=nullptr,formatBar=nullptr,tips=nullptr;
    HFONT titleFont=nullptr;std::wstring originalTitle;bool titleSync=false,titleDirty=false;
    bool loading=true,dirty=false,error=false,loadFailed=false,selecting=false,moving=false,formatMenu=false;
    int hot=0,pressed=0,formatHot=0,formatPressed=0,formatDepth=0;
    CHARRANGE formatRange{0,0};
    unsigned suppressChar[32]{};
};
struct Popup {HWND h=nullptr,list=nullptr,search=nullptr;HFONT f=nullptr;std::string book;bool recent=false,books=false;int hot=-1,actionHot=0,pressed=0;std::vector<std::string> ids;};
struct App {
    HINSTANCE instance=nullptr;HWND host=nullptr,hub=nullptr,petals[4]{};HICON icon=nullptr;
    State state;std::map<std::string,std::unique_ptr<NoteWindow>> windows;std::unique_ptr<Popup> popup;
    std::wstring exe,dir,data,hostClass;HANDLE mutex=nullptr;bool menuOpen=false,exiting=false,loading=true,saveError=false;UINT taskbarMessage=0;int modal=0;
    Note* note(const std::string& id){for(auto& n:state.notes)if(n.id==id)return &n;return nullptr;}
    std::wstring path(const std::string& id){return data+L"\\"+wide(id)+L".rtf";}
} app;

LRESULT CALLBACK hostProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK hubProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK petalProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK noteProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK popupProc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK formatBarProc(HWND,UINT,WPARAM,LPARAM);
void closeMenu();void showMenu();void showNote(const std::string&,bool=true);void newNote(bool=false,const std::string& ={});
void noteMenu(NoteWindow*,POINT);void trayMenu(POINT);void notebooksMenu(POINT);
void openPopup(bool,const std::string& ={},bool=false);void closePopup();void refreshPopup();void populatePopup();
bool saveState();bool flushNote(NoteWindow*);void layoutNote(NoteWindow*);void applyOpacity(NoteWindow*);void exitApp();
void hideFormatBar(NoteWindow*);void requestFormatBar(NoteWindow*);void updateFormatBar(NoteWindow*);
void colorDropdown(NoteWindow*);void layoutNoteTools(NoteWindow*);
void commitTitle(NoteWindow*);void beginTitleEdit(NoteWindow*);
void updateTrayTip();

std::wstring bookTitle(const std::string& id){for(const auto& b:app.state.books)if(b.id==id)return wide(b.title);return langString(L"Без блокнота",L"No notebook");}
std::wstring noteTitle(const Note& n){return n.title.empty()?langString(L"Новая записка",L"New note"):wide(n.title);}
std::wstring dataLanguagePath(){return app.data+L"\\language.ini";}
void loadLanguage(){
    gLanguage=Language::Russian;
    try{auto value=readBytes(dataLanguagePath());if(value.size()>=2&&(value[0]=='e'||value[0]=='E')&&(value[1]=='n'||value[1]=='N'))gLanguage=Language::English;}catch(...){ }
}
void saveLanguage(){if(!app.data.empty())atomicWrite(dataLanguagePath(),utf8(languageFileValue()),false);}
void refreshLanguage(){
    for(auto& item:app.windows){auto* nw=item.second.get();if(!nw->h)continue;auto* n=app.note(nw->id);
        SetWindowTextW(nw->h,n?noteTitle(*n).c_str():L"");
        if(nw->titleEdit)SendMessageW(nw->titleEdit,EM_SETCUEBANNER,TRUE,(LPARAM)langText(L"Новая записка",L"New note"));
        layoutNoteTools(nw);InvalidateRect(nw->h,nullptr,TRUE);
    }
    if(app.popup){SendMessageW(app.popup->search,EM_SETCUEBANNER,TRUE,(LPARAM)(app.popup->books?langText(L"Найти блокнот…",L"Find a notebook…"):langText(L"Найти записку…",L"Find a note…")));populatePopup();InvalidateRect(app.popup->h,nullptr,TRUE);}
    for(auto h:app.petals)if(h)InvalidateRect(h,nullptr,TRUE);
    if(app.hub)InvalidateRect(app.hub,nullptr,TRUE);
    updateTrayTip();
}
void setLanguage(Language language){if(gLanguage==language)return;gLanguage=language;saveLanguage();refreshLanguage();}
void notify(const std::wstring& text,DWORD flags=NIIF_INFO){NOTIFYICONDATAW ni{};ni.cbSize=sizeof(ni);ni.hWnd=app.host;ni.uID=1;ni.uFlags=NIF_INFO;ni.dwInfoFlags=flags;wcsncpy(ni.szInfoTitle,langText(L"Листки",L"Listki"),63);wcsncpy(ni.szInfo,text.c_str(),255);Shell_NotifyIconW(NIM_MODIFY,&ni);}
void storageError(){if(!app.saveError){app.saveError=true;notify(langString(L"Не удалось сохранить данные. Проверьте свободное место и доступ к папке рядом с EXE. Текст остаётся в открытых окнах.",L"Could not save data. Check free space and access to the folder next to the EXE. Text remains in open notes."),NIIF_ERROR);}}
void scheduleSave(){if(!app.loading&&!app.exiting)SetTimer(app.host,1,600,nullptr);}
bool saveState(){if(app.loading)return true;bool ok=atomicWrite(app.data+L"\\index.dat",encode(app.state));if(!ok)storageError();else {app.saveError=false;for(auto& item:app.windows){auto* nw=item.second.get();if(nw->titleDirty){nw->titleDirty=false;if(nw->h)InvalidateRect(nw->h,nullptr,FALSE);}}}return ok;}
bool flushAll(){bool ok=true;for(auto& p:app.windows)if(p.second->h&&!flushNote(p.second.get()))ok=false;return saveState()&&ok;}

DWORD CALLBACK streamOut(DWORD_PTR cookie,LPBYTE data,LONG cb,LONG* wrote){auto* s=(std::string*)cookie;try{s->append((char*)data,cb);*wrote=cb;return 0;}catch(...){*wrote=0;return 1;}}
struct Input {const std::string* s;size_t at=0;};
DWORD CALLBACK streamIn(DWORD_PTR cookie,LPBYTE data,LONG cb,LONG* got){auto* p=(Input*)cookie;*got=LONG(std::min<size_t>(cb,p->s->size()-p->at));memcpy(data,p->s->data()+p->at,*got);p->at+=*got;return 0;}
std::string getRtf(HWND edit){std::string s;EDITSTREAM es{};es.dwCookie=(DWORD_PTR)&s;es.pfnCallback=streamOut;SendMessageW(edit,EM_STREAMOUT,SF_RTF,(LPARAM)&es);if(es.dwError)throw std::runtime_error("RTF write failed");return s;}
bool setRtf(HWND edit,const std::string& s){if(s.size()<6||s.substr(0,5)!="{\\rtf")return false;Input input{&s};EDITSTREAM es{};es.dwCookie=(DWORD_PTR)&input;es.pfnCallback=streamIn;SendMessageW(edit,EM_STREAMIN,SF_RTF,(LPARAM)&es);return es.dwError==0;}
void updatePreview(NoteWindow* nw){auto* n=app.note(nw->id);if(!n)return;wchar_t buf[321]{};TEXTRANGEW range{{0,320},buf};SendMessageW(nw->edit,EM_GETTEXTRANGE,0,(LPARAM)&range);std::wstring s=buf;for(auto& c:s)if(c==L'\r'||c==L'\n'||c==L'\t')c=L' ';n->preview=utf8(trim(s));SetWindowTextW(nw->h,noteTitle(*n).c_str());InvalidateRect(nw->h,nullptr,FALSE);}
bool flushNote(NoteWindow* nw){if(!nw->dirty||nw->loading||nw->loadFailed)return true;bool ok=false;try{ok=atomicWrite(app.path(nw->id),getRtf(nw->edit));}catch(...){ok=false;}nw->error=!ok;if(ok){nw->dirty=false;updatePreview(nw);}else storageError();InvalidateRect(nw->h,nullptr,FALSE);return ok;}
void dirtyNote(NoteWindow* nw){if(nw->loading||nw->loadFailed)return;nw->dirty=true;auto* n=app.note(nw->id);if(n)n->modified=nowTime();SetTimer(nw->h,1,650,nullptr);InvalidateRect(nw->h,nullptr,FALSE);}

std::wstring createBook(HWND owner){std::wstring name;if(!prompt(owner,langString(L"Новый блокнот",L"New notebook"),name))return {};for(auto& b:app.state.books)if(CompareStringOrdinal(wide(b.title).c_str(),-1,name.c_str(),-1,TRUE)==CSTR_EQUAL)return wide(b.id);Book b{makeId(),utf8(name)};app.state.books.push_back(b);scheduleSave();return wide(b.id);}
bool clipboardText(std::wstring& out){if(!OpenClipboard(app.hub))return false;HANDLE h=GetClipboardData(CF_UNICODETEXT);if(h){const wchar_t* p=(const wchar_t*)GlobalLock(h);if(p){size_t limit=GlobalSize(h)/sizeof(wchar_t);size_t n=0;while(n<limit&&p[n])++n;out.assign(p,n);GlobalUnlock(h);}}CloseClipboard();return h!=nullptr;}

void newNote(bool fromClipboard,const std::string& book){
    std::wstring text;
    if(fromClipboard&&!clipboardText(text)){notify(langString(L"В буфере обмена нет текста. Скопируйте текст и повторите действие.",L"There is no text in the clipboard. Copy some text and try again."));return;}
    closeMenu();closePopup();POINT cursor{};GetCursorPos(&cursor);int scale=dpi(app.hub);
    Note n;n.id=makeId();n.book=book;n.w=MulDiv(350,scale,96);n.h=MulDiv(310,scale,96);n.x=cursor.x-n.w-22;n.y=cursor.y-90;
    RECT rect=fitRect({n.x,n.y,n.x+n.w,n.y+n.h});n.x=rect.left;n.y=rect.top;n.w=rect.right-rect.left;n.h=rect.bottom-rect.top;
    n.created=n.modified=n.accessed=nowTime();std::string id=n.id;app.state.notes.push_back(n);showNote(id,false);
    auto it=app.windows.find(id);if(it==app.windows.end()||!it->second->h)return;auto* nw=it->second.get();
    if(fromClipboard)SetWindowTextW(nw->edit,text.c_str());nw->dirty=true;flushNote(nw);saveState();SetFocus(nw->edit);
}
void applyOpacity(NoteWindow* nw){BYTE a=BYTE((GetForegroundWindow()==nw->h?100:app.state.opacity)*255/100);SetLayeredWindowAttributes(nw->h,0,a,LWA_ALPHA);}
void showNote(const std::string& id,bool touch){
    auto* n=app.note(id);if(!n)return;
    auto it=app.windows.find(id);
    if(it==app.windows.end()){
        auto nw=std::make_unique<NoteWindow>();nw->id=id;auto* ptr=nw.get();app.windows.emplace(id,std::move(nw));
        RECT r=fitRect({n->x,n->y,n->x+n->w,n->y+n->h});
        HWND h=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_LAYERED|(n->pinned?WS_EX_TOPMOST:0),L"ListkiNote",noteTitle(*n).c_str(),WS_POPUP|WS_CLIPCHILDREN|WS_THICKFRAME,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,app.instance,ptr);
        if(!h){app.windows.erase(id);messageCard(app.hub,langText(L"Не удалось создать окно записки.",L"Could not create the note window."),langText(L"Листки",L"Listki"),MB_OK|MB_ICONERROR);return;}
        it=app.windows.find(id);
    }
    if(!it->second->h)return;
    n->visible=true;if(touch)n->accessed=nowTime();HWND h=it->second->h;
    ShowWindow(h,SW_SHOWNORMAL);SetWindowPos(h,n->pinned?HWND_TOPMOST:HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);SetForegroundWindow(h);SetFocus(it->second->edit);applyOpacity(it->second.get());scheduleSave();
}
void hideNote(NoteWindow* nw){hideFormatBar(nw);if(!flushNote(nw)){messageCard(nw->h,langText(L"Сохранение не удалось. Окно останется открытым, чтобы вы не потеряли текст.",L"Saving failed. The note will remain open so you do not lose text."),langText(L"Листки",L"Listki"),MB_OK|MB_ICONWARNING);return;}auto* n=app.note(nw->id);if(n)n->visible=false;ShowWindow(nw->h,SW_HIDE);saveState();}
void pinNote(NoteWindow* nw){auto* n=app.note(nw->id);if(!n)return;n->pinned=!n->pinned;SetWindowPos(nw->h,n->pinned?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);InvalidateRect(nw->h,nullptr,FALSE);scheduleSave();}
void deleteNote(NoteWindow* nw){
    hideFormatBar(nw);
    auto* n=app.note(nw->id);if(!n)return;
    if(messageCard(nw->h,langText(L"Удалить эту записку?\n\nКопия текста останется в папке Listki-data\\Deleted.",L"Delete this note?\n\nA copy of its text will remain in Listki-data\\Deleted."),langText(L"Удаление записки",L"Delete note"),MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)return;
    if(!flushNote(nw))return;
    auto folder=app.data+L"\\Deleted";CreateDirectoryW(folder.c_str(),nullptr);auto dest=folder+L"\\"+wide(n->id)+L".rtf";
    if(exists(app.path(n->id))&&!MoveFileExW(app.path(n->id).c_str(),dest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){storageError();return;}
    std::string id=n->id;Note removed=*n;
    app.state.notes.erase(std::remove_if(app.state.notes.begin(),app.state.notes.end(),[&](const Note& t){return t.id==id;}),app.state.notes.end());
    if(!saveState()){app.state.notes.push_back(removed);MoveFileExW(dest.c_str(),app.path(id).c_str(),MOVEFILE_REPLACE_EXISTING);return;}
    DeleteFileW((app.path(id)+L".bak").c_str());DestroyWindow(nw->h);app.windows.erase(id);refreshPopup();
}

void formatSelection(NoteWindow* nw,const std::function<void()>& action){
    ++nw->formatDepth;
    CHARRANGE range{};SendMessageW(nw->edit,EM_EXGETSEL,0,(LPARAM)&range);
    bool all=range.cpMin==range.cpMax;if(all)SendMessageW(nw->edit,EM_SETSEL,0,-1);
    action();if(all)SendMessageW(nw->edit,EM_EXSETSEL,0,(LPARAM)&range);--nw->formatDepth;dirtyNote(nw);SetFocus(nw->edit);requestFormatBar(nw);
}
CHARFORMAT2W currentFormat(NoteWindow* nw){CHARFORMAT2W cf{};cf.cbSize=sizeof(cf);SendMessageW(nw->edit,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);return cf;}
void toggleFormat(NoteWindow* nw,DWORD mask,DWORD effect){auto old=currentFormat(nw);formatSelection(nw,[&](){CHARFORMAT2W cf{};cf.cbSize=sizeof(cf);cf.dwMask=mask;cf.dwEffects=((old.dwMask&mask)&&(old.dwEffects&effect))?0:effect;SendMessageW(nw->edit,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);});}
void chooseFont(NoteWindow* nw){auto current=currentFormat(nw);LOGFONTW lf{};wcsncpy(lf.lfFaceName,current.szFaceName,LF_FACESIZE-1);lf.lfHeight=-MulDiv(current.yHeight,dpi(nw->h),1440);lf.lfWeight=(current.dwEffects&CFE_BOLD)?FW_BOLD:FW_NORMAL;lf.lfItalic=(current.dwEffects&CFE_ITALIC)!=0;lf.lfUnderline=(current.dwEffects&CFE_UNDERLINE)!=0;
    modern::FontDialog dialogStyle;CHOOSEFONTW choice{};choice.lStructSize=sizeof(choice);choice.hwndOwner=nw->h;choice.lpLogFont=&lf;choice.Flags=CF_INITTOLOGFONTSTRUCT|CF_SCREENFONTS|CF_EFFECTS|CF_FORCEFONTEXIST|CF_ENABLEHOOK;choice.lpfnHook=modern::fontHook;choice.lCustData=(LPARAM)&dialogStyle;choice.rgbColors=current.crTextColor;
    if(ChooseFontW(&choice))formatSelection(nw,[&](){CHARFORMAT2W cf{};cf.cbSize=sizeof(cf);cf.dwMask=CFM_FACE|CFM_SIZE|CFM_BOLD|CFM_ITALIC|CFM_UNDERLINE|CFM_STRIKEOUT|CFM_COLOR;wcsncpy(cf.szFaceName,lf.lfFaceName,LF_FACESIZE-1);cf.yHeight=choice.iPointSize*2;cf.crTextColor=choice.rgbColors;cf.dwEffects=(lf.lfWeight>=FW_BOLD?CFE_BOLD:0)|(lf.lfItalic?CFE_ITALIC:0)|(lf.lfUnderline?CFE_UNDERLINE:0)|(lf.lfStrikeOut?CFE_STRIKEOUT:0);SendMessageW(nw->edit,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);});
}
void noteCommand(NoteWindow* nw,int cmd){auto* n=app.note(nw->id);if(!n)return;
    if(nw->loadFailed && ((cmd>=CMD_UNDO&&cmd<=CMD_UNDERLINE&&cmd!=CMD_COPY&&cmd!=CMD_SELECT)||(cmd>=CMD_SIZE&&cmd<CMD_SIZE+int(std::size(fontSizes))))){notify(langString(L"Редактирование недоступно: файл записки не прочитан. Исходный файл не будет перезаписан.",L"Editing is unavailable because the note file could not be read. The original file will not be overwritten."),NIIF_WARNING);return;}
    switch(cmd){
    case CMD_UNDO:SendMessageW(nw->edit,EM_UNDO,0,0);break;
    case CMD_REDO:SendMessageW(nw->edit,EM_REDO,0,0);break;
    case CMD_CUT:SendMessageW(nw->edit,WM_CUT,0,0);break;
    case CMD_COPY:SendMessageW(nw->edit,WM_COPY,0,0);break;
    case CMD_PASTE:SendMessageW(nw->edit,EM_PASTESPECIAL,CF_UNICODETEXT,0);break;
    case CMD_SELECT:SendMessageW(nw->edit,EM_SETSEL,0,-1);break;
    case CMD_FONT:chooseFont(nw);break;
    case CMD_BOLD:toggleFormat(nw,CFM_BOLD,CFE_BOLD);break;
    case CMD_ITALIC:toggleFormat(nw,CFM_ITALIC,CFE_ITALIC);break;
    case CMD_UNDERLINE:toggleFormat(nw,CFM_UNDERLINE,CFE_UNDERLINE);break;
    case CMD_PIN:pinNote(nw);break;
    case CMD_RENAME:beginTitleEdit(nw);break;
    case CMD_DELETE:deleteNote(nw);return;
    case CMD_NEWBOOK:{auto id=createBook(nw->h);n=app.note(nw->id);if(!id.empty()&&n){n->book=utf8(id);scheduleSave();InvalidateRect(nw->h,nullptr,FALSE);}break;}
    case CMD_NOGROUP:n->book.clear();scheduleSave();InvalidateRect(nw->h,nullptr,FALSE);break;
    case CMD_CLOSE:hideNote(nw);break;
    default:
        if(cmd>=CMD_SIZE&&cmd<CMD_SIZE+int(std::size(fontSizes))){int pt=fontSizes[cmd-CMD_SIZE];formatSelection(nw,[&](){CHARFORMAT2W cf{};cf.cbSize=sizeof(cf);cf.dwMask=CFM_SIZE;cf.yHeight=pt*20;SendMessageW(nw->edit,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&cf);});}
        if(cmd>=CMD_COLOR&&cmd<CMD_COLOR+5){n->color=cmd-CMD_COLOR;SendMessageW(nw->edit,EM_SETBKGNDCOLOR,0,noteColors[n->color]);InvalidateRect(nw->titleEdit,nullptr,TRUE);InvalidateRect(nw->h,nullptr,TRUE);scheduleSave();}
        if(cmd>=CMD_BOOK&&cmd<CMD_BOOK+int(app.state.books.size())){n->book=app.state.books[cmd-CMD_BOOK].id;InvalidateRect(nw->h,nullptr,FALSE);scheduleSave();}
    }
}
void noteMenu(NoteWindow* nw,POINT pt){auto* n=app.note(nw->id);if(!n)return;HMENU m=CreatePopupMenu();auto add=[&](int cmd,const wchar_t* name,UINT flags=0){AppendMenuW(m,MF_STRING|flags,cmd,name);};
    hideFormatBar(nw);
    add(CMD_UNDO,langText(L"Отменить\tCtrl+Z",L"Undo\tCtrl+Z"),SendMessageW(nw->edit,EM_CANUNDO,0,0)?0:MF_GRAYED);add(CMD_REDO,langText(L"Повторить\tCtrl+Y",L"Redo\tCtrl+Y"),SendMessageW(nw->edit,EM_CANREDO,0,0)?0:MF_GRAYED);AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    add(CMD_CUT,langText(L"Вырезать\tCtrl+X",L"Cut\tCtrl+X"));add(CMD_COPY,langText(L"Копировать\tCtrl+C",L"Copy\tCtrl+C"));add(CMD_PASTE,langText(L"Вставить текст\tCtrl+V",L"Paste text\tCtrl+V"),IsClipboardFormatAvailable(CF_UNICODETEXT)?0:MF_GRAYED);add(CMD_SELECT,langText(L"Выделить всё\tCtrl+A",L"Select all\tCtrl+A"));AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    add(CMD_FONT,langText(L"Шрифт и цвет текста…",L"Text font and color…"));HMENU sizes=CreatePopupMenu();auto cf=currentFormat(nw);for(size_t i=0;i<std::size(fontSizes);++i)AppendMenuW(sizes,MF_STRING|(cf.yHeight==fontSizes[i]*20?MF_CHECKED:0),CMD_SIZE+i,(std::to_wstring(fontSizes[i])+langString(L" пт",L" pt")).c_str());AppendMenuW(m,MF_POPUP,(UINT_PTR)sizes,langText(L"Размер текста",L"Text size"));
    add(CMD_BOLD,langText(L"Полужирный\tCtrl+B",L"Bold\tCtrl+B"),cf.dwEffects&CFE_BOLD?MF_CHECKED:0);add(CMD_ITALIC,langText(L"Курсив\tCtrl+I",L"Italic\tCtrl+I"),cf.dwEffects&CFE_ITALIC?MF_CHECKED:0);add(CMD_UNDERLINE,langText(L"Подчёркнутый\tCtrl+U",L"Underline\tCtrl+U"),cf.dwEffects&CFE_UNDERLINE?MF_CHECKED:0);
    HMENU colors=CreatePopupMenu();for(int i=0;i<5;++i)AppendMenuW(colors,MF_STRING|(n->color==i?MF_CHECKED:0),CMD_COLOR+i,colorName(i));AppendMenuW(m,MF_POPUP,(UINT_PTR)colors,langText(L"Цвет записки",L"Note color"));AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    add(CMD_PIN,langText(L"Поверх всех окон",L"Always on top"),n->pinned?MF_CHECKED:0);
    HMENU books=CreatePopupMenu();AppendMenuW(books,MF_STRING|(n->book.empty()?MF_CHECKED:0),CMD_NOGROUP,langText(L"Без блокнота",L"No notebook"));for(size_t i=0;i<app.state.books.size();++i)AppendMenuW(books,MF_STRING|(n->book==app.state.books[i].id?MF_CHECKED:0),CMD_BOOK+i,wide(app.state.books[i].title).c_str());AppendMenuW(books,MF_SEPARATOR,0,nullptr);AppendMenuW(books,MF_STRING,CMD_NEWBOOK,langText(L"Создать блокнот…",L"Create notebook…"));AppendMenuW(m,MF_POPUP,(UINT_PTR)books,langText(L"В блокнот",L"Notebook"));
    add(CMD_RENAME,langText(L"Переименовать…\tF2",L"Rename…\tF2"));add(CMD_CLOSE,langText(L"Скрыть записку\tEsc",L"Hide note\tEsc"));AppendMenuW(m,MF_SEPARATOR,0,nullptr);add(CMD_DELETE,langText(L"Удалить записку…",L"Delete note…"));
    SetForegroundWindow(nw->h);int cmd=styledPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,nw->h,nullptr);DestroyMenu(m);if(cmd)noteCommand(nw,cmd);
}

RECT winRect(const ui::Rect& r){return {r.left,r.top,r.right,r.bottom};}
void setToolTip(NoteWindow* nw,HWND target,UINT_PTR id,RECT r,const wchar_t* text){
    if(!nw->tips)return;
    TOOLINFOW info{};info.cbSize=sizeof(info);info.hwnd=target;info.uId=id;info.rect=r;
    SendMessageW(nw->tips,TTM_DELTOOLW,0,(LPARAM)&info);
    info.uFlags=TTF_SUBCLASS;info.lpszText=const_cast<wchar_t*>(text);
    SendMessageW(nw->tips,TTM_ADDTOOLW,0,(LPARAM)&info);
}
void layoutNoteTools(NoteWindow* nw){
    if(!nw->tips||!nw->h)return;RECT client{};GetClientRect(nw->h,&client);
    const wchar_t* names[]={langText(L"Закрепить поверх окон",L"Always on top"),langText(L"Удалить записку",L"Delete note"),langText(L"Цвет записки",L"Note color"),langText(L"Меню записки",L"Note menu"),langText(L"Скрыть записку",L"Hide note")};
    auto buttons=ui::headerButtons(client.right,dpi(nw->h));
    for(int i=0;i<5;++i)setToolTip(nw,nw->h,1+i,winRect(buttons[i]),names[i]);
    if(nw->formatBar){
        const wchar_t* formatNames[]={langText(L"Полужирный (Ctrl+B)",L"Bold (Ctrl+B)"),langText(L"Курсив (Ctrl+I)",L"Italic (Ctrl+I)"),langText(L"Подчёркивание (Ctrl+U)",L"Underline (Ctrl+U)"),langText(L"Размер шрифта",L"Font size")};
        auto format=ui::formatButtons(dpi(nw->formatBar));
        for(int i=0;i<4;++i)setToolTip(nw,nw->formatBar,20+i,winRect(format[i]),formatNames[i]);
    }
}
void hideFormatBar(NoteWindow* nw){
    if(nw->h)KillTimer(nw->h,2);
    if(nw->formatBar){if(GetCapture()==nw->formatBar)ReleaseCapture();ShowWindow(nw->formatBar,SW_HIDE);nw->formatHot=0;nw->formatPressed=0;}
}
void requestFormatBar(NoteWindow* nw){
    if(nw->loading||nw->loadFailed||nw->formatDepth||nw->formatMenu||nw->moving||!nw->edit)return;
    CHARRANGE range{};SendMessageW(nw->edit,EM_EXGETSEL,0,(LPARAM)&range);
    if(range.cpMin==range.cpMax){hideFormatBar(nw);return;}
    SetTimer(nw->h,2,60,nullptr);
}
void updateFormatBar(NoteWindow* nw){
    if(nw->formatDepth||nw->formatMenu)return;
    if(nw->loading||nw->loadFailed||nw->moving||!IsWindowVisible(nw->h)||IsIconic(nw->h)||GetForegroundWindow()!=nw->h||GetFocus()!=nw->edit){hideFormatBar(nw);return;}
    if(nw->selecting||GetCapture()==nw->edit){SetTimer(nw->h,2,60,nullptr);return;}
    CHARRANGE range{};SendMessageW(nw->edit,EM_EXGETSEL,0,(LPARAM)&range);
    if(range.cpMin==range.cpMax){hideFormatBar(nw);return;}
    RECT editRect{};GetClientRect(nw->edit,&editRect);
    POINTL position{-1,-1};SendMessageW(nw->edit,EM_POSFROMCHAR,(WPARAM)&position,range.cpMin);
    // Use the caret if the beginning of a multi-line selection has scrolled away.
    if(position.y<0||position.y>=editRect.bottom){POINT caret{};if(!GetCaretPos(&caret)||caret.y<0||caret.y>=editRect.bottom){hideFormatBar(nw);return;}position={caret.x,caret.y};}
    POINT anchor{std::clamp<LONG>(position.x,0,editRect.right),position.y};ClientToScreen(nw->edit,&anchor);
    RECT work=workArea(anchor);auto cf=currentFormat(nw);int lineHeight=std::max(px(nw->h,18),MulDiv(cf.yHeight,dpi(nw->h),1440)+px(nw->h,4));
    auto place=ui::toolbarPlacement(anchor.x,anchor.y,lineHeight,{int(work.left),int(work.top),int(work.right),int(work.bottom)},dpi(nw->h));
    if(!nw->formatBar){
        nw->formatBar=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED,L"ListkiFormatBar",langText(L"Форматирование выделенного текста",L"Selection formatting"),WS_POPUP,place.left,place.top,place.right-place.left,place.bottom-place.top,nw->h,nullptr,app.instance,nw);
        if(!nw->formatBar)return;layoutNoteTools(nw);
    }
    nw->formatRange=range;
    SetWindowPos(nw->formatBar,HWND_TOP,place.left,place.top,place.right-place.left,place.bottom-place.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(nw->formatBar,nullptr,FALSE);
}
void formatBarAction(NoteWindow* nw,int hit){
    if(hit<1||hit>4||nw->loadFailed||nw->formatRange.cpMin==nw->formatRange.cpMax)return;
    CHARRANGE selected{};SendMessageW(nw->edit,EM_EXGETSEL,0,(LPARAM)&selected);
    if(selected.cpMin!=nw->formatRange.cpMin||selected.cpMax!=nw->formatRange.cpMax){hideFormatBar(nw);return;}
    if(hit<=3){const int cmds[]={CMD_BOLD,CMD_ITALIC,CMD_UNDERLINE};noteCommand(nw,cmds[hit-1]);InvalidateRect(nw->formatBar,nullptr,FALSE);return;}
    auto cf=currentFormat(nw);HMENU m=CreatePopupMenu();
    for(size_t i=0;i<std::size(fontSizes);++i){bool checked=(cf.dwMask&CFM_SIZE)&&cf.yHeight==fontSizes[i]*20;AppendMenuW(m,MF_STRING|(checked?MF_CHECKED:0),CMD_SIZE+i,(std::to_wstring(fontSizes[i])+langString(L" пт",L" pt")).c_str());}
    auto button=ui::formatButtons(dpi(nw->formatBar))[3];POINT at{button.left,button.bottom+px(nw->formatBar,5)};ClientToScreen(nw->formatBar,&at);
    nw->formatMenu=true;
    int cmd=styledPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,at.x,at.y,0,nw->h,nullptr);
    DestroyMenu(m);nw->formatMenu=false;
    if(cmd)noteCommand(nw,cmd);requestFormatBar(nw);
}
void paintFormatBar(NoteWindow* nw){
    HWND h=nw->formatBar;Canvas c(h);fill(c.dc,c.rect,RGB(253,254,252));
    roundOutline(c.dc,c.rect,px(h,10),RGB(219,227,221));
    auto buttons=ui::formatButtons(dpi(h));auto cf=currentFormat(nw);
    const DWORD masks[]={CFM_BOLD,CFM_ITALIC,CFM_UNDERLINE};const DWORD effects[]={CFE_BOLD,CFE_ITALIC,CFE_UNDERLINE};
    for(int i=0;i<4;++i){
        RECT r=winRect(buttons[i]);bool on=i<3&&(cf.dwMask&masks[i])&&(cf.dwEffects&effects[i]);
        if(on||nw->formatHot==i+1)roundFill(c.dc,r,px(h,6),on?RGB(222,238,228):RGB(238,243,239));
        if(i<3){
            HFONT f=CreateFontW(-px(h,15),0,0,0,i==0?FW_BOLD:FW_NORMAL,i==1,i==2,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            auto old=SelectObject(c.dc,f);SetBkMode(c.dc,TRANSPARENT);SetTextColor(c.dc,on?TEAL:INK);const wchar_t* letters[]={L"B",L"I",L"U"};DrawTextW(c.dc,letters[i],1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(c.dc,old);DeleteObject(f);
        }else{
            std::wstring size=L"—";
            if(cf.dwMask&CFM_SIZE){wchar_t value[24]{};swprintf(value,24,L"%.2f",cf.yHeight/20.0);size=value;while(!size.empty()&&size.back()==L'0')size.pop_back();if(!size.empty()&&size.back()==L'.')size.pop_back();if(!isEnglish())std::replace(size.begin(),size.end(),L'.',L',');}
            label(c.dc,h,size,{r.left+px(h,9),r.top,r.right-px(h,24),r.bottom},13,INK,FW_SEMIBOLD,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
            glyph(c.dc,{r.right-px(h,23),r.top+px(h,8),r.right-px(h,7),r.bottom-px(h,8)},9,MUTED,px(h,1));
        }
    }
    line(c.dc,px(h,114),px(h,12),px(h,114),px(h,30),RGB(222,229,223));
}
LRESULT CALLBACK formatBarProc(HWND h,UINT m,WPARAM w,LPARAM l){
    auto* nw=(NoteWindow*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(m==WM_NCCREATE){nw=(NoteWindow*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)nw);nw->formatBar=h;}
    if(!nw)return DefWindowProcW(h,m,w,l);
    switch(m){
    case WM_CREATE:SetLayeredWindowAttributes(h,0,253,LWA_ALPHA);roundedRegion(h,20);return 0;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_SIZE:roundedRegion(h,20);return 0;
    case WM_DPICHANGED:layoutNoteTools(nw);requestFormatBar(nw);return 0;
    case WM_MOUSEMOVE:{int hit=ui::formatHit(dpi(h),GET_X_LPARAM(l),GET_Y_LPARAM(l));if(hit!=nw->formatHot){nw->formatHot=hit;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_MOUSELEAVE:nw->formatHot=0;InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:nw->formatPressed=ui::formatHit(dpi(h),GET_X_LPARAM(l),GET_Y_LPARAM(l));SetCapture(h);return 0;
    case WM_LBUTTONUP:{int hit=ui::formatHit(dpi(h),GET_X_LPARAM(l),GET_Y_LPARAM(l));bool apply=hit&&hit==nw->formatPressed&&IsWindowVisible(h);nw->formatPressed=0;ReleaseCapture();if(apply)formatBarAction(nw,hit);return 0;}
    case WM_CAPTURECHANGED:nw->formatPressed=0;return 0;
    case WM_CLOSE:hideFormatBar(nw);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paintFormatBar(nw);return 0;
    case WM_NCDESTROY:nw->formatBar=nullptr;break;
    }
    return DefWindowProcW(h,m,w,l);
}
void colorDropdown(NoteWindow* nw){
    hideFormatBar(nw);auto* n=app.note(nw->id);if(!n)return;HMENU menu=CreatePopupMenu();
    for(int i=0;i<5;++i)AppendMenuW(menu,MF_OWNERDRAW|(n->color==i?MF_CHECKED:0),CMD_COLOR+i,(LPCWSTR)(UINT_PTR)i);
    RECT client{};GetClientRect(nw->h,&client);auto button=ui::headerButtons(client.right,dpi(nw->h))[ui::Color-1];POINT at{button.right,button.bottom+px(nw->h,5)};ClientToScreen(nw->h,&at);
    int cmd=styledPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON|TPM_RIGHTALIGN,at.x,at.y,0,nw->h,nullptr);
    DestroyMenu(menu);if(cmd)noteCommand(nw,cmd);
}

// Title edits only change metadata; the RTF body and its selection are independent.
void updateTitle(NoteWindow* nw){
    if(nw->loading||nw->titleSync)return;auto* n=app.note(nw->id);if(!n)return;
    std::string value=utf8(windowText(nw->titleEdit));if(value==n->title)return;
    n->title=value;n->modified=nowTime();nw->titleDirty=true;
    SetWindowTextW(nw->h,noteTitle(*n).c_str());scheduleSave();InvalidateRect(nw->h,nullptr,FALSE);
}
void setTitleText(NoteWindow* nw,const std::wstring& value){
    nw->titleSync=true;SetWindowTextW(nw->titleEdit,value.c_str());nw->titleSync=false;updateTitle(nw);
}
void commitTitle(NoteWindow* nw){
    if(!nw->titleEdit||nw->loading)return;
    auto value=trim(windowText(nw->titleEdit));if(value!=windowText(nw->titleEdit))setTitleText(nw,value);
    InvalidateRect(nw->titleEdit,nullptr,TRUE);InvalidateRect(nw->h,nullptr,FALSE);
}
void beginTitleEdit(NoteWindow* nw){hideFormatBar(nw);SetFocus(nw->titleEdit);SendMessageW(nw->titleEdit,EM_SETSEL,0,-1);}
LRESULT CALLBACK titleSubclass(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR ref){
    auto* nw=(NoteWindow*)ref;
    if(m==WM_KEYDOWN){
        if(w==VK_RETURN||w==VK_TAB){commitTitle(nw);SetFocus(nw->edit);return 0;}
        if(w==VK_ESCAPE){setTitleText(nw,nw->originalTitle);SetFocus(nw->edit);return 0;}
        if((GetKeyState(VK_CONTROL)&0x8000)&&w=='A'){SendMessageW(h,EM_SETSEL,0,-1);return 0;}
        if((GetKeyState(VK_CONTROL)&0x8000)&&w=='S'){commitTitle(nw);saveState();return 0;}
    }
    if(m==WM_CHAR&&(w==13||w==9||w==27||w==1||w==19))return 0;
    return DefSubclassProc(h,m,w,l);
}
LRESULT CALLBACK editSubclass(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR ref){auto* nw=(NoteWindow*)ref;
    if(m==WM_LBUTTONDOWN||m==WM_LBUTTONDBLCLK){hideFormatBar(nw);nw->selecting=true;}
    if(m==WM_LBUTTONUP||m==WM_CAPTURECHANGED){auto result=DefSubclassProc(h,m,w,l);nw->selecting=false;requestFormatBar(nw);return result;}
    if(m==WM_MOUSEWHEEL||m==WM_VSCROLL||m==WM_HSCROLL){auto result=DefSubclassProc(h,m,w,l);requestFormatBar(nw);return result;}
    if(m==WM_CONTEXTMENU){POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};if(pt.x==-1&&pt.y==-1){GetCaretPos(&pt);ClientToScreen(h,&pt);}noteMenu(nw,pt);return 0;}
    if(m==WM_KEYDOWN){bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        if(ctrl){int cmd=w=='B'?CMD_BOLD:w=='I'?CMD_ITALIC:w=='U'?CMD_UNDERLINE:w=='A'?CMD_SELECT:w=='V'?CMD_PASTE:0;if(cmd){++nw->suppressChar[w-'A'+1];noteCommand(nw,cmd);return 0;}if(w=='S'){++nw->suppressChar[19];flushNote(nw);saveState();return 0;}}
        if(w==VK_F2){noteCommand(nw,CMD_RENAME);return 0;}if(w==VK_ESCAPE){hideNote(nw);return 0;}
    }
    // TranslateMessage has already queued WM_CHAR. Do not execute shortcuts twice.
    if(m==WM_CHAR){if(w==27)return 0;if(w<32&&nw->suppressChar[w]){--nw->suppressChar[w];return 0;}}
    if(m==WM_PASTE){SendMessageW(h,EM_PASTESPECIAL,CF_UNICODETEXT,0);return 0;}
    return DefSubclassProc(h,m,w,l);
}
int noteButton(HWND h,POINT p){RECT r{};GetClientRect(h,&r);return ui::headerHit(r.right,dpi(h),p.x,p.y);}
void layoutNote(NoteWindow* nw){if(!nw->edit)return;RECT r{};GetClientRect(nw->h,&r);int m=px(nw->h,18),top=px(nw->h,57),bottom=px(nw->h,31);MoveWindow(nw->edit,m,top,std::max(1,int(r.right)-2*m),std::max(1,int(r.bottom)-top-bottom),TRUE);auto title=ui::titleRect(r.right,dpi(nw->h));if(nw->titleEdit)MoveWindow(nw->titleEdit,title.left,title.top,title.right-title.left,title.bottom-title.top,TRUE);roundedRegion(nw->h,28);layoutNoteTools(nw);}
LRESULT CALLBACK noteProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    auto* nw=(NoteWindow*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(msg==WM_NCCREATE){nw=(NoteWindow*)((CREATESTRUCTW*)l)->lpCreateParams;nw->h=h;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)nw);}
    if(!nw)return DefWindowProcW(h,msg,w,l);auto* n=app.note(nw->id);
    switch(msg){
    case WM_NCCALCSIZE:return 0;
    case WM_CREATE:{
        nw->titleEdit=CreateWindowExW(0,L"EDIT",n?wide(n->title).c_str():L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)2,app.instance,nullptr);
        if(!nw->titleEdit)return -1;nw->titleFont=font(h,13,FW_SEMIBOLD);
        SendMessageW(nw->titleEdit,WM_SETFONT,(WPARAM)nw->titleFont,TRUE);SendMessageW(nw->titleEdit,EM_SETLIMITTEXT,160,0);
        SendMessageW(nw->titleEdit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);SendMessageW(nw->titleEdit,EM_SETCUEBANNER,TRUE,(LPARAM)langText(L"Новая записка",L"New note"));
        SetWindowSubclass(nw->titleEdit,titleSubclass,1,(DWORD_PTR)nw);
        nw->edit=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|ES_NOHIDESEL,0,0,10,10,h,(HMENU)1,app.instance,nullptr);
        if(!nw->edit)return -1;
        SetWindowSubclass(nw->edit,editSubclass,1,(DWORD_PTR)nw);SendMessageW(nw->edit,EM_EXLIMITTEXT,0,4*1024*1024);
        SendMessageW(nw->edit,EM_SETBKGNDCOLOR,0,noteColors[n?n->color:0]);SendMessageW(nw->edit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(0,0));
        SendMessageW(nw->edit,EM_SETTEXTMODE,TM_RICHTEXT|TM_MULTILEVELUNDO|TM_MULTICODEPAGE,0);
        CHARFORMAT2W cf{};cf.cbSize=sizeof(cf);cf.dwMask=CFM_FACE|CFM_SIZE|CFM_COLOR;cf.yHeight=240;cf.crTextColor=INK;wcscpy(cf.szFaceName,L"Segoe UI");SendMessageW(nw->edit,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&cf);
        PARAFORMAT2 pf{};pf.cbSize=sizeof(pf);pf.dwMask=PFM_SPACEAFTER;pf.dySpaceAfter=70;SendMessageW(nw->edit,EM_SETPARAFORMAT,0,(LPARAM)&pf);
        auto path=app.path(nw->id);bool loaded=false;
        if(exists(path)||exists(path+L".bak")||(n&&(!n->title.empty()||!n->preview.empty()))){
            try{loaded=setRtf(nw->edit,readBytes(path));}catch(...){}
            if(!loaded){try{auto recovered=readBytes(path+L".bak");loaded=setRtf(nw->edit,recovered);if(loaded){if(exists(path))CopyFileW(path.c_str(),(path+L".damaged").c_str(),FALSE);atomicWrite(path,recovered,false);notify(langString(L"Одна из записок восстановлена из резервной копии.",L"One note was restored from its backup."));}}catch(...){}}
            if(!loaded){SetWindowTextW(nw->edit,langText(L"Не удалось прочитать файл записки. Проверьте RTF и его резервную копию в папке данных.",L"Could not read the note file. Check the RTF and its backup in the data folder."));SendMessageW(nw->edit,EM_SETREADONLY,TRUE,0);nw->error=true;nw->loadFailed=true;notify(langString(L"Не удалось прочитать одну из записок. Исходный файл сохранён без изменений.",L"Could not read one of the notes. The original file was left unchanged."),NIIF_ERROR);}
        }
        SendMessageW(nw->edit,EM_EMPTYUNDOBUFFER,0,0);SendMessageW(nw->edit,EM_SETEVENTMASK,0,ENM_CHANGE|ENM_SELCHANGE|ENM_SCROLL);nw->loading=false;if(loaded)updatePreview(nw);
        nw->tips=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,h,nullptr,app.instance,nullptr);
        if(nw->tips){SendMessageW(nw->tips,TTM_SETDELAYTIME,TTDT_INITIAL,450);SendMessageW(nw->tips,TTM_SETMAXTIPWIDTH,0,px(h,300));}
        layoutNote(nw);applyOpacity(nw);return 0;
    }
    case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);RECT r{};GetClientRect(h,&r);int b=px(h,6);bool left=p.x<b,right=p.x>=r.right-b,top=p.y<b,bottom=p.y>=r.bottom-b;
        if(top&&left)return HTTOPLEFT;if(top&&right)return HTTOPRIGHT;if(bottom&&left)return HTBOTTOMLEFT;if(bottom&&right)return HTBOTTOMRIGHT;if(left)return HTLEFT;if(right)return HTRIGHT;if(top)return HTTOP;if(bottom)return HTBOTTOM;
        if(ui::contains(ui::titleRect(r.right,dpi(h)),p.x,p.y))return HTCLIENT;
        if(p.y<px(h,48)&&!noteButton(h,p))return HTCAPTION;return HTCLIENT;
    }
    case WM_NCLBUTTONDBLCLK:return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize={px(h,260),px(h,180)};return 0;
    case WM_ENTERSIZEMOVE:nw->moving=true;hideFormatBar(nw);return 0;
    case WM_EXITSIZEMOVE:nw->moving=false;requestFormatBar(nw);return 0;
    case WM_SIZE:layoutNote(nw);[[fallthrough]];
    case WM_MOVE:hideFormatBar(nw);if(n&&!nw->loading&&w!=SIZE_MINIMIZED){RECT r{};GetWindowRect(h,&r);n->x=r.left;n->y=r.top;n->w=r.right-r.left;n->h=r.bottom-r.top;scheduleSave();}return 0;
    case WM_DPICHANGED:{RECT* r=(RECT*)l;SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);HFONT old=nw->titleFont;nw->titleFont=font(h,13,FW_SEMIBOLD);SendMessageW(nw->titleEdit,WM_SETFONT,(WPARAM)nw->titleFont,TRUE);if(old)DeleteObject(old);layoutNote(nw);return 0;}
    case WM_COMMAND:
        if((HWND)l==nw->titleEdit){
            if(HIWORD(w)==EN_CHANGE)updateTitle(nw);
            if(HIWORD(w)==EN_SETFOCUS){nw->originalTitle=windowText(nw->titleEdit);hideFormatBar(nw);InvalidateRect(h,nullptr,FALSE);}
            if(HIWORD(w)==EN_KILLFOCUS)commitTitle(nw);
            return 0;
        }
        if((HWND)l==nw->edit){if(HIWORD(w)==EN_CHANGE){dirtyNote(nw);return 0;}if(HIWORD(w)==EN_VSCROLL||HIWORD(w)==EN_HSCROLL){requestFormatBar(nw);return 0;}}break;
    case WM_CTLCOLOREDIT:if((HWND)l==nw->titleEdit){COLORREF bg=mixColor(noteColors[n?n->color:0],RGB(255,255,255),55);SetTextColor((HDC)w,INK);SetBkColor((HDC)w,bg);SetDCBrushColor((HDC)w,bg);return (LRESULT)GetStockObject(DC_BRUSH);}break;
    case WM_NOTIFY:{auto* header=(NMHDR*)l;if(header&&header->hwndFrom==nw->edit&&header->code==EN_SELCHANGE){requestFormatBar(nw);return 0;}break;}
    case WM_TIMER:if(w==1){KillTimer(h,1);if(flushNote(nw))saveState();}if(w==2){KillTimer(h,2);updateFormatBar(nw);}return 0;
    case WM_ACTIVATE:applyOpacity(nw);if(LOWORD(w)==WA_INACTIVE){hideFormatBar(nw);if(!nw->loading&&!app.exiting){flushNote(nw);scheduleSave();}}return 0;
    case WM_SETFOCUS:if(nw->edit){SetFocus(nw->edit);requestFormatBar(nw);}return 0;
    case WM_SHOWWINDOW:if(!w)hideFormatBar(nw);break;
    case WM_ENABLE:if(!w)hideFormatBar(nw);break;
    case WM_MOUSEMOVE:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int hot=noteButton(h,p);if(hot!=nw->hot){nw->hot=hot;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_MOUSELEAVE:nw->hot=0;InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:nw->pressed=noteButton(h,{GET_X_LPARAM(l),GET_Y_LPARAM(l)});if(nw->pressed){hideFormatBar(nw);SetCapture(h);}return 0;
    case WM_CAPTURECHANGED:nw->pressed=0;return 0;
    case WM_LBUTTONUP:{int hit=noteButton(h,{GET_X_LPARAM(l),GET_Y_LPARAM(l)});bool apply=hit&&hit==nw->pressed;nw->pressed=0;ReleaseCapture();if(!apply)return 0;
        switch(hit){case ui::Pin:pinNote(nw);break;case ui::Delete:deleteNote(nw);break;case ui::Color:colorDropdown(nw);break;case ui::More:{POINT p{};GetCursorPos(&p);noteMenu(nw,p);break;}case ui::Close:hideNote(nw);break;}return 0;}
    case WM_CONTEXTMENU:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};if(p.x==-1)GetCursorPos(&p);noteMenu(nw,p);return 0;}
    case WM_CLOSE:if(app.exiting)DestroyWindow(h);else hideNote(nw);return 0;
    case WM_MEASUREITEM:{auto* item=(MEASUREITEMSTRUCT*)l;if(item->CtlType==ODT_MENU&&item->itemID>=CMD_COLOR&&item->itemID<CMD_COLOR+5){item->itemWidth=px(h,152);item->itemHeight=px(h,34);return TRUE;}break;}
    case WM_DRAWITEM:{auto* item=(DRAWITEMSTRUCT*)l;if(item->CtlType!=ODT_MENU||item->itemID<CMD_COLOR||item->itemID>=CMD_COLOR+5)break;
        int color=item->itemID-CMD_COLOR;RECT r=item->rcItem;fill(item->hDC,r,(item->itemState&ODS_SELECTED)?RGB(236,244,238):RGB(253,254,252));
        circle(item->hDC,{r.left+px(h,11),r.top+px(h,9),r.left+px(h,27),r.top+px(h,25)},noteColors[color],mixColor(noteColors[color],MUTED,35));
        label(item->hDC,h,colorName(color),{r.left+px(h,38),r.top,r.right-px(h,28),r.bottom},13,INK);
        if(n&&n->color==color)glyph(item->hDC,{r.right-px(h,25),r.top+px(h,8),r.right-px(h,7),r.top+px(h,26)},10,TEAL,px(h,1));return TRUE;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{Canvas c(h);if(!n){fill(c.dc,c.rect,PAPER);return 0;}int ww=c.rect.right,hh=c.rect.bottom;fill(c.dc,c.rect,noteColors[n->color]);
        COLORREF header=mixColor(noteColors[n->color],RGB(255,255,255),55);fill(c.dc,{0,0,ww,px(h,48)},header);
        auto buttons=ui::headerButtons(ww,dpi(h));
        auto title=ui::titleRect(ww,dpi(h));
        if(GetFocus()==nw->titleEdit)roundOutline(c.dc,{title.left-px(h,5),px(h,10),title.right+px(h,4),px(h,38)},px(h,6),mixColor(header,TEAL,35));
        for(int x:{9,12})for(int y:{19,24,29})fill(c.dc,{px(h,x),px(h,y),px(h,x+1),px(h,y+1)},mixColor(header,MUTED,55));
        for(int i=1;i<=5;++i){RECT r=winRect(buttons[i-1]);bool hot=nw->hot==i;bool pinned=i==ui::Pin&&n->pinned;
            if(hot||pinned)roundFill(c.dc,r,px(h,6),i==ui::Delete?RGB(251,231,228):pinned?RGB(221,237,227):mixColor(header,MUTED,10));
            if(i==ui::Color){circle(c.dc,{r.left+px(h,3),r.top+px(h,7),r.left+px(h,17),r.top+px(h,21)},noteColors[n->color],mixColor(noteColors[n->color],MUTED,40));glyph(c.dc,{r.left+px(h,20),r.top+px(h,7),r.right-px(h,2),r.bottom-px(h,7)},9,MUTED,px(h,1));}
            else{int kind=i==ui::Pin?5:i==ui::Delete?8:i==ui::More?6:4;glyph(c.dc,{r.left+px(h,4),r.top+px(h,4),r.right-px(h,4),r.bottom-px(h,4)},kind,hot&&i==ui::Delete?RGB(189,78,70):pinned?TEAL:MUTED,px(h,1));}
        }
        label(c.dc,h,bookTitle(n->book),{px(h,18),hh-px(h,27),ww-px(h,112),hh-px(h,7)},10,MUTED);
        label(c.dc,h,nw->error?langText(L"Ошибка файла",L"File error"):(nw->dirty||nw->titleDirty)?langText(L"Сохраняется…",L"Saving…"):langText(L"Сохранено",L"Saved"),{ww-px(h,114),hh-px(h,27),ww-px(h,20),hh-px(h,7)},10,nw->error?RGB(178,62,48):MUTED,FW_NORMAL,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
        for(int i=1;i<=2;++i)line(c.dc,ww-px(h,7+i*3),hh-px(h,7),ww-px(h,7),hh-px(h,7+i*3),mixColor(noteColors[n->color],MUTED,42));
        roundOutline(c.dc,c.rect,px(h,13),mixColor(noteColors[n->color],MUTED,25));return 0;
    }
    case WM_DESTROY:hideFormatBar(nw);if(nw->formatBar)DestroyWindow(nw->formatBar);if(nw->tips)DestroyWindow(nw->tips);nw->tips=nullptr;if(nw->titleFont){DeleteObject(nw->titleFont);nw->titleFont=nullptr;}break;
    case WM_NCDESTROY:nw->h=nullptr;nw->edit=nullptr;nw->titleEdit=nullptr;break;
    }
    return DefWindowProcW(h,msg,w,l);
}

void closeMenu(){app.menuOpen=false;for(auto& h:app.petals)if(h)ShowWindow(h,SW_HIDE);if(app.hub)InvalidateRect(app.hub,nullptr,FALSE);KillTimer(app.hub,2);}
void showMenu(){
    if(app.menuOpen){closeMenu();return;}closePopup();app.menuOpen=true;RECT hr{};GetWindowRect(app.hub,&hr);RECT work=workArea({hr.left,hr.top});int cx=(hr.left+hr.right)/2,cy=(hr.top+hr.bottom)/2;
    int pw=px(app.hub,124),ph=px(app.hub,51);bool left=cx>(work.left+work.right)/2;int sign=left?-1:1;
    int dx[]={60,124,124,60},dy[]={-106,-37,37,106};
    for(int i=0;i<4;++i){int x=cx+sign*px(app.hub,dx[i])-pw/2,y=cy+px(app.hub,dy[i])-ph/2;x=std::clamp(x,int(work.left+5),int(work.right-pw-5));y=std::clamp(y,int(work.top+5),int(work.bottom-ph-5));
        // Stacking at the upper/lower edge avoids overlapping buttons after clamping.
        if(cy<work.top+px(app.hub,143)||cy>work.bottom-px(app.hub,143)){int base=std::clamp(cy-px(app.hub,111),int(work.top+6),int(work.bottom-px(app.hub,226)));y=base+i*px(app.hub,57);x=left?hr.left-pw-px(app.hub,10):hr.right+px(app.hub,10);x=std::clamp(x,int(work.left+5),int(work.right-pw-5));}
        SetWindowPos(app.petals[i],HWND_TOPMOST,x,y,pw,ph,SWP_NOACTIVATE|SWP_SHOWWINDOW);roundedRegion(app.petals[i],24);
    }
    InvalidateRect(app.hub,nullptr,FALSE);SetTimer(app.hub,2,80,nullptr);
}
void saveHubPosition(){RECT r{};GetWindowRect(app.hub,&r);app.state.hubX=r.left;app.state.hubY=r.top;scheduleSave();}
void resetHub(){RECT work=workArea({GetSystemMetrics(SM_CXSCREEN)/2,GetSystemMetrics(SM_CYSCREEN)/2});int size=px(app.hub,72);SetWindowPos(app.hub,HWND_TOPMOST,work.right-size-px(app.hub,14),(work.top+work.bottom-size)/2,size,size,SWP_NOACTIVATE|SWP_SHOWWINDOW);saveHubPosition();}
void ensureHub(){RECT r{};GetWindowRect(app.hub,&r);r=fitRect(r);SetWindowPos(app.hub,HWND_TOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);saveHubPosition();}
LRESULT CALLBACK hubProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    static POINT start{},origin{};static bool down=false,drag=false,hot=false;static bool wasMouseDown=false;
    switch(msg){
    case WM_CREATE:SetLayeredWindowAttributes(h,0,225,LWA_ALPHA);return 0;
    case WM_SIZE:{RECT r{};GetClientRect(h,&r);SetWindowRgn(h,CreateEllipticRgn(0,0,r.right,r.bottom),TRUE);return 0;}
    case WM_LBUTTONDOWN:{SetCapture(h);GetCursorPos(&start);RECT r{};GetWindowRect(h,&r);origin={r.left,r.top};down=true;drag=false;return 0;}
    case WM_MOUSEMOVE:{if(down){POINT p{};GetCursorPos(&p);if(abs(p.x-start.x)+abs(p.y-start.y)>px(h,5))drag=true;if(drag){closeMenu();int size=px(h,72);RECT r=fitRect({origin.x+p.x-start.x,origin.y+p.y-start.y,origin.x+p.x-start.x+size,origin.y+p.y-start.y+size});SetWindowPos(h,HWND_TOPMOST,r.left,r.top,0,0,SWP_NOSIZE|SWP_NOACTIVATE);}}
        if(!hot){hot=true;SetLayeredWindowAttributes(h,0,250,LWA_ALPHA);InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_LBUTTONUP:if(down){down=false;ReleaseCapture();if(drag)saveHubPosition();else showMenu();}return 0;
    case WM_CAPTURECHANGED:down=false;return 0;
    case WM_MOUSELEAVE:hot=false;SetLayeredWindowAttributes(h,0,225,LWA_ALPHA);InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_CONTEXTMENU:{closeMenu();POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};if(p.x==-1)GetCursorPos(&p);trayMenu(p);return 0;}
    case WM_KEYDOWN:if(w==VK_ESCAPE)closeMenu();else if(w==VK_SPACE||w==VK_RETURN)showMenu();return 0;
    case WM_TIMER:if(w==2){bool pressed=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;if(pressed&&!wasMouseDown){POINT p{};GetCursorPos(&p);HWND hit=WindowFromPoint(p);bool ours=hit==h;for(auto petal:app.petals)ours|=hit==petal;if(!ours)closeMenu();}wasMouseDown=pressed;}return 0;
    case WM_DPICHANGED:{closeMenu();RECT* r=(RECT*)l;SetWindowPos(h,HWND_TOPMOST,r->left,r->top,px(h,72),px(h,72),SWP_NOACTIVATE);saveHubPosition();return 0;}
    case WM_DISPLAYCHANGE:closeMenu();ensureHub();return 0;
    case WM_CLOSE:ShowWindow(h,SW_HIDE);closeMenu();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{Canvas c(h);fill(c.dc,c.rect,hot||app.menuOpen?RGB(239,248,243):PAPER);
        RECT inner{px(h,8),px(h,8),c.rect.right-px(h,8),c.rect.bottom-px(h,8)};circle(c.dc,inner,hot||app.menuOpen?RGB(225,241,231):RGB(236,246,239),RGB(224,237,228));
        glyph(c.dc,{px(h,20),px(h,14),px(h,52),px(h,45)},app.menuOpen?4:1,TEAL,px(h,1));
        label(c.dc,h,L"LISTKI",{0,px(h,45),c.rect.right,px(h,59)},8,TEAL,FW_SEMIBOLD,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        Gdiplus::Graphics g(c.dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen border(gc(modern::edge),1.f);g.DrawEllipse(&border,.5f,.5f,float(c.rect.right)-1.f,float(c.rect.bottom)-1.f);return 0;}

    }return DefWindowProcW(h,msg,w,l);
}
LRESULT CALLBACK petalProc(HWND h,UINT msg,WPARAM w,LPARAM l){int index=int(GetWindowLongPtrW(h,GWLP_USERDATA));bool hot=GetPropW(h,L"hot")!=nullptr;
    switch(msg){case WM_CREATE:SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams);SetLayeredWindowAttributes(h,0,245,LWA_ALPHA);return 0;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_MOUSEMOVE:if(!hot){SetPropW(h,L"hot",(HANDLE)1);InvalidateRect(h,nullptr,FALSE);} {TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);}return 0;
    case WM_MOUSELEAVE:RemovePropW(h,L"hot");InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONUP:{closeMenu();if(index==0)newNote();if(index==1)newNote(true);if(index==2)openPopup(true);if(index==3){POINT p{};GetCursorPos(&p);notebooksMenu(p);}return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{Canvas c(h);fill(c.dc,c.rect,hot?RGB(240,248,242):PAPER);
        const COLORREF chips[]={RGB(226,241,229),RGB(229,239,249),RGB(244,235,220),RGB(238,230,246)};
        RECT chip{px(h,9),px(h,11),px(h,37),px(h,39)};roundFill(c.dc,chip,px(h,9),chips[index]);
        glyph(c.dc,{px(h,13),px(h,15),px(h,33),px(h,35)},index,TEAL,px(h,1));
        const wchar_t* titles[]={langText(L"Новая",L"New"),langText(L"Из буфера",L"From clipboard"),langText(L"Последние",L"Recent"),langText(L"Блокнот",L"Notebook")};label(c.dc,h,titles[index],{px(h,44),0,c.rect.right-px(h,7),c.rect.bottom},12,INK,FW_SEMIBOLD);
        roundOutline(c.dc,c.rect,px(h,11),hot?RGB(183,207,190):modern::edge);return 0;}

    }return DefWindowProcW(h,msg,w,l);
}

void closePopup(){if(app.popup){HWND h=app.popup->h;if(h)DestroyWindow(h);app.popup.reset();}}
void populatePopup(){auto* p=app.popup.get();if(!p||!p->h||!p->list||!p->search)return;p->ids.clear();p->hot=-1;SendMessageW(p->list,WM_SETREDRAW,FALSE,0);SendMessageW(p->list,LB_RESETCONTENT,0,0);auto q=windowText(p->search);for(auto& c:q)c=towlower(c);
    auto matches=[&](std::wstring s){for(auto& c:s)c=towlower(c);return q.empty()||s.find(q)!=std::wstring::npos;};
    if(p->books){for(const auto& b:app.state.books)if(matches(wide(b.title)))p->ids.push_back(b.id);}
    else {
        auto ids=recent(app.state,p->recent?10:app.state.notes.size());for(const auto& id:ids){auto* n=app.note(id);if(!n)continue;bool group=p->book.empty()||(p->book=="none"?n->book.empty():n->book==p->book);if(group&&matches(noteTitle(*n)+L" "+wide(n->preview)))p->ids.push_back(id);}
    }
    for(auto& id:p->ids){std::wstring title=p->books?bookTitle(id):noteTitle(*app.note(id));SendMessageW(p->list,LB_ADDSTRING,0,(LPARAM)title.c_str());}
    if(!p->ids.empty())SendMessageW(p->list,LB_SETCURSEL,0,0);SendMessageW(p->list,WM_SETREDRAW,TRUE,0);InvalidateRect(p->list,nullptr,TRUE);InvalidateRect(p->h,nullptr,FALSE);
}
void refreshPopup(){if(app.popup)populatePopup();}
void selectPopup(int index){auto* p=app.popup.get();if(!p||index<0||index>=int(p->ids.size()))return;std::string id=p->ids[index];bool books=p->books;closePopup();if(books)openPopup(false,id);else showNote(id);}
void bookContext(POINT point,int index){auto* p=app.popup.get();if(!p||!p->books||index<0||index>=int(p->ids.size()))return;auto id=p->ids[index];HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,1,langText(L"Переименовать…",L"Rename…"));AppendMenuW(m,MF_STRING,2,langText(L"Удалить блокнот…",L"Delete notebook…"));++app.modal;int cmd=styledPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,p->h,nullptr);DestroyMenu(m);
    if(cmd==1){auto name=bookTitle(id);if(prompt(p->h,langString(L"Название блокнота",L"Notebook name"),name)){for(auto& b:app.state.books)if(b.id==id)b.title=utf8(name);saveState();}}
    if(cmd==2&&messageCard(p->h,langText(L"Удалить блокнот?\n\nЕго записки сохранятся в разделе «Без блокнота».",L"Delete this notebook?\n\nIts notes will remain under “No notebook”."),langText(L"Удаление блокнота",L"Delete notebook"),MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES){removeBook(app.state,id);saveState();}
    --app.modal;populatePopup();for(auto& nw:app.windows)if(nw.second->h)InvalidateRect(nw.second->h,nullptr,FALSE);
}
LRESULT CALLBACK listSubclass(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(m==WM_MOUSEMOVE&&app.popup){auto hit=SendMessageW(h,LB_ITEMFROMPOINT,0,l);int hot=HIWORD(hit)?-1:int(LOWORD(hit));if(hot!=app.popup->hot){app.popup->hot=hot;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);}
    if(m==WM_MOUSELEAVE&&app.popup){app.popup->hot=-1;InvalidateRect(h,nullptr,FALSE);}
    if(m==WM_SETFOCUS||m==WM_KILLFOCUS)InvalidateRect(h,nullptr,FALSE);
    if(m==WM_PAINT&&SendMessageW(h,LB_GETCOUNT,0,0)==0){Canvas c(h);fill(c.dc,c.rect,PAPER);bool books=app.popup&&app.popup->books;bool query=app.popup&&!windowText(app.popup->search).empty();int center=c.rect.right/2;
        roundFill(c.dc,{center-px(h,26),px(h,47),center+px(h,26),px(h,99)},px(h,16),modern::soft);glyph(c.dc,{center-px(h,16),px(h,57),center+px(h,16),px(h,89)},books?3:1,TEAL,px(h,1));
        label(c.dc,h,query?langText(L"Ничего не найдено",L"Nothing found"):books?langText(L"Пока нет блокнотов",L"No notebooks yet"):langText(L"Пока нет записок",L"No notes yet"),{px(h,10),px(h,113),c.rect.right-px(h,10),px(h,141)},15,INK,FW_SEMIBOLD,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        label(c.dc,h,query?langText(L"Попробуйте другое слово",L"Try another word"):books?langText(L"Создайте первый блокнот кнопкой внизу",L"Create your first notebook with the button below"):app.popup&&app.popup->recent?langText(L"Новая записка появится здесь после открытия",L"A new note will appear here after you open it"):langText(L"Создайте записку кнопкой внизу",L"Create a note with the button below"),{px(h,10),px(h,146),c.rect.right-px(h,10),px(h,173)},11,MUTED,FW_NORMAL,DT_CENTER|DT_SINGLELINE|DT_VCENTER);return 0;}
    if(m==WM_LBUTTONUP){auto result=DefSubclassProc(h,m,w,l);LRESULT hit=SendMessageW(h,LB_ITEMFROMPOINT,0,l);if(HIWORD(hit)==0)PostMessageW(GetParent(h),WM_APP+20,LOWORD(hit),0);return result;}
    if(m==WM_KEYDOWN){if(w==VK_RETURN){auto index=SendMessageW(h,LB_GETCURSEL,0,0);PostMessageW(GetParent(h),WM_APP+20,index,0);return 0;}if(w==VK_ESCAPE){PostMessageW(GetParent(h),WM_CLOSE,0,0);return 0;}}
    if(m==WM_CONTEXTMENU){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int index=int(SendMessageW(h,LB_GETCURSEL,0,0));if(p.x!=-1){POINT c=p;ScreenToClient(h,&c);auto hit=SendMessageW(h,LB_ITEMFROMPOINT,0,MAKELPARAM(c.x,c.y));if(HIWORD(hit)==0)index=LOWORD(hit);}else GetCursorPos(&p);bookContext(p,index);return 0;}
    return DefSubclassProc(h,m,w,l);
}
LRESULT CALLBACK searchSubclass(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(m==WM_SETFOCUS||m==WM_KILLFOCUS)InvalidateRect(GetParent(h),nullptr,FALSE);
    if(m==WM_KEYDOWN){if(w==VK_ESCAPE){PostMessageW(GetParent(h),WM_CLOSE,0,0);return 0;}if(w==VK_RETURN){PostMessageW(GetParent(h),WM_APP+20,0,0);return 0;}if(w==VK_DOWN&&app.popup){SetFocus(app.popup->list);return 0;}}
    if(m==WM_CHAR&&(w==13||w==27))return 0;return DefSubclassProc(h,m,w,l);
}
void layoutPopup(Popup* p){RECT r{};GetClientRect(p->h,&r);auto layout=ui::popupLayout(r.right,r.bottom,dpi(p->h));
    MoveWindow(p->search,layout.search.left,layout.search.top,layout.search.right-layout.search.left,layout.search.bottom-layout.search.top,TRUE);
    MoveWindow(p->list,layout.list.left,layout.list.top,layout.list.right-layout.list.left,layout.list.bottom-layout.list.top,TRUE);
    SendMessageW(p->list,LB_SETITEMHEIGHT,0,px(p->h,p->books?72:84));roundedRegion(p->h,28);
}
int popupAction(HWND h,POINT point){RECT r{};GetClientRect(h,&r);auto layout=ui::popupLayout(r.right,r.bottom,dpi(h));return ui::contains(layout.close,point.x,point.y)?1:ui::contains(layout.footer,point.x,point.y)?2:0;}

void openPopup(bool recentOnly,const std::string& book,bool books){
    closeMenu();closePopup();auto p=std::make_unique<Popup>();p->recent=recentOnly;p->book=book;p->books=books;app.popup=std::move(p);
    RECT hub{};GetWindowRect(app.hub,&hub);int width=px(app.hub,380),height=px(app.hub,520);RECT work=workArea({hub.left,hub.top});height=std::min(height,int(work.bottom-work.top-px(app.hub,20)));
    int x=hub.left-width-px(app.hub,15);if(x<work.left)x=hub.right+px(app.hub,15);RECT rect=fitRect({x,hub.top-height/2,x+width,hub.top+height/2});
    HWND h=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED,L"ListkiPopup",langText(L"Листки — список",L"Listki — list"),WS_POPUP|WS_CLIPCHILDREN,rect.left,rect.top,width,height,app.hub,nullptr,app.instance,app.popup.get());
    if(!h){app.popup.reset();return;}ShowWindow(h,SW_SHOWNORMAL);SetForegroundWindow(h);SetFocus(app.popup->search);
}
LRESULT CALLBACK popupProc(HWND h,UINT m,WPARAM w,LPARAM l){auto* p=(Popup*)GetWindowLongPtrW(h,GWLP_USERDATA);if(m==WM_NCCREATE){p=(Popup*)((CREATESTRUCTW*)l)->lpCreateParams;p->h=h;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)p);}if(!p)return DefWindowProcW(h,m,w,l);
    switch(m){
    case WM_CREATE:{p->f=font(h,13);p->search=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)20,app.instance,nullptr);SendMessageW(p->search,WM_SETFONT,(WPARAM)p->f,TRUE);SendMessageW(p->search,EM_SETCUEBANNER,TRUE,(LPARAM)(p->books?langText(L"Найти блокнот…",L"Find a notebook…"):langText(L"Найти записку…",L"Find a note…")));SetWindowSubclass(p->search,searchSubclass,1,0);
        p->list=CreateWindowExW(0,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|LBS_NOTIFY,0,0,10,10,h,(HMENU)21,app.instance,nullptr);SendMessageW(p->list,WM_SETFONT,(WPARAM)p->f,TRUE);SetWindowSubclass(p->list,listSubclass,1,0);layoutPopup(p);SetLayeredWindowAttributes(h,0,250,LWA_ALPHA);populatePopup();return 0;
    }
    case WM_SIZE:if(p->list)layoutPopup(p);return 0;
    case WM_DPICHANGED:{RECT* r=(RECT*)l;SetWindowPos(h,HWND_TOPMOST,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOACTIVATE);HFONT old=p->f;p->f=font(h,13);SendMessageW(p->search,WM_SETFONT,(WPARAM)p->f,TRUE);SendMessageW(p->list,WM_SETFONT,(WPARAM)p->f,TRUE);if(old)DeleteObject(old);layoutPopup(p);return 0;}
    case WM_COMMAND:if(LOWORD(w)==20&&HIWORD(w)==EN_CHANGE)populatePopup();return 0;
    case WM_APP+20:selectPopup(int(w));return 0;
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE&&app.modal==0)PostMessageW(h,WM_CLOSE,0,0);return 0;
    case WM_CLOSE:closePopup();return 0;
    case WM_MOUSEMOVE:{int hot=popupAction(h,{GET_X_LPARAM(l),GET_Y_LPARAM(l)});if(hot!=p->actionHot){p->actionHot=hot;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_MOUSELEAVE:p->actionHot=0;InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT&&p->actionHot){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
    case WM_LBUTTONDOWN:p->pressed=popupAction(h,{GET_X_LPARAM(l),GET_Y_LPARAM(l)});if(p->pressed)SetCapture(h);return 0;
    case WM_CAPTURECHANGED:p->pressed=0;return 0;
    case WM_LBUTTONUP:{int hit=popupAction(h,{GET_X_LPARAM(l),GET_Y_LPARAM(l)});bool apply=hit&&hit==p->pressed;p->pressed=0;ReleaseCapture();if(!apply)return 0;
        if(hit==1){closePopup();return 0;}
        if(p->books){++app.modal;createBook(h);--app.modal;populatePopup();}
        else if(p->recent){closePopup();openPopup(false);}
        else {std::string book=p->book=="none"?"":p->book;newNote(false,book);}return 0;
    }
    case WM_MEASUREITEM:if(((MEASUREITEMSTRUCT*)l)->CtlType==ODT_LISTBOX){((MEASUREITEMSTRUCT*)l)->itemHeight=px(h,p->books?72:84);return TRUE;}break;
    case WM_DRAWITEM:{auto* d=(DRAWITEMSTRUCT*)l;if(d->CtlType!=ODT_LISTBOX)break;if(d->itemID==UINT(-1)||d->itemID>=p->ids.size())return TRUE;
        RECT r=d->rcItem;fill(d->hDC,r,PAPER);bool selected=d->itemState&ODS_SELECTED,hot=int(d->itemID)==p->hot;
        RECT card{r.left+px(h,4),r.top+px(h,3),r.right-px(h,4),r.bottom-px(h,3)};
        roundFill(d->hDC,card,px(h,11),selected?RGB(237,246,239):hot?RGB(247,250,246):PAPER);
        roundOutline(d->hDC,card,px(h,11),selected?RGB(199,218,203):hot?modern::edge:RGB(237,241,235));
        if(selected&&GetFocus()==p->list){RECT focus=card;InflateRect(&focus,-px(h,2),-px(h,2));roundOutline(d->hDC,focus,px(h,9),mixColor(TEAL,PAPER,65));}
        int x=r.left+px(h,62);std::wstring title,preview,meta;COLORREF chip=RGB(231,240,234);bool pinned=false;
        if(p->books){title=bookTitle(p->ids[d->itemID]);int count=0;for(auto& n:app.state.notes)if(n.book==p->ids[d->itemID])++count;preview=langString(L"Записок: ",L"Notes: ")+std::to_wstring(count);}
        else {auto* n=app.note(p->ids[d->itemID]);if(!n)return TRUE;title=noteTitle(*n);preview=wide(n->preview);if(preview.empty())preview=langString(L"Пустая записка",L"Empty note");meta=bookTitle(n->book)+L"  ·  "+dateText(n->modified);chip=noteColors[n->color];pinned=n->pinned;}
        roundFill(d->hDC,{r.left+px(h,16),r.top+px(h,16),r.left+px(h,48),r.top+px(h,48)},px(h,10),chip);
        glyph(d->hDC,{r.left+px(h,21),r.top+px(h,21),r.left+px(h,43),r.top+px(h,43)},p->books?3:1,mixColor(chip,TEAL,70),px(h,1));
        int top=p->books?13:8;
        label(d->hDC,h,title,{x,r.top+px(h,top),r.right-px(h,pinned?35:17),r.top+px(h,top+24)},13,INK,FW_SEMIBOLD);
        if(pinned)glyph(d->hDC,{r.right-px(h,31),r.top+px(h,13),r.right-px(h,15),r.top+px(h,29)},5,TEAL,px(h,1));
        label(d->hDC,h,preview,{x,r.top+px(h,top+26),r.right-px(h,17),r.top+px(h,top+45)},11,MUTED);
        if(!meta.empty())label(d->hDC,h,meta,{x,r.top+px(h,57),r.right-px(h,17),r.top+px(h,74)},10,MUTED);return TRUE;
    }
    case WM_CTLCOLOREDIT:SetTextColor((HDC)w,INK);SetBkColor((HDC)w,modern::soft);SetDCBrushColor((HDC)w,modern::soft);return (LRESULT)GetStockObject(DC_BRUSH);
    case WM_CTLCOLORLISTBOX:SetBkColor((HDC)w,PAPER);SetDCBrushColor((HDC)w,PAPER);return (LRESULT)GetStockObject(DC_BRUSH);
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{Canvas c(h);fill(c.dc,c.rect,PAPER);auto layout=ui::popupLayout(c.rect.right,c.rect.bottom,dpi(h));
        std::wstring title=p->books?langString(L"Блокноты",L"Notebooks"):p->recent?langString(L"Последние",L"Recent"):p->book.empty()?langString(L"Все записки",L"All notes"):p->book=="none"?langString(L"Без блокнота",L"No notebook"):bookTitle(p->book);
        label(c.dc,h,title,{px(h,22),px(h,15),layout.close.left-px(h,10),px(h,45)},20,INK,FW_SEMIBOLD);
        std::wstring count=(p->books?langString(L"Блокнотов: ",L"Notebooks: "):langString(L"Записок: ",L"Notes: "))+std::to_wstring(p->ids.size());
        std::wstring subtitle=count+(p->books?langString(L"  ·  ПКМ — управление",L"  ·  Right-click to manage"):p->recent?langString(L"  ·  Последние 10",L"  ·  Last 10"):langString(L"  ·  Открыть одним кликом",L"  ·  Open with one click"));
        label(c.dc,h,subtitle,{px(h,23),px(h,45),c.rect.right-px(h,22),px(h,63)},11,MUTED);
        RECT close=winRect(layout.close);if(p->actionHot==1)roundFill(c.dc,close,px(h,9),modern::soft);RECT cross=close;InflateRect(&cross,-px(h,5),-px(h,5));glyph(c.dc,cross,4,MUTED,px(h,1));
        RECT search{px(h,18),px(h,74),c.rect.right-px(h,18),px(h,112)};roundFill(c.dc,search,px(h,10),modern::soft);roundOutline(c.dc,search,px(h,10),GetFocus()==p->search?RGB(190,212,197):modern::edge);
        glyph(c.dc,{px(h,27),px(h,82),px(h,47),px(h,102)},11,MUTED,px(h,1));
        RECT footer=winRect(layout.footer);roundFill(c.dc,footer,px(h,10),p->actionHot==2?RGB(219,237,225):modern::soft);roundOutline(c.dc,footer,px(h,10),modern::edge);
        std::wstring action=p->books?langString(L"Создать блокнот",L"Create notebook"):p->recent?langString(L"Открыть все записки",L"Open all notes"):langString(L"Новая записка",L"New note");
        glyph(c.dc,{footer.left+px(h,13),footer.top+px(h,9),footer.left+px(h,33),footer.bottom-px(h,9)},p->recent?1:0,TEAL,px(h,1));
        label(c.dc,h,action,{footer.left+px(h,44),footer.top,footer.right-px(h,16),footer.bottom},13,TEAL,FW_SEMIBOLD);
        roundOutline(c.dc,c.rect,px(h,13),modern::edge);return 0;
    }
    case WM_DESTROY:if(p->f)DeleteObject(p->f);p->f=nullptr;return 0;
    case WM_NCDESTROY:p->h=nullptr;break;
    }return DefWindowProcW(h,m,w,l);
}

void notebooksMenu(POINT point){HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,1,langText(L"Все записки",L"All notes"));AppendMenuW(m,MF_STRING,2,langText(L"Без блокнота",L"No notebook"));AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    for(size_t i=0;i<app.state.books.size();++i){int count=0;for(auto& n:app.state.notes)if(n.book==app.state.books[i].id)++count;auto title=wide(app.state.books[i].title)+L"  ("+std::to_wstring(count)+L")";AppendMenuW(m,MF_STRING,CMD_BOOK+i,title.c_str());}
    AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,3,langText(L"Создать блокнот…",L"Create notebook…"));AppendMenuW(m,MF_STRING,4,langText(L"Управление блокнотами…",L"Manage notebooks…"));SetForegroundWindow(app.hub);int cmd=styledPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,app.hub,nullptr);DestroyMenu(m);
    if(cmd==1)openPopup(false);if(cmd==2)openPopup(false,"none");if(cmd==3){auto id=createBook(app.hub);if(!id.empty())openPopup(false,utf8(id));}if(cmd==4)openPopup(false,"",true);if(cmd>=CMD_BOOK&&cmd<CMD_BOOK+int(app.state.books.size()))openPopup(false,app.state.books[cmd-CMD_BOOK].id);
}
const wchar_t* runKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
bool autorunEnabled(){HKEY key=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,runKey,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;wchar_t value[32768]{};DWORD type=0,size=sizeof(value);auto result=RegQueryValueExW(key,L"Listki",nullptr,&type,(BYTE*)value,&size);RegCloseKey(key);return result==ERROR_SUCCESS&&type==REG_SZ&&std::wstring(value)==L"\""+app.exe+L"\"";}
void toggleAutorun(){bool enabled=autorunEnabled();HKEY key=nullptr;auto err=RegCreateKeyExW(HKEY_CURRENT_USER,runKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);if(err==ERROR_SUCCESS){if(enabled)err=RegDeleteValueW(key,L"Listki");else{auto value=L"\""+app.exe+L"\"";err=RegSetValueExW(key,L"Listki",0,REG_SZ,(const BYTE*)value.c_str(),DWORD((value.size()+1)*sizeof(wchar_t)));}RegCloseKey(key);}if(err!=ERROR_SUCCESS)messageCard(app.hub,(langString(L"Не удалось изменить автозапуск.\n",L"Could not change startup setting.\n")+sysError(err)).c_str(),langText(L"Листки",L"Listki"),MB_OK|MB_ICONERROR);}
void trayMenu(POINT pt){closeMenu();closePopup();HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,CMD_NEW,langText(L"Новая записка",L"New note"));AppendMenuW(m,MF_STRING,CMD_CLIP,langText(L"Из буфера обмена",L"From clipboard"));AppendMenuW(m,MF_STRING,CMD_RECENT,langText(L"Последние записки",L"Recent notes"));AppendMenuW(m,MF_STRING,CMD_BOOKS,langText(L"Блокноты",L"Notebooks"));AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,CMD_SHOW,langText(L"Показать кнопку и открытые записки",L"Show button and open notes"));AppendMenuW(m,MF_STRING,CMD_HIDE,langText(L"Скрыть все записки",L"Hide all notes"));AppendMenuW(m,MF_STRING,CMD_RESET,langText(L"Вернуть кнопку к правому краю",L"Reset button to the right edge"));
    HMENU opacity=CreatePopupMenu();for(int v:{70,80,90,92,95,100})AppendMenuW(opacity,MF_STRING|(app.state.opacity==v?MF_CHECKED:0),500+v,(std::to_wstring(v)+L"% "+langString(L"непрозрачности",L"opacity")).c_str());AppendMenuW(m,MF_POPUP,(UINT_PTR)opacity,langText(L"Прозрачность неактивных записок",L"Inactive note opacity"));
    HMENU languages=CreatePopupMenu();AppendMenuW(languages,MF_STRING|(gLanguage==Language::Russian?MF_CHECKED:0),CMD_LANG_RU,L"Русский");AppendMenuW(languages,MF_STRING|(gLanguage==Language::English?MF_CHECKED:0),CMD_LANG_EN,L"English");AppendMenuW(m,MF_POPUP,(UINT_PTR)languages,langText(L"Язык",L"Language"));
    AppendMenuW(m,MF_STRING|(autorunEnabled()?MF_CHECKED:0),CMD_AUTORUN,langText(L"Запускать вместе с Windows",L"Start with Windows"));AppendMenuW(m,MF_STRING,CMD_FOLDER,langText(L"Открыть папку данных",L"Open data folder"));AppendMenuW(m,MF_STRING,CMD_ABOUT,langText(L"О программе",L"About"));AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,CMD_EXIT,langText(L"Выход",L"Exit"));ShowWindow(app.hub,SW_SHOWNOACTIVATE);SetForegroundWindow(app.hub);
    int cmd=styledPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,app.hub,nullptr);DestroyMenu(m);PostMessageW(app.hub,WM_NULL,0,0);
    switch(cmd){case CMD_NEW:newNote();break;case CMD_CLIP:newNote(true);break;case CMD_RECENT:openPopup(true);break;case CMD_BOOKS:notebooksMenu(pt);break;
    case CMD_SHOW:ensureHub();for(auto& n:app.state.notes)if(n.visible)showNote(n.id,false);break;
    case CMD_HIDE:for(auto& p:app.windows)if(p.second->h){flushNote(p.second.get());ShowWindow(p.second->h,SW_HIDE);}saveState();break;
    case CMD_LANG_RU:setLanguage(Language::Russian);break;case CMD_LANG_EN:setLanguage(Language::English);break;
    case CMD_AUTORUN:toggleAutorun();break;case CMD_FOLDER:ShellExecuteW(app.hub,L"open",app.data.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;case CMD_RESET:resetHub();break;
    case CMD_ABOUT:messageCard(app.hub,langText(L"Листки 1.3\n\nЛёгкие портативные записки для Windows 10/11.\n\nКнопку можно перетаскивать мышью.\nНазвание меняется по клику в шапке записки.\nКрестик и Esc скрывают записку.\nВыделите текст для быстрого форматирования.\nПравый клик — оформление и блокнот.\nДанные сохраняются автоматически рядом с EXE.\n\nВыход из приложения — через меню значка в трее.\nРаботает локально, без сети.",L"Listki 1.3\n\nLightweight portable notes for Windows 10/11.\n\nDrag the floating button to move it.\nClick the note title to edit it.\nThe close button and Esc hide a note.\nSelect text for quick formatting.\nRight-click for formatting and notebooks.\nData is saved automatically next to the EXE.\n\nExit through the tray menu.\nWorks locally, without network access."),langText(L"О программе «Листки»",L"About Listki"),MB_OK|MB_ICONINFORMATION);break;
    case CMD_EXIT:exitApp();break;
    default:if(cmd>=570&&cmd<=600){app.state.opacity=cmd-500;for(auto& p:app.windows)if(p.second->h)applyOpacity(p.second.get());saveState();}
    }
}

void updateTrayTip(){if(!app.host)return;NOTIFYICONDATAW ni{};ni.cbSize=sizeof(ni);ni.hWnd=app.host;ni.uID=1;ni.uFlags=NIF_TIP|NIF_SHOWTIP;wcscpy(ni.szTip,langText(L"Листки — записки под рукой",L"Listki — notes at hand"));Shell_NotifyIconW(NIM_MODIFY,&ni);}
void addTray(){NOTIFYICONDATAW ni{};ni.cbSize=sizeof(ni);ni.hWnd=app.host;ni.uID=1;ni.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_SHOWTIP;ni.uCallbackMessage=MSG_TRAY;ni.hIcon=app.icon;wcscpy(ni.szTip,langText(L"Листки — записки под рукой",L"Listki — notes at hand"));Shell_NotifyIconW(NIM_ADD,&ni);ni.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&ni);}
void exitApp(){if(!flushAll()){if(messageCard(app.hub,langText(L"Не все данные удалось сохранить.\n\nЗакрыть приложение с потерей несохранённых изменений?",L"Some data could not be saved.\n\nClose the application and lose unsaved changes?"),langText(L"Листки",L"Listki"),MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return;}
    app.exiting=true;closeMenu();closePopup();NOTIFYICONDATAW ni{};ni.cbSize=sizeof(ni);ni.hWnd=app.host;ni.uID=1;Shell_NotifyIconW(NIM_DELETE,&ni);
    for(auto& p:app.windows)if(p.second->h)DestroyWindow(p.second->h);for(auto h:app.petals)if(h)DestroyWindow(h);DestroyWindow(app.hub);DestroyWindow(app.host);PostQuitMessage(0);
}
LRESULT CALLBACK hostProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==app.taskbarMessage&&app.taskbarMessage){addTray();return 0;}
    switch(m){
    case MSG_TRAY:{UINT ev=LOWORD(l);if(ev==WM_CONTEXTMENU||ev==WM_RBUTTONUP){POINT p{};GetCursorPos(&p);trayMenu(p);}else if(ev==NIN_SELECT||ev==NIN_KEYSELECT){ensureHub();SetForegroundWindow(app.hub);showMenu();}return 0;}
    case MSG_SHOW:ensureHub();SetForegroundWindow(app.hub);return 0;
    case WM_TIMER:if(w==1){KillTimer(h,1);saveState();}return 0;
    case WM_QUERYENDSESSION:flushAll();return TRUE;
    case WM_ENDSESSION:if(w){flushAll();app.exiting=true;}return 0;
    case WM_DISPLAYCHANGE:if(app.hub){closeMenu();ensureHub();for(auto& nw:app.windows)if(nw.second->h){RECT r{};GetWindowRect(nw.second->h,&r);r=fitRect(r);SetWindowPos(nw.second->h,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_NOZORDER);}}return 0;
    case WM_CLOSE:exitApp();return 0;
    }return DefWindowProcW(h,m,w,l);
}

bool loadState(){bool recovered=false;const auto index=app.data+L"\\index.dat";bool bad=false;
    if(exists(index)){try{app.state=decode(readBytes(index));}catch(...){bad=true;}}
    if(bad||(!exists(index)&&exists(index+L".bak"))){try{app.state=decode(readBytes(index+L".bak"));recovered=true;if(exists(index))CopyFileW(index.c_str(),(index+L".damaged").c_str(),FALSE);atomicWrite(index,encode(app.state),false);}catch(...){
            if(exists(index))CopyFileW(index.c_str(),(index+L".damaged").c_str(),FALSE);app.state=State{};recovered=true;
        }}
    // A complete RTF written just before a power loss may not yet appear in the index.
    WIN32_FIND_DATAW f{};HANDLE find=FindFirstFileW((app.data+L"\\*.rtf").c_str(),&f);if(find!=INVALID_HANDLE_VALUE){do{
        std::wstring filename=f.cFileName;if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;auto id=utf8(filename.substr(0,filename.size()-4));if(!validId(id)||app.note(id))continue;
        Note n;n.id=id;n.title=utf8(langString(L"Восстановленная записка",L"Recovered note"));n.visible=false;n.created=n.modified=n.accessed=(uint64_t(f.ftLastWriteTime.dwHighDateTime)<<32)|f.ftLastWriteTime.dwLowDateTime;app.state.notes.push_back(n);recovered=true;
    }while(FindNextFileW(find,&f));FindClose(find);}
    return recovered;
}
void registerClass(const wchar_t* name,WNDPROC proc,UINT extra=CS_DROPSHADOW){WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW|extra;wc.lpfnWndProc=proc;wc.hInstance=app.instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=app.icon;wc.hIconSm=app.icon;wc.lpszClassName=name;if(!RegisterClassExW(&wc))throw std::runtime_error("Window registration failed");}

#ifndef LISTKI_TEST
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    app.instance=instance;SetErrorMode(SEM_FAILCRITICALERRORS);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&input,nullptr);
    INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ic);
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);app.exe=path;app.dir=app.exe.substr(0,app.exe.find_last_of(L"\\/"));app.data=app.dir+L"\\Listki-data";CreateDirectoryW(app.data.c_str(),nullptr);loadLanguage();
    auto keyPath=app.data;for(auto& c:keyPath)c=towlower(c);auto key=std::to_wstring(checksum(utf8(keyPath)));auto mutexName=L"Local\\Listki-"+key;app.hostClass=L"ListkiHost-"+key;
    app.mutex=CreateMutexW(nullptr,FALSE,mutexName.c_str());if(!app.mutex){messageCard(nullptr,langText(L"Не удалось создать блокировку запуска.",L"Could not create the single-instance lock."),langText(L"Листки",L"Listki"),MB_OK|MB_ICONERROR);return 1;}
    if(GetLastError()==ERROR_ALREADY_EXISTS){HWND h=FindWindowW(app.hostClass.c_str(),nullptr);if(h)PostMessageW(h,MSG_SHOW,0,0);CloseHandle(app.mutex);return 0;}
    HMODULE rich=LoadLibraryExW(L"Msftedit.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!rich){messageCard(nullptr,langText(L"Не найден системный редактор Msftedit.dll. Нужна Windows 10 или новее.",L"The system editor Msftedit.dll was not found. Windows 10 or newer is required."),langText(L"Листки",L"Listki"),MB_OK|MB_ICONERROR);return 1;}
    auto probe=app.data+L"\\write-check.tmp";
    if(!atomicWrite(probe,"ok",false)){messageCard(nullptr,langText(L"Не удаётся записать данные рядом с программой.\n\nПереместите EXE в обычную папку с правом записи, например Documents\\Listki, и запустите снова.",L"Listki cannot write data next to the program.\n\nMove the EXE to a writable folder, for example Documents\\Listki, and run it again."),langText(L"Листки — папка недоступна",L"Listki — data folder unavailable"),MB_OK|MB_ICONERROR);return 1;}DeleteFileW(probe.c_str());
    bool recovered=loadState();app.icon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR);if(!app.icon)app.icon=LoadIconW(nullptr,IDI_APPLICATION);
    try{registerClass(app.hostClass.c_str(),hostProc,0);registerClass(L"ListkiHub",hubProc);registerClass(L"ListkiPetal",petalProc);registerClass(L"ListkiNote",noteProc);registerClass(L"ListkiPopup",popupProc);registerClass(L"ListkiFormatBar",formatBarProc);}catch(...){return 1;}
    app.host=CreateWindowExW(WS_EX_TOOLWINDOW,app.hostClass.c_str(),L"ListkiHost",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);app.taskbarMessage=RegisterWindowMessageW(L"TaskbarCreated");
    app.hub=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED,L"ListkiHub",langText(L"Листки",L"Listki"),WS_POPUP,0,0,72,72,app.host,nullptr,instance,nullptr);
    for(int i=0;i<4;++i)app.petals[i]=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_NOACTIVATE,L"ListkiPetal",L"",WS_POPUP,0,0,124,51,app.hub,nullptr,instance,(void*)(INT_PTR)i);
    addTray();app.loading=false;
    if(app.state.hubX==std::numeric_limits<int>::min())resetHub();else{int sz=px(app.hub,72);RECT r=fitRect({app.state.hubX,app.state.hubY,app.state.hubX+sz,app.state.hubY+sz});SetWindowPos(app.hub,HWND_TOPMOST,r.left,r.top,sz,sz,SWP_NOACTIVATE|SWP_SHOWWINDOW);}
    for(auto& n:app.state.notes)if(n.visible)showNote(n.id,false);
    if(recovered)notify(langString(L"Данные восстановлены. Проверьте раздел «Все записки».",L"Data was recovered. Check the “All notes” section."),NIIF_INFO);saveState();
    MSG msg{};int result;while((result=GetMessageW(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    app.windows.clear();CloseHandle(app.mutex);FreeLibrary(rich);Gdiplus::GdiplusShutdown(token);CoUninitialize();return result==-1?1:int(msg.wParam);
}
#endif
