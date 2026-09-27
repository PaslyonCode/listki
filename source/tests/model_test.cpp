#include "../src/model.hpp"
#include <cassert>
#include <iostream>
using namespace listki;
int main(){
    Note untitled;untitled.preview=u8"Текст записки, который не должен становиться заголовком";
    assert(noteDisplayTitle(untitled)==u8"Новая записка");
    untitled.preview=u8"Другой текст";assert(noteDisplayTitle(untitled)==u8"Новая записка");
    untitled.title=u8"Мои планы";assert(noteDisplayTitle(untitled)==u8"Мои планы");
    State s;s.hubX=-1200;s.hubY=360;s.opacity=92;
    s.books.push_back({std::string(32,'b'),u8"Исследования 🧭"});
    for(int i=0;i<15;++i){Note n;n.id=std::string(31,'a')+"0123456789abcde"[i];n.title=u8"Записка № "+std::to_string(i);n.book=s.books[0].id;n.preview=u8"Русский текст\nвторая строка\t✓";n.x=-200;n.pinned=i%2;n.created=100;n.modified=100+i;n.accessed=100;n.visible=i%3;s.notes.push_back(n);}
    auto bytes=encode(s);auto restored=decode(bytes);assert(encode(restored)==bytes);assert(restored.hubX==-1200);assert(restored.notes[0].preview==s.notes[0].preview);
    auto ids=recent(s);assert(ids.size()==10);assert(ids.front()==s.notes.back().id);
    s.notes[0].accessed=1000;assert(recent(s)[0]==s.notes[0].id);
    auto group=s.books[0].id;removeBook(s,group);assert(s.books.empty());assert(s.notes.size()==15);for(const auto& n:s.notes)assert(n.book.empty());
    int rejected=0;for(size_t i=0;i<bytes.size();i+=7){auto broken=bytes;broken[i]^=0x55;try{decode(broken);}catch(...){++rejected;}}
    assert(rejected==int((bytes.size()+6)/7));
    for(size_t i=0;i<bytes.size();i+=31){bool fail=false;try{decode(bytes.substr(0,i));}catch(...){fail=true;}assert(fail);}
    assert(!validId("../../secrets"));assert(!validId(std::string(32,'z')));assert(validId(std::string(32,'a')));
    State invalid=s;invalid.notes[0].id="../bad";bool caught=false;try{decode(encode(invalid));}catch(...){caught=true;}assert(caught);
    invalid=s;invalid.notes[0].book=std::string(32,'b');assert(decode(encode(invalid)).notes[0].book.empty());
    std::cout<<"PASS: fixed default title, custom title, Unicode metadata, coordinates, recent ordering and limit, notebook deletion, corrupt/truncated files, identifier validation\n";
}
