#pragma once
#include <algorithm>
#include <array>

namespace listki::ui {
struct Rect { int left, top, right, bottom; };
enum HeaderAction { NoAction=0, Pin=1, Delete=2, Color=3, More=4, Close=5 };
constexpr int toolbarWidth=206, toolbarHeight=42;
inline int scale(int value,int dpi) { return (value*dpi+48)/96; }
inline bool contains(const Rect& r,int x,int y) { return x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom; }
inline std::array<Rect,5> headerButtons(int width,int dpi) {
    std::array<Rect,5> result{};
    const int widths[]={26,26,36,26,26};
    int right=width-scale(10,dpi);
    for(int i=4;i>=0;--i){
        result[i]={right-scale(widths[i],dpi),scale(10,dpi),right,scale(38,dpi)};
        right=result[i].left-scale(2,dpi);
    }
    return result;
}
inline Rect titleRect(int width,int dpi) {
    auto buttons=headerButtons(width,dpi);
    return {scale(23,dpi),scale(16,dpi),buttons[0].left-scale(9,dpi),scale(34,dpi)};
}
inline HeaderAction headerHit(int width,int dpi,int x,int y) {
    auto buttons=headerButtons(width,dpi);
    for(int i=0;i<5;++i)if(contains(buttons[i],x,y))return HeaderAction(i+1);
    return NoAction;
}
inline std::array<Rect,4> formatButtons(int dpi) {
    return {{{scale(7,dpi),scale(5,dpi),scale(39,dpi),scale(37,dpi)},
             {scale(41,dpi),scale(5,dpi),scale(73,dpi),scale(37,dpi)},
             {scale(75,dpi),scale(5,dpi),scale(107,dpi),scale(37,dpi)},
             {scale(120,dpi),scale(5,dpi),scale(199,dpi),scale(37,dpi)}}};
}
inline int formatHit(int dpi,int x,int y) {
    auto buttons=formatButtons(dpi);
    for(int i=0;i<4;++i)if(contains(buttons[i],x,y))return i+1;
    return 0;
}
struct PopupLayout {Rect search,list,close,footer;};
inline PopupLayout popupLayout(int width,int height,int dpi) {
    return {{scale(51,dpi),scale(84,dpi),width-scale(29,dpi),scale(104,dpi)},
            {scale(12,dpi),scale(123,dpi),width-scale(12,dpi),std::max(scale(124,dpi),height-scale(68,dpi))},
            {width-scale(50,dpi),scale(20,dpi),width-scale(20,dpi),scale(50,dpi)},
            {scale(18,dpi),height-scale(55,dpi),width-scale(18,dpi),height-scale(16,dpi)}};
}
inline Rect toolbarPlacement(int anchorX,int anchorY,int lineHeight,Rect work,int dpi) {
    int gap=scale(8,dpi),margin=scale(5,dpi);
    int width=std::min(scale(toolbarWidth,dpi),work.right-work.left-2*margin);
    int height=std::min(scale(toolbarHeight,dpi),work.bottom-work.top-2*margin);
    int x=anchorX-scale(12,dpi),y=anchorY-height-gap;
    if(y<work.top+margin)y=anchorY+lineHeight+gap;
    x=std::clamp(x,work.left+margin,work.right-width-margin);
    y=std::clamp(y,work.top+margin,work.bottom-height-margin);
    return {x,y,x+width,y+height};
}
}
