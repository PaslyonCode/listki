#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace listki {
struct Book { std::string id, title; };
struct Note {
    std::string id, title, book, preview;
    int x = 180, y = 180, w = 350, h = 320;
    int color = 0;
    bool pinned = false, visible = true;
    uint64_t created = 0, modified = 0, accessed = 0;
};
inline std::string noteDisplayTitle(const Note& note) {
    return note.title.empty() ? u8"Новая записка" : note.title;
}
struct State {
    int hubX = std::numeric_limits<int>::min(), hubY = 0;
    int opacity = 92;
    std::vector<Book> books;
    std::vector<Note> notes;
};
inline uint32_t checksum(const std::string& data) {
    uint32_t h = 2166136261U;
    for (unsigned char c : data) { h ^= c; h *= 16777619U; }
    return h;
}
class Writer {
public:
    std::string bytes;
    void u32(uint32_t n) { for (int i=0;i<4;++i) bytes.push_back(char(n>>(i*8))); }
    void u64(uint64_t n) { u32(uint32_t(n)); u32(uint32_t(n>>32)); }
    void str(const std::string& s) { if(s.size()>16*1024*1024) throw std::runtime_error("Oversized field"); u32(uint32_t(s.size())); bytes+=s; }
};
class Reader {
    const std::string& bytes; size_t pos=0;
public:
    explicit Reader(const std::string& b):bytes(b){}
    uint32_t u32() { if(pos+4>bytes.size()) throw std::runtime_error("Truncated index"); uint32_t n=0; for(int i=0;i<4;++i) n |= uint32_t((unsigned char)bytes[pos++])<<(i*8); return n; }
    uint64_t u64() { uint64_t lo=u32(); return lo | uint64_t(u32())<<32; }
    std::string str() { uint32_t n=u32(); if(n>16*1024*1024 || n>bytes.size()-pos) throw std::runtime_error("Invalid length"); auto s=bytes.substr(pos,n); pos+=n; return s; }
    bool done() const { return pos==bytes.size(); }
};
inline bool validId(const std::string& s) {
    if(s.size()!=32) return false;
    return std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
inline std::string encode(const State& s) {
    Writer p;
    p.u32(uint32_t(s.hubX));p.u32(uint32_t(s.hubY));p.u32(s.opacity);
    p.u32(uint32_t(s.books.size()));
    for(const auto& b:s.books){p.str(b.id);p.str(b.title);}
    p.u32(uint32_t(s.notes.size()));
    for(const auto& n:s.notes){
        p.str(n.id);p.str(n.title);p.str(n.book);p.str(n.preview);
        p.u32(uint32_t(n.x));p.u32(uint32_t(n.y));p.u32(n.w);p.u32(n.h);
        p.u32(n.color);p.u32(n.pinned?1:0);p.u32(n.visible?1:0);
        p.u64(n.created);p.u64(n.modified);p.u64(n.accessed);
    }
    Writer h;h.bytes="LISTKI01";h.u32(uint32_t(p.bytes.size()));h.u32(checksum(p.bytes));
    return h.bytes+p.bytes;
}
inline State decode(const std::string& blob) {
    if(blob.size()<16||blob.substr(0,8)!="LISTKI01") throw std::runtime_error("Invalid index header");
    std::string header=blob.substr(8,8);Reader h(header);
    auto len=h.u32(),sum=h.u32();auto payload=blob.substr(16);
    if(payload.size()!=len||checksum(payload)!=sum) throw std::runtime_error("Invalid index checksum");
    Reader r(payload);State s;
    s.hubX=int32_t(r.u32());s.hubY=int32_t(r.u32());s.opacity=int(r.u32());
    if(s.opacity<60||s.opacity>100) throw std::runtime_error("Invalid opacity");
    std::unordered_set<std::string> ids;
    uint32_t count=r.u32();if(count>10000)throw std::runtime_error("Too many notebooks");
    for(uint32_t i=0;i<count;++i){Book b{r.str(),r.str()};if(!validId(b.id)||!ids.insert(b.id).second)throw std::runtime_error("Invalid notebook id");s.books.push_back(b);}
    count=r.u32();if(count>100000)throw std::runtime_error("Too many notes");
    ids.clear();
    for(uint32_t i=0;i<count;++i){
        Note n;n.id=r.str();n.title=r.str();n.book=r.str();n.preview=r.str();
        if(!validId(n.id)||!ids.insert(n.id).second)throw std::runtime_error("Invalid note id");
        n.x=int32_t(r.u32());n.y=int32_t(r.u32());n.w=int(r.u32());n.h=int(r.u32());
        n.color=int(r.u32());auto pin=r.u32(),vis=r.u32();
        if(n.w<160||n.w>30000||n.h<120||n.h>30000||n.color<0||n.color>4||pin>1||vis>1)throw std::runtime_error("Invalid note settings");
        n.pinned=pin;n.visible=vis;n.created=r.u64();n.modified=r.u64();n.accessed=r.u64();
        if(!n.book.empty() && std::none_of(s.books.begin(),s.books.end(),[&](const Book& b){return b.id==n.book;}))n.book.clear();
        s.notes.push_back(n);
    }
    if(!r.done())throw std::runtime_error("Trailing data");
    return s;
}
inline std::vector<std::string> recent(const State& s,size_t limit=10) {
    std::vector<const Note*> items;
    for(const auto& n:s.notes)items.push_back(&n);
    std::stable_sort(items.begin(),items.end(),[](const Note* a,const Note* b){return std::max(a->modified,a->accessed)>std::max(b->modified,b->accessed);});
    std::vector<std::string> out;
    for(size_t i=0;i<std::min(limit,items.size());++i)out.push_back(items[i]->id);
    return out;
}
inline void removeBook(State& s,const std::string& id) {
    for(auto& n:s.notes)if(n.book==id)n.book.clear();
    s.books.erase(std::remove_if(s.books.begin(),s.books.end(),[&](const Book& b){return b.id==id;}),s.books.end());
}
}
