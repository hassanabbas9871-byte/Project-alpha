// This optional desktop test uses the actual private application renderer.
// Including its translation unit keeps this test from maintaining a second,
// simplified copy of the UI that could pass while the real game is broken.
// Rename the normal entry point so this file can provide its own test main().
#define main ludoGameEntry
#include "main.cpp"
#undef main

#include <rlgl.h>

namespace {
struct Frame {
    int width{}, height{};
    vector<Color> pixels;
    Color at(int x, int y) const { return pixels[static_cast<size_t>(y)*width+x]; }
};

struct Layout {
    float scale{};
    Vector2 offset{};
    Vector2 point(Vector2 logical) const {
        return {offset.x+logical.x*scale,offset.y+logical.y*scale};
    }
};

Layout layoutFor(const Surface& surface) {
    const float scale=min(surface.width/Width,surface.height/Height);
    return {scale,{(surface.width-Width*scale)/2,(surface.height-Height*scale)/2}};
}

Frame renderFrame(Application& app, bool inheritedStateIsWrong, bool restoreViewport = true) {
    const Surface surface=displaySurface();
    const Layout layout=layoutFor(surface);
    BeginDrawing();
    if(inheritedStateIsWrong) {
        // Reproduce the important failure condition: a framework can leave a
        // viewport, projection and DPI matrix belonging to a previous size.
        // The game's new frame boundary must replace ALL three, not merely
        // compensate the board while its labels inherit the old transform.
        rlDrawRenderBatchActive();
        rlViewport(37,19,surface.width*4/5,surface.height*4/5);
        rlMatrixMode(RL_PROJECTION);
        rlLoadIdentity();
        rlOrtho(0,surface.width*1.25,surface.height*1.25,0,-1,1);
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
        rlTranslatef(117,53,0);
        rlScalef(1.25f,1.25f,1);
    }
    if(restoreViewport) beginNativeFrame(surface);
    app.draw(layout.scale,layout.offset,{-100,-100},false);
    rlDrawRenderBatchActive();
    Image image{rlReadScreenPixels(surface.width,surface.height),surface.width,surface.height,
                1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if(!image.data) throw runtime_error("Could not read the rendered framebuffer.");
    const auto* colors=static_cast<Color*>(image.data);
    Frame frame{surface.width,surface.height,vector<Color>(colors,colors+surface.width*surface.height)};
    UnloadImage(image);
    EndDrawing();
    return frame;
}

bool equalFrames(const Frame& a,const Frame& b) {
    if(a.width!=b.width || a.height!=b.height) return false;
    for(size_t i=0;i<a.pixels.size();++i) {
        const Color x=a.pixels[i],y=b.pixels[i];
        if(x.r!=y.r || x.g!=y.g || x.b!=y.b || x.a!=y.a) return false;
    }
    return true;
}

bool white(Color color) { return color.r>230 && color.g>230 && color.b>230; }
bool near(Color actual,Color expected) {
    return abs(static_cast<int>(actual.r)-expected.r)<12 &&
           abs(static_cast<int>(actual.g)-expected.g)<12 &&
           abs(static_cast<int>(actual.b)-expected.b)<12;
}

// Checking actual pixels avoids a vacuous comparison in which both screenshots
// contain the same incorrectly aligned or completely missing labels.
bool numberedCone(const Frame& frame,const Layout& layout,Vector2 logical,Color color) {
    const Vector2 center=layout.point(logical);
    const float innerRadius=12*layout.scale, outerRadius=16*layout.scale;
    int whitePixels=0,coloredPixels=0;
    for(int y=max(0,static_cast<int>(floor(center.y-outerRadius)));y<min(frame.height,static_cast<int>(ceil(center.y+outerRadius)));++y)
        for(int x=max(0,static_cast<int>(floor(center.x-outerRadius)));x<min(frame.width,static_cast<int>(ceil(center.x+outerRadius)));++x) {
            const float distance=hypotf(x+.5f-center.x,y+.5f-center.y);
            if(distance<innerRadius && white(frame.at(x,y))) ++whitePixels;
            if(distance<outerRadius && near(frame.at(x,y),color)) ++coloredPixels;
        }
    return whitePixels>=8 && coloredPixels>=30;
}

bool centeredButtonLabel(const Frame& frame,const Layout& layout,Rectangle logical) {
    const Vector2 topLeft=layout.point({logical.x,logical.y});
    const float width=logical.width*layout.scale,height=logical.height*layout.scale;
    int left=frame.width,top=frame.height,right=-1,bottom=-1;
    // The active Start button has dark fill and a white caption. Its bounding
    // box must remain centered INSIDE the same shape at every tested size.
    for(int y=max(0,static_cast<int>(topLeft.y+5));y<min(frame.height,static_cast<int>(topLeft.y+height-5));++y)
        for(int x=max(0,static_cast<int>(topLeft.x+5));x<min(frame.width,static_cast<int>(topLeft.x+width-5));++x)
            if(white(frame.at(x,y))) {
                left=min(left,x); right=max(right,x); top=min(top,y); bottom=max(bottom,y);
            }
    if(right-left<30 || bottom-top<8) return false;
    return abs((left+right)*.5f-(topLeft.x+width*.5f))<width*.08f &&
           abs((top+bottom)*.5f-(topLeft.y+height*.5f))<height*.25f;
}

void settleResize(int width,int height) {
    SetWindowSize(width,height);
    // EndDrawing polls native events. Let the real framebuffer callback settle
    // before recording the size; the poisoned-state case then tests recovery.
    for(int i=0;i<4;++i) { BeginDrawing(); ClearBackground(BLACK); EndDrawing(); }
}
} // namespace

int main(int argc,char** argv) {
    (void)argc;
    try {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
        InitWindow(1200,675,"Ludo display regression");
        if(!IsWindowReady()) throw runtime_error("A graphical desktop is required for the optional display test.");
        SetTargetFPS(0);
        int checks=0;
        {
            Application lobby(argv[0]);
            Application match(argv[0]);
            match.preset("classes");
            match.start(true);
            for(const auto& size:array<pair<int,int>,3>{{{1717,966},{1200,675},{1077,606}}}) {
                settleResize(size.first,size.second);
                const Surface surface=displaySurface();
                const Layout layout=layoutFor(surface);
                const Frame normal=renderFrame(lobby,false);
                const Frame stale=renderFrame(lobby,true);
                if(!equalFrames(normal,stale)) throw runtime_error("Inherited DPI/viewport state changed the lobby's rendered pixels.");
                ++checks;
                // This negative control proves the poisoned state actually
                // changes pixels when the frame-boundary repair is omitted.
                const Frame unrepaired=renderFrame(lobby,true,false);
                if(equalFrames(normal,unrepaired)) throw runtime_error("Invalid test setup: stale viewport did not affect the negative control.");
                ++checks;
                for(int player=0;player<4;++player) for(int cone=0;cone<4;++cone)
                    if(!numberedCone(normal,layout,basePosition(player,cone),Colors[player]))
                        throw runtime_error("A lobby numeral is missing from its colored cone.");
                ++checks;
                if(!centeredButtonLabel(normal,layout,{700,810,870,60}))
                    throw runtime_error("Start caption does not fit the visible Start button.");
                ++checks;
                const Frame normalMatch=renderFrame(match,false);
                const Frame staleMatch=renderFrame(match,true);
                if(!equalFrames(normalMatch,staleMatch)) throw runtime_error("Inherited DPI/viewport state changed the match's rendered pixels.");
                ++checks;
                // Every player's fourth cone remains in base in this preview.
                // Their real white numerals must be inside those actual circles.
                for(int player=0;player<4;++player)
                    if(!numberedCone(normalMatch,layout,basePosition(player,3),Colors[player]))
                        throw runtime_error("A match numeral is missing from its colored cone.");
                ++checks;
                cout<<"PASS display "<<surface.width<<'x'<<surface.height<<" (lobby, match, stale DPI matrices, cone numerals, button caption)\n";
            }
        }
        CloseWindow();
        cout<<checks<<" display checks passed.\n";
        return 0;
    } catch(const exception& error) {
        cerr<<"Display regression: "<<error.what()<<'\n';
        if(IsWindowReady()) CloseWindow();
        return 1;
    }
}
