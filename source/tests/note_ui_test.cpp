#include "../src/note_ui.hpp"
#include <cassert>
#include <iostream>
using namespace listki::ui;
int main(){
    for(int d:{96,120,144,192,240})for(int logicalWidth:{260,350,560}){
        int width=scale(logicalWidth,d);auto buttons=headerButtons(width,d);
        auto title=titleRect(width,d);assert(title.left>=scale(18,d));assert(title.right>title.left);assert(title.right+scale(7,d)<=buttons[0].left);
        assert(title.top>=scale(10,d)&&title.bottom<=scale(38,d));assert(contains(title,(title.left+title.right)/2,(title.top+title.bottom)/2));
        for(int i=0;i<5;++i){auto r=buttons[i];assert(r.left>=scale(85,d));assert(r.right<=width-scale(10,d));assert(r.bottom<scale(48,d));
            assert(headerHit(width,d,(r.left+r.right)/2,(r.top+r.bottom)/2)==HeaderAction(i+1));
            if(i)assert(buttons[i-1].right<=r.left);
        }
        assert(headerHit(width,d,scale(20,d),scale(20,d))==NoAction);
        assert(headerHit(width,d,width-scale(10,d),scale(48,d))==NoAction);
        assert(headerHit(width,d,(buttons[1].left+buttons[1].right)/2,scale(24,d))==Delete);
        assert(headerHit(width,d,(buttons[2].left+buttons[2].right)/2,scale(24,d))==Color);
    }
    for(int d:{96,120,144,192,240}){
        for(int height:{330,520,720}){
            auto p=popupLayout(scale(380,d),scale(height,d),d);
            assert(p.search.right>p.search.left&&p.search.bottom<p.list.top);
            assert(p.list.right>p.list.left&&p.list.bottom>p.list.top&&p.list.bottom<p.footer.top);
            assert(p.close.bottom<p.search.top&&p.footer.bottom<=scale(height-15,d));
        }
        auto format=formatButtons(d);for(int i=0;i<4;++i){auto r=format[i];assert(r.left>=0&&r.right<scale(toolbarWidth,d));assert(r.top>=0&&r.bottom<scale(toolbarHeight,d));assert(formatHit(d,(r.left+r.right)/2,(r.top+r.bottom)/2)==i+1);if(i)assert(format[i-1].right<=r.left);}
        for(Rect work:{Rect{0,0,1920,1080},Rect{-1920,-500,0,580},Rect{0,0,800,600}}){
            for(int x:{work.left,work.left+10,work.right-10,work.right})for(int y:{work.top,work.top+30,work.bottom-5,work.bottom}){
                Rect r=toolbarPlacement(x,y,scale(20,d),work,d);assert(r.left>=work.left&&r.top>=work.top&&r.right<=work.right&&r.bottom<=work.bottom);assert(r.right>r.left&&r.bottom>r.top);
            }
        }
    }
    Rect above=toolbarPlacement(400,400,20,{0,0,1920,1080},96);assert(above.bottom<400);
    Rect below=toolbarPlacement(400,0,20,{0,0,1920,1080},96);assert(below.top>=20);
    std::cout<<"PASS: inline title field, popup controls and list separation, header targets and spacing, compact window, 100-250% DPI, formatting targets, toolbar placement at screen edges and on negative-coordinate monitors\n";
}
