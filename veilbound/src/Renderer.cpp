#include "Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <unordered_set>

// A shorthand inside one implementation file is convenient without changing
// the names available in every other file that includes our headers.
using namespace std;

namespace veil {
namespace {
const Color Ink{14, 19, 27, 255}, Panel{22, 29, 38, 255}, Raised{31, 40, 49, 255};
const Color Paper{235, 228, 207, 255}, Gold{213, 178, 113, 255}, Muted{143, 160, 164, 255};
const Color Teal{112, 208, 188, 255}, Red{226, 117, 119, 255}, Rule{60, 71, 77, 255};
const array<Color, 3> HeroColors{{Teal, {143, 182, 235, 255}, {207, 157, 225, 255}}};
const array<Color, 8> Floors{{{38,47,51,255},{49,46,43,255},{29,46,53,255},{41,50,48,255},
                            {49,39,52,255},{36,40,51,255},{52,40,36,255},{36,47,45,255}}};

Vector2 center(Pos p) { return {BoardRect.x + (p.x + .5f) * TileSize, BoardRect.y + (p.y + .5f) * TileSize}; }
Rectangle tile(Pos p) { return {BoardRect.x + p.x * TileSize, BoardRect.y + p.y * TileSize, TileSize, TileSize}; }
int distance(Pos a, Pos b) { return abs(a.x - b.x) + abs(a.y - b.y); }
Color fade(Color c, float a) { return Fade(c, clamp(a, 0.f, 1.f)); }
void panel(Rectangle r, Color c = Panel) {
    DrawRectangleRounded(r, .06f, 8, c);
    DrawRectangleRoundedLinesEx(r, .06f, 8, 1, fade(Rule, .7f));
}
void bar(Rectangle r, float fraction, Color color) {
    DrawRectangleRec(r, Raised);
    DrawRectangleRec({r.x, r.y, r.width * clamp(fraction, 0.f, 1.f), r.height}, color);
}
void sigil(Vector2 p, float radius, Color color, float phase = 0) {
    DrawCircleLinesV(p, radius, color);
    DrawCircleLinesV(p, radius * .72f, fade(color, .55f));
    for (int i = 0; i < 6; ++i) {
        const float a = phase + i * 6.2831853f / 6;
        const float b = a + 2.0943951f;
        DrawLineEx({p.x + cos(a) * radius * .72f, p.y + sin(a) * radius * .72f},
                   {p.x + cos(b) * radius * .72f, p.y + sin(b) * radius * .72f}, 1, fade(color, .6f));
    }
}
string slotName(Slot s) { return array<string,4>{"WEAPON","ARMOR","CHARM","BOOTS"}[static_cast<int>(s)]; }
string aiName(Ai a) { return array<string,10>{"MELEE","MARKSMAN","MEDIC","BRUTE","STALKER","SUMMONER","ARTILLERY","LEECH","GUARDIAN","TRICKSTER"}[static_cast<int>(a)]; }
string played(double seconds) {
    const int minutes = static_cast<int>(seconds / 60);
    return to_string(minutes / 60) + "h " + to_string(minutes % 60) + "m";
}
} // namespace

Renderer::Renderer(const filesystem::path& executableDirectory) {
    const array paths{executableDirectory / "assets/DejaVuSans.ttf", filesystem::path(VEIL_FONT_PATH)};
    for (const auto& path : paths) if (filesystem::exists(path)) {
        font_ = LoadFontEx(path.string().c_str(), 72, nullptr, 0);
        if (font_.texture.id) { ownsFont_ = true; break; }
    }
    if (!ownsFont_) font_ = GetFontDefault();
    SetTextureFilter(font_.texture, TEXTURE_FILTER_BILINEAR);
}
Renderer::~Renderer() { if (ownsFont_) UnloadFont(font_); }

void Renderer::text(const string& value, float x, float y, float size, Color color) const {
    DrawTextEx(font_, value.c_str(), {x, y}, max(12.f, size), .35f, color);
}

float Renderer::wrap(const string& value, Rectangle area, float size, Color color) const {
    // Word wrapping measures real font widths. Each completed line is drawn
    // only while it fits, so a long journal entry cannot overwrite a button.
    istringstream words(value);
    string word, line;
    float y = area.y;
    while (words >> word) {
        const string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureTextEx(font_, candidate.c_str(), size, .35f).x > area.width) {
            if (y + size > area.y + area.height) return y;
            text(line, area.x, y, size, color);
            y += size * 1.55f;
            line = word;
        } else line = candidate;
    }
    if (!line.empty() && y + size <= area.y + area.height) text(line, area.x, y, size, color);
    return y + size * 1.55f;
}

bool Renderer::button(const string& value, Rectangle r, const UiState& ui, bool enabled, bool selected) const {
    const bool hover = enabled && CheckCollisionPointRec(ui.mouse, r);
    DrawRectangleRounded(r, .12f, 8, selected ? Gold : hover ? Raised : Panel);
    DrawRectangleRoundedLinesEx(r, .12f, 8, 1, selected ? Gold : hover ? Gold : Rule);
    text(value, r.x + 12, r.y + (r.height - 14) / 2, 14, selected ? Ink : !enabled ? fade(Muted,.45f) : hover ? Gold : Paper);
    return enabled && hover && ui.interactive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void Renderer::header(Game& game, const UiState& ui) const {
    sigil({48,43}, 15, Gold, .1f);
    text("VEILBOUND", 77, 24, 28, Paper);
    text("THE NINTH BELL", 79, 59, 12, Gold);
    text("LV " + to_string(game.progress.level), 350, 34, 18, Teal);
    text(to_string(game.progress.gold) + " CROWNS", 430, 37, 13, Gold);
    text("CASE " + to_string(min(32, game.progress.mission + 1)) + " / 32", 574, 37, 13, Muted);
    const bool navigate = game.page == Page::Map || game.page == Page::Inventory || game.page == Page::Skills || game.page == Page::Journal;
    if (button("CASEBOARD", {748,27,135,39}, ui, navigate, game.page == Page::Map)) game.page = Page::Map;
    if (button("EQUIPMENT", {897,27,135,39}, ui, navigate, game.page == Page::Inventory)) game.page = Page::Inventory;
    if (button("ABILITIES", {1046,27,135,39}, ui, navigate, game.page == Page::Skills)) game.page = Page::Skills;
    if (button("JOURNAL", {1195,27,135,39}, ui, navigate, game.page == Page::Journal)) game.page = Page::Journal;
    DrawLine(32,94,1408,94,Rule);
}

void Renderer::title(Game& game, UiState& ui) {
    // This poster-like city is drawn with geometry rather than downloaded art.
    DrawRectangleGradientV(0,0,1440,900,{18,29,40,255},{58,42,49,255});
    const Vector2 moon{1095,236};
    DrawCircleGradient(moon,180,fade(Gold,.08f),fade(Gold,0));
    DrawCircleV(moon,91,{220,192,149,255});
    DrawCircleV({1122,220},82,{24,32,43,255});
    for (int i=0;i<22;++i) {
        const float x=i*72.f-16, height=130.f+(i*71%200);
        DrawRectangleRec({x,760-height,60,height+140},i%2 ? Color{20,29,38,255}:Color{15,23,32,255});
        for (int j=0;j<4;++j) if((i+j)%3==0) DrawRectangle(static_cast<int>(x+17),static_cast<int>(775-height+j*32),8,15,fade(Gold,.25f));
    }
    // The bell tower is the visual anchor of the mystery.
    DrawRectangle(1002,401,112,337,{16,24,34,255});
    DrawTriangle({994,401},{1122,401},{1058,311},{16,24,34,255});
    DrawCircle(1058,445,25,fade(Gold,.22f));
    DrawCircleLines(1058,445,25,Gold);
    DrawLineEx({1058,445},{1058,428},2,Gold);
    DrawLineEx({1058,445},{1070,450},2,Gold);
    for(int i=0;i<30;++i) {
        const float x=fmod(i*113.4f+ui.time*19,1440.f), y=fmod(i*47.7f+ui.time*56,900.f);
        DrawLineV({x,y},{x-4,y+14},fade(Muted,.1f));
    }
    DrawRectangleGradientH(0,0,1000,900,{11,17,25,245},{11,17,25,0});
    text("AN OCCULT DETECTIVE RPG",70,110,15,Gold);
    text("VEILBOUND",62,180,76,Paper);
    text("THE NINTH BELL",68,272,34,Gold);
    DrawRectangle(72,340,76,3,Gold);
    wrap("A city that trades in memories. A sister who vanished from every record. And a bell that must never ring nine times.", {72,377,600,140},22,Paper);
    text("THREE INVESTIGATORS. ONE IMPOSSIBLE CASE.",72,541,13,Muted);
    if(button(confirmNew_ ? "CONFIRM NEW CAMPAIGN" : "BEGIN A NEW CASE",{72,594,324,52},ui,true,true)) {
        if(filesystem::exists(game.savePath)&&!confirmNew_) confirmNew_=true;
        else {game.newGame();confirmNew_=false;}
    }
    if(button("CONTINUE INVESTIGATION",{72,662,324,52},ui,filesystem::exists(game.savePath))) game.continueGame();
    text("8 CHAPTERS   /   32 CASES   /   64 ENCOUNTERS",72,779,14,Muted);
    text("Original fiction inspired by occult mystery and theatrical horror.",72,843,12,Muted);
    text("C++23 / RAYLIB 6.2",1190,843,12,Gold);
    if(confirmNew_) text("This replaces the existing campaign. Continue keeps your save.",72,731,12,Red);
}

void Renderer::map(Game& game, UiState& ui) {
    const int index=clamp(game.progress.mission,0,MissionCount-1);
    const auto& mission=missions()[static_cast<size_t>(index)];
    panel({44,119,1352,170});
    text("THE CURRENT INVESTIGATION",66,138,12,Gold);
    text(mission.name,66,169,27,Paper);
    wrap(mission.summary,{66,211,915,58},15,Muted);
    text(mission.location,1044,141,15,Teal);
    if(button(game.progress.wave ? "RESUME ENCOUNTER II" : "OPEN CASE FILE",{1044,204,324,49},ui,true,true)) game.beginMission();
    text("HARROWMERE / EIGHT DISTRICTS, ONE CONSPIRACY",44,318,14,Muted);
    for(int chapter=0;chapter<8;++chapter) {
        const float x=46+chapter*171.f;
        wrap("0"+to_string(chapter+1)+" / "+chapterNames()[chapter],{x,359,151,50},13,chapter==mission.chapter?Gold:Muted);
        for(int local=0;local<4;++local) {
            const int id=chapter*4+local;
            const float y=433+local*90.f;
            const bool done=game.progress.completed[id], current=id==index;
            if(local<3) DrawLineEx({x+14,y+34},{x+14,y+90},2,done?fade(Teal,.5f):Rule);
            DrawCircleV({x+14,y+17},12,current?Gold:done?Teal:Raised);
            text(to_string(id+1),x+7,y+10,12,current||done?Ink:Muted);
            wrap(missions()[id].name,{x+36,y+1,125,63},12,current?Paper:done?Teal:Muted);
            if(current) DrawCircleLinesV({x+14,y+17},18,fade(Gold,.5f+.3f*sin(ui.time*2)));
        }
    }
    text(to_string(game.progress.clues)+" CLUES FILED",47,830,13,Teal);
    text("INVESTIGATION TIME  "+played(game.progress.playSeconds),310,830,13,Muted);
    text("MERCY  "+to_string(game.progress.compassion),726,830,13,Gold);
    if(button("SAVE CAMPAIGN",{1135,811,260,43},ui)) game.save();
}

void Renderer::story(Game& game, UiState& ui) {
    const auto& m=missions()[clamp(game.progress.mission,0,MissionCount-1)];
    panel({82,137,1276,672});
    text("CASE FILE "+to_string(m.id+1)+" / "+chapterNames()[m.chapter],114,166,14,Gold);
    text(m.name,114,201,32,Paper);
    text(m.location,114,249,15,Teal);
    DrawLine(114,285,1325,285,Rule);
    if(game.awaitingChoice()) {
        text("HOW WILL YOU PROCEED?",114,321,17,Gold);
        for(int i=0;i<2;++i) {
            const Rectangle r{114,383.f+i*125,1210,103};
            if(button("",r,ui)) game.choose(i);
            text(i==0?"I / COMPASSION":"II / EXPEDIENCE",132,r.y+16,13,i==0?Teal:Red);
            wrap(m.choices[i],{132,r.y+43,1160,48},17,Paper);
        }
    } else if(game.awaitingClue()) {
        wrap(m.clueQuestion,{114,319,1180,98},21,Paper);
        for(int i=0;i<3;++i) {
            const Rectangle r{114,441.f+i*76,1210,62};
            if(button("",r,ui)) game.answerClue(i);
            wrap(to_string(i+1)+". "+m.clueOptions[i],{132,r.y+15,1160,40},16,Paper);
        }
        text("Examine the case carefully. A sound deduction earns extra evidence.",114,699,13,Muted);
    } else {
        wrap(game.storyText(),{114,326,1180,328},23,Paper);
        const bool ready=game.clueAnswered;
        if(button(ready?"ENTER THE FIELD":"CONTINUE READING",{985,712,339,52},ui,true,true)) {
            if(ready) game.enterBattle(); else game.advanceStory();
        }
        text(ready?"Your deductions are filed. The team is ready.":"READ AT YOUR OWN PACE / NO TIMED DIALOGUE",114,733,13,Muted);
    }
}

void Renderer::actorFigure(const Unit& unit, Vector2 p, float time) const {
    const bool boss=!unit.party&&unit.source>=0&&static_cast<size_t>(unit.source)<enemies().size()&&enemies()[unit.source].boss;
    const float radius=boss?23.f:17.f;
    const Color color=unit.party?HeroColors[unit.source%3]:boss?Gold:Red;
    DrawEllipse(static_cast<int>(p.x),static_cast<int>(p.y+16),radius+4,9,{5,10,16,130});
    if(unit.guard>0) DrawCircleLinesV(p,radius+8,Teal);
    if(unit.party) {
        DrawTriangle({p.x,p.y-10},{p.x-14,p.y+17},{p.x+14,p.y+17},ColorBrightness(color,-.35f));
        DrawRectangleRec({p.x-10,p.y-7,20,23},color);
        DrawRectangleRec({p.x-11,p.y+3,22,5},Ink);
        DrawCircleV({p.x,p.y-16},9,{210,186,154,255});
        DrawRectangleRec({p.x-11,p.y-23,22,7},Ink);
        DrawRectangleRec({p.x-15,p.y-18,30,3},color);
        if(unit.source==0) {
            // Equipment changes reach, so a melee loadout carries a blade
            // while a ranged loadout carries a visible firearm.
            if(unit.range<=1) {
                DrawLineEx({p.x+8,p.y+7},{p.x+25,p.y-14},3,Paper);
                DrawLineEx({p.x+7,p.y-1},{p.x+17,p.y+7},3,Gold);
            } else DrawRectangleRec({p.x+9,p.y-3,unit.range>=6?24.f:16.f,5},Gold);
            DrawLineEx({p.x-7,p.y+17},{p.x-9,p.y+23},4,Ink);
        } else if(unit.source==1) {
            DrawLineEx({p.x+14,p.y-10},{p.x+14,p.y+24},3,Gold);
            DrawCircleV({p.x+14,p.y-11},4,Teal);
        } else {
            DrawTriangle({p.x-9,p.y-6},{p.x-33,p.y-2+sin(time*4)*3},{p.x-10,p.y+4},Gold);
            DrawRectangleRec({p.x+11,p.y-6,10,14},Paper);
        }
    } else {
        const Ai ai=enemies()[unit.source].ai;
        if(ai==Ai::Brute||ai==Ai::Guardian) {
            DrawRectangleRounded({p.x-radius,p.y-radius,radius*2,radius*2},.25f,6,ColorBrightness(color,-.35f));
            DrawRectangleRec({p.x-radius-3,p.y-7,9,25},ai==Ai::Guardian?Gold:color);
            DrawCircleV({p.x,p.y-9},9,color);
        } else if(ai==Ai::Stalker||ai==Ai::Trickster) {
            DrawTriangle({p.x,p.y-radius-5},{p.x-radius,p.y+18},{p.x+radius,p.y+18},ColorBrightness(color,-.4f));
            DrawLineEx({p.x-5,p.y-9},{p.x+5,p.y-9},3,color);
            DrawLineEx({p.x+12,p.y+4},{p.x+24,p.y-13},3,Paper);
        } else if(ai==Ai::Summoner||ai==Ai::Leech) {
            DrawCircleGradient(p,radius+13,fade(color,.23f),fade(color,0));
            DrawCircleV(p,radius,ColorBrightness(color,-.5f));
            sigil(p,radius-2,color,time*.3f);
        } else {
            DrawRectangleRounded({p.x-12,p.y-4,24,24},.3f,6,ColorBrightness(color,-.3f));
            DrawCircleV({p.x,p.y-12},10,color);
            if(ai==Ai::Medic) {DrawRectangleRec({p.x-5,p.y-4,10,19},Paper);DrawRectangleRec({p.x-10,p.y+1,20,8},Paper);}
            else DrawRectangleRec({p.x+8,p.y-1,ai==Ai::Artillery?23.f:16.f,6},Gold);
        }
        if(boss) {
            for(int i=0;i<3;++i) DrawTriangle({p.x-14+i*14.f,p.y-29},{p.x-9+i*14.f,p.y-39},{p.x-4+i*14.f,p.y-29},Gold);
        }
    }
    bar({p.x-23,p.y+27,46,4},static_cast<float>(unit.hp)/max(1,unit.maxHp),color);
    if(unit.bleed||unit.poison||unit.stun) DrawCircleV({p.x+22,p.y-23},4,unit.stun?Gold:unit.poison?Teal:Red);
}

void Renderer::battle(Game& game, UiState& ui) {
    auto& b=game.battle;
    const auto& m=missions()[clamp(b.missionId,0,MissionCount-1)];
    const Unit* actor=b.actor();
    text(m.name,278,111,21,Paper);
    text("ENCOUNTER "+to_string(b.wave+1)+" / 2   |   ROUND "+to_string(b.round),278,145,12,Muted);
    wrap(b.objectiveText(),{1072,119,326,68},15,Gold);
    for(int i=0;i<3;++i) {
        const auto it=find_if(b.units.begin(),b.units.end(),[i](const Unit& u){return u.party&&u.source==i;});
        const Rectangle r{278+i*259.f,171,248,31};
        DrawRectangleRec(r,actor&&actor->party&&actor->source==i?Raised:Panel);
        if(it!=b.units.end()) {
            text(heroNames()[i],r.x+9,r.y+8,12,it->alive()?HeroColors[i]:Muted);
            text(to_string(it->hp)+" HP",r.x+174,r.y+8,12,it->alive()?Paper:Red);
        }
    }
    const auto reachable=b.isPartyTurn()&&ui.action==-2?b.reachable():vector<Pos>{};
    const auto isReachable=[&](Pos p){return find(reachable.begin(),reachable.end(),p)!=reachable.end();};
    for(int y=0;y<BoardHeight;++y) for(int x=0;x<BoardWidth;++x) {
        const Pos cell{x,y}; const Rectangle r=tile(cell);const auto p=center(cell);
        const auto terrain=b.board[static_cast<size_t>(y*BoardWidth+x)];
        Color floor=Floors[m.biome%8];
        if((x+y)%2) floor=ColorBrightness(floor,.06f);
        DrawRectangleRec(r,floor);
        DrawRectangleLinesEx(r,1,{25,34,40,255});
        DrawLine(static_cast<int>(r.x+8),static_cast<int>(r.y+11),static_cast<int>(r.x+26),static_cast<int>(r.y+11),fade(Muted,.08f));
        // Each district has its own ground materials, not just another color.
        // These decorations have no collision or gameplay effect.
        const int seed=(x*31+y*19+m.id*7)%17;
        if(m.biome==0) {
            DrawLineEx({r.x,r.y+32},{r.x+64,r.y+32},1,fade(Muted,.14f));
            DrawLineEx({r.x+32,r.y},{r.x+32,r.y+32},1,fade(Muted,.12f));
            DrawLineEx({r.x+16,r.y+32},{r.x+16,r.y+64},1,fade(Muted,.12f));
            if(seed<4&&terrain==Terrain::Floor) {
                DrawEllipse(static_cast<int>(p.x+5),static_cast<int>(p.y+12),22,7,{43,65,75,180});
                DrawLineEx({p.x-9,p.y+11},{p.x+13,p.y+11},1,fade(Gold,.26f));
            }
        } else if(m.biome==1) {
            DrawRectangleLinesEx({r.x+9,r.y+9,46,46},1,fade(Gold,.16f));
            if(seed<4) DrawCircleV({r.x+12,r.y+49},2,fade(Gold,.3f));
        } else if(m.biome==2) {
            for(int plank=1;plank<4;++plank)
                DrawLineEx({r.x,r.y+plank*16.f},{r.x+64,r.y+plank*16.f},2,{28,36,40,200});
            DrawCircleV({r.x+5,r.y+7},1.5f,fade(Gold,.3f));
            DrawCircleV({r.x+58,r.y+55},1.5f,fade(Gold,.3f));
        } else if(m.biome==3) {
            DrawRectangleLinesEx({r.x+5,r.y+5,54,54},1,{91,104,100,90});
            if(seed==2) DrawLineEx({r.x+12,r.y+48},{r.x+27,r.y+52},2,fade(Red,.25f));
        } else if(m.biome==4) {
            for(int plank=0;plank<3;++plank)
                DrawLineEx({r.x+plank*22.f,r.y},{r.x+plank*22.f,r.y+64},1,{83,49,51,110});
            if(seed<3) DrawCircleLinesV(p,14,fade(Gold,.18f));
        } else if(m.biome==5) {
            DrawRectangleLinesEx({r.x+12,r.y+12,40,40},1,fade(Teal,.13f));
            if(seed<4) {
                DrawRectangleRec({p.x-9,p.y-8,17,22},{112,105,85,90});
                DrawLineEx({p.x-5,p.y-3},{p.x+5,p.y-3},1,fade(Ink,.8f));
            }
        } else if(m.biome==6) {
            for(int rail=0;rail<5;++rail)
                DrawLineEx({r.x+8+rail*11.f,r.y+5},{r.x+8+rail*11.f,r.y+59},1,{19,24,29,180});
            if(seed<4) DrawCircleV({p.x+11,p.y-5},2,fade(Gold,.6f));
        } else {
            DrawCircleLinesV(p,21,{97,119,97,45});
            if(seed<6) {
                DrawLineEx({r.x+7,r.y+60},{r.x+26,r.y+37},2,{65,99,76,180});
                DrawEllipse(static_cast<int>(r.x+15),static_cast<int>(r.y+48),8,3,{83,125,89,180});
            }
        }
        if(terrain==Terrain::Wall) {
            DrawRectangleRec({r.x+4,r.y+12,56,48},{13,21,28,255});
            DrawRectangleRec({r.x+4,r.y+4,56,43},{66,69,68,255});
            DrawRectangleRec({r.x+7,r.y+7,50,5},fade(Gold,.25f));
            DrawLine(static_cast<int>(r.x+6),static_cast<int>(r.y+25),static_cast<int>(r.x+58),static_cast<int>(r.y+25),Rule);
            if(m.biome==1) {
                for(int stripe=0;stripe<5;++stripe) DrawRectangleRec({r.x+5+stripe*11.f,r.y+5,10,13},stripe%2?Gold:Red);
            } else if(m.biome==3) {
                DrawRectangleRounded({r.x+12,r.y+10,40,34},.2f,6,{153,162,149,255});
                DrawRectangleRec({r.x+16,r.y+11,32,9},Paper);
                DrawRectangleRec({r.x+29,r.y+24,6,16},Red);
                DrawRectangleRec({r.x+24,r.y+29,16,6},Red);
            } else if(m.biome==4) {
                DrawRectangleRec({r.x+9,r.y+7,46,34},{97,38,52,255});
                for(int fold=0;fold<5;++fold) DrawLineEx({r.x+12+fold*9.f,r.y+9},{r.x+12+fold*9.f,r.y+40},2,{142,65,70,255});
            } else if(m.biome==5) {
                for(int book=0;book<6;++book) DrawRectangleRec({r.x+10+book*7.f,r.y+12,5,20},book%2?Teal:Gold);
                DrawRectangleRec({r.x+7,r.y+34,50,4},Ink);
            } else if(m.biome==6) {
                DrawCircleV(p,17,{35,33,32,255});
                DrawCircleGradient(p,22,fade(Gold,.35f),fade(Gold,0));
                DrawCircleLinesV(p,17,Gold);
            } else if(m.biome==7) {
                DrawCircleV({p.x-12,p.y-8},12,{50,93,69,255});
                DrawCircleV({p.x+10,p.y-5},15,{65,109,79,255});
                DrawCircleV({p.x,p.y-16},10,{103,134,92,255});
            }
        } else if(terrain==Terrain::Cover) {
            DrawRectangleRec({r.x+12,r.y+28,40,25},{66,55,44,255});
            DrawRectangleLinesEx({r.x+12,r.y+25,40,23},2,{124,99,69,255});
            DrawLineEx({r.x+12,r.y+25},{r.x+52,r.y+48},2,{124,99,69,255});
        } else if(terrain==Terrain::Hazard) {
            DrawCircleGradient(p,30,fade(Red,.3f),fade(Red,0));
            sigil(p,19,fade(Red,.75f),ui.time*.2f);
        } else if(terrain==Terrain::Sigil) {
            sigil(p,24,Teal,ui.time*.15f);
            DrawCircleV(p,4,Paper);
        } else if(terrain==Terrain::Exit) {
            DrawRectangleLinesEx({r.x+10,r.y+10,44,44},2,Gold);
            DrawLineEx({r.x+19,p.y},{r.x+45,p.y},2,Gold);
            DrawTriangle({r.x+45,p.y},{r.x+37,p.y-6},{r.x+37,p.y+6},Gold);
        }
        if(isReachable(cell)) DrawRectangleRec({r.x+2,r.y+2,60,60},fade(Teal,.11f));
        if(actor&&b.isPartyTurn()&&ui.action>=-1&&terrain!=Terrain::Wall) {
            int range=actor->range;
            if(ui.action>=0&&ui.action<static_cast<int>(skills().size())) range=skills()[ui.action].range;
            if(distance(actor->cell,cell)<=range) DrawRectangleLinesEx({r.x+3,r.y+3,58,58},1,fade(Gold,.22f));
        }
        if(ui.hovered==cell) DrawRectangleLinesEx({r.x+1,r.y+1,62,62},2,Paper);
    }
    // Gas lamps sit outside the collision grid and light the edges of the set.
    for(int i=0;i<4;++i) {
        const Vector2 p{BoardRect.x+18+i*244.f,BoardRect.y+BoardRect.height-12};
        DrawCircleGradient(p,43,fade(Gold,.13f),fade(Gold,0));
        DrawRectangleRec({p.x-2,p.y-34,4,36},Ink);
        DrawRectangleRec({p.x-5,p.y-42,10,13},Gold);
    }
    if(m.objective==Objective::Escort) {
        // The carriage is a non-blocking objective, not an enemy. Showing it
        // explicitly lets players locate the escort-radius described above.
        const auto p=center({b.escortX,4});
        DrawCircleLinesV(p,29,fade(Gold,.6f));
        DrawRectangleRounded({p.x-22,p.y+2,44,23},.2f,6,{109,78,44,255});
        DrawRectangleLinesEx({p.x-22,p.y+2,44,23},2,Gold);
        DrawCircleV({p.x-13,p.y+27},5,Ink);
        DrawCircleV({p.x+13,p.y+27},5,Ink);
        DrawRectangleRec({p.x-3,p.y+7,6,14},Red);
        text("EVIDENCE",p.x-25,p.y-15,10,Gold);
    }
    vector<const Unit*> visible;
    for(const auto& unit:b.units) if(unit.alive()) visible.push_back(&unit);
    sort(visible.begin(),visible.end(),[](const Unit* a,const Unit* z){return a->cell.y<z->cell.y;});
    for(const auto* unit:visible) {
        if(actor&&unit->id==actor->id) {
            DrawEllipseLines(static_cast<int>(center(unit->cell).x),static_cast<int>(center(unit->cell).y+20),26,12,Gold);
            DrawTriangle({center(unit->cell).x-5,center(unit->cell).y-41},{center(unit->cell).x+5,center(unit->cell).y-41},{center(unit->cell).x,center(unit->cell).y-32},Gold);
        }
        actorFigure(*unit,center(unit->cell),ui.time);
    }
    for(const auto& effect:b.effects) {
        const auto from=center(effect.from), to=center(effect.to);
        const Color color=effect.kind==1?Teal:effect.kind==2?Gold:Red;
        DrawLineEx(from,to,3,fade(color,effect.life*2));
        DrawCircleLinesV(to,12+(1-effect.life*2)*24,fade(color,effect.life*2));
        if(effect.amount) text(to_string(abs(effect.amount)),to.x+12,to.y-43-(.5f-effect.life)*40,20,color);
    }
    panel({35,123,218,599});
    if(actor) {
        text(b.isPartyTurn()?"YOUR INVESTIGATOR":"THE VEIL MOVES",52,143,12,Gold);
        wrap(actor->name,{52,177,184,64},21,Paper);
        actorFigure(*actor,{143,287},ui.time);
        text("HEALTH",52,345,12,Muted);
        text(to_string(actor->hp)+" / "+to_string(actor->maxHp),52,369,20,Paper);
        bar({52,402,184,6},static_cast<float>(actor->hp)/max(1,actor->maxHp),Red);
        text("FOCUS",52,429,12,Muted);
        text(to_string(actor->focus)+" / "+to_string(actor->maxFocus),52,452,20,Teal);
        text("ACTION POINTS",52,503,12,Muted);
        for(int i=0;i<2;++i) DrawCircle(62+i*27,538,7,i<actor->ap?Gold:Raised);
        text("POWER "+to_string(actor->attack)+"   ARMOR "+to_string(actor->defense),52,571,12,Muted);
        if(button("M / MOVE",{52,610,184,38},ui,b.isPartyTurn(),ui.action==-2)) ui.action=-2;
        if(button("A / ATTACK",{52,662,184,38},ui,b.isPartyTurn(),ui.action==-1)) ui.action=-1;
    }
    panel({1072,210,326,512});
    text("FIELD NOTES",1091,230,13,Gold);
    const Unit* hovered=nullptr;
    for(const auto& u:b.units) if(u.alive()&&u.cell==ui.hovered) hovered=&u;
    if(hovered) {
        wrap(hovered->name,{1091,264,286,58},20,hovered->party?Teal:Red);
        text("HP "+to_string(hovered->hp)+"  /  POWER "+to_string(hovered->attack),1091,327,13,Paper);
        if(!hovered->party) {
            text(aiName(enemies()[hovered->source].ai),1091,359,12,Gold);
            wrap(enemies()[hovered->source].description,{1091,390,286,150},15,Muted);
        } else wrap("An investigator under your command. Keep the team together: focus and support abilities can change a difficult encounter.",{1091,367,286,160},15,Muted);
    } else {
        wrap(b.isPartyTurn()?"Select Move, Attack, or an ability. Click a highlighted tile or a target on the board. Each investigator has two actions.":"Your opponents are acting. Watch their roles and ranges; cover protects, while crimson sigils harm.",{1091,269,286,210},16,Muted);
        text("M / MOVE    A / ATTACK",1091,516,12,Paper);
        text("1-6 / ABILITY    E / END TURN",1091,546,12,Paper);
        text("ESC / PAUSE",1091,576,12,Paper);
    }
    if(button("E / END TURN",{1091,646,286,51},ui,b.isPartyTurn(),true)) b.endTurn();
    text("ABILITIES / SELECT, THEN CLICK A TARGET",278,740,12,Muted);
    if(actor&&actor->party) {
        for(int local=0;local<6;++local) {
            const int id=actor->source*6+local; if(id>=static_cast<int>(skills().size())) continue;
            const auto& skill=skills()[id]; const bool learned=game.progress.learned[id];
            const bool ready=learned&&actor->focus>=skill.focusCost&&actor->cooldowns[id]<=0;
            const Rectangle r{278+local*130.f,770,120,68};
            if(button("",r,ui,ready&&b.isPartyTurn(),ui.action==id)) ui.action=id;
            wrap(to_string(local+1)+" / "+skill.name,{r.x+8,r.y+8,105,33},12,ui.action==id?Ink:ready?Paper:Muted);
            text(!learned?"NOT LEARNED":actor->cooldowns[id]>0?"CD "+to_string(actor->cooldowns[id]):to_string(skill.focusCost)+" FOCUS",r.x+8,r.y+48,12,ui.action==id?Ink:learned?Teal:Muted);
            if(CheckCollisionPointRec(ui.mouse,r)) wrap(skill.description,{278,846,768,34},12,Gold);
        }
    }
    const size_t first=b.log.size()>3?b.log.size()-3:0;
    for(size_t i=first;i<b.log.size();++i) wrap(b.log[i],{1075,750+(i-first)*36.f,320,34},12,i+1==b.log.size()?Paper:Muted);
}

void Renderer::inventory(Game& game, UiState& ui) {
    text("THE QUARTERMASTER",44,119,26,Paper);
    text("Buy once. Equip freely. Every investigator has four equipment slots.",44,160,14,Muted);
    for(int hero=0;hero<3;++hero)
        if(button(heroNames()[hero],{44+hero*304.f,201,290,42},ui,true,ui.selectedHero==hero)) ui.selectedHero=hero;
    const int pages=max(1,(static_cast<int>(items().size())+11)/12);
    ui.inventoryPage=clamp(ui.inventoryPage,0,pages-1);
    for(int i=0;i<12;++i) {
        const int id=ui.inventoryPage*12+i;if(id>=static_cast<int>(items().size())) break;
        const auto& item=items()[id];const Rectangle r{44+(i%3)*304.f,267+(i/3)*123.f,290,110};
        const bool owned=id<static_cast<int>(game.progress.owned.size())&&game.progress.owned[id];
        if(button("",r,ui,true,ui.selectedItem==id)) ui.selectedItem=id;
        text(slotName(item.slot),r.x+12,r.y+12,12,ui.selectedItem==id?Ink:Gold);
        wrap(item.name,{r.x+12,r.y+38,265,44},16,ui.selectedItem==id?Ink:Paper);
        text(owned?"OWNED":to_string(item.price)+" CROWNS",r.x+12,r.y+86,12,ui.selectedItem==id?Ink:owned?Teal:Muted);
    }
    if(button("PREVIOUS",{44,789,180,40},ui,ui.inventoryPage>0)) --ui.inventoryPage;
    text("PAGE "+to_string(ui.inventoryPage+1)+" / "+to_string(pages),278,803,13,Muted);
    if(button("NEXT",{714,789,228,40},ui,ui.inventoryPage+1<pages)) ++ui.inventoryPage;
    panel({968,201,430,628});
    const auto& item=items()[clamp(ui.selectedItem,0,static_cast<int>(items().size())-1)];
    wrap(item.name,{992,227,380,80},25,Gold);
    wrap(item.description,{992,323,380,150},16,Paper);
    text("POWER +"+to_string(item.attack)+"    ARMOR +"+to_string(item.defense),992,487,14,Teal);
    text("HEALTH +"+to_string(item.hp)+"    FOCUS +"+to_string(item.focus),992,520,14,Teal);
    text("MOVE +"+to_string(item.move),992,553,14,Teal);
    const bool owned=item.id<static_cast<int>(game.progress.owned.size())&&game.progress.owned[item.id];
    const bool unlocked=item.unlockChapter<=game.progress.mission/4;
    if(button(owned?"OWNED":"BUY / "+to_string(item.price)+" CROWNS",{992,604,380,45},ui,!owned&&unlocked&&game.progress.gold>=item.price)) game.buy(item.id);
    if(button("EQUIP ON "+heroNames()[ui.selectedHero],{992,665,380,45},ui,owned,true)) game.equip(ui.selectedHero,item.id);
    const int equipped=game.progress.equipment[ui.selectedHero][static_cast<int>(item.slot)];
    wrap("CURRENT "+slotName(item.slot)+": "+(equipped>=0?items()[equipped].name:"None"),{992,737,380,62},13,Muted);
    if(!unlocked) text("AVAILABLE IN CHAPTER "+to_string(item.unlockChapter+1),992,577,12,Red);
}

void Renderer::skillTree(Game& game, UiState& ui) {
    text("THE ARTS BEHIND THE VEIL",44,119,26,Paper);
    text(to_string(game.progress.skillPoints)+" ABILITY POINTS / Unlock abilities as your investigation level rises.",44,164,14,Gold);
    for(int hero=0;hero<3;++hero)
        if(button(heroNames()[hero],{44+hero*451.f,201,431,42},ui,true,ui.selectedHero==hero)) ui.selectedHero=hero;
    for(int local=0;local<6;++local) {
        const int id=ui.selectedHero*6+local; const auto& skill=skills()[id];
        const Rectangle r{44+(local%3)*451.f,277+(local/3)*263.f,431,239};
        panel(r);
        text("0"+to_string(local+1)+" / LEVEL "+to_string(skill.unlockLevel),r.x+20,r.y+19,12,Gold);
        text(skill.name,r.x+20,r.y+51,21,Paper);
        wrap(skill.description,{r.x+20,r.y+91,390,78},15,Muted);
        text("FOCUS "+to_string(skill.focusCost)+" / RANGE "+to_string(skill.range)+" / COOLDOWN "+to_string(skill.cooldown),r.x+20,r.y+174,12,Teal);
        const bool learned=game.progress.learned[id];
        if(button(learned?"LEARNED":"LEARN / 1 POINT",{r.x+20,r.y+197,391,30},ui,!learned&&game.progress.level>=skill.unlockLevel&&game.progress.skillPoints>0,learned)) game.learn(id);
    }
}

void Renderer::journal(Game& game, UiState& ui) {
    text("THE INVESTIGATOR'S JOURNAL",44,119,26,Paper);
    text(to_string(game.progress.clues)+" DEDUCTIONS / "+played(game.progress.playSeconds)+" INVESTIGATED / MERCY "+to_string(game.progress.compassion),44,165,14,Gold);
    const int pageCount=max(1,(static_cast<int>(game.progress.journal.size())+5)/6);
    journalPage_=clamp(journalPage_,0,pageCount-1);
    if(game.progress.journal.empty()) wrap("Every case leaves a paper trail. Read the briefing, decide how to proceed, and solve its deduction to build your record of the city.",{60,252,1090,150},23,Muted);
    for(int local=0;local<6;++local) {
        const int index=journalPage_*6+local;if(index>=static_cast<int>(game.progress.journal.size())) break;
        const Rectangle r{44+(local%2)*683.f,224+(local/2)*185.f,662,166};
        panel(r);
        text("RECORD "+to_string(index+1),r.x+20,r.y+17,12,Gold);
        wrap(game.progress.journal[index],{r.x+20,r.y+46,622,103},15,Paper);
    }
    if(button("EARLIER RECORDS",{44,807,250,42},ui,journalPage_>0)) --journalPage_;
    text("PAGE "+to_string(journalPage_+1)+" / "+to_string(pageCount),626,821,14,Muted);
    if(button("LATER RECORDS",{1146,807,250,42},ui,journalPage_+1<pageCount)) ++journalPage_;
}

void Renderer::reward(Game& game, UiState& ui) {
    const bool lost=game.battle.outcome==Outcome::Defeat;
    if(rewardSeen_!=game.rewardText) {rewardSeen_=game.rewardText;rewardPage_=0;}
    // A case ending may contain several paragraphs. Paginate its actual text
    // rather than clipping the final revelation off the bottom of the panel.
    vector<string> lines;
    istringstream words(game.rewardText);
    string word,line;
    while(words>>word) {
        const string candidate=line.empty()?word:line+" "+word;
        if(!line.empty()&&MeasureTextEx(font_,candidate.c_str(),19,.35f).x>1128) {lines.push_back(line);line=word;}
        else line=candidate;
    }
    if(!line.empty()) lines.push_back(line);
    const int pages=max(1,(static_cast<int>(lines.size())+9)/10);
    rewardPage_=clamp(rewardPage_,0,pages-1);
    string pageText;
    for(int i=rewardPage_*10;i<min(static_cast<int>(lines.size()),rewardPage_*10+10);++i) pageText+=lines[i]+" ";
    panel({112,148,1216,660});
    text(lost?"THE VEIL CLAIMS THIS NIGHT":"ANOTHER THREAD, UNCOVERED",152,190,31,lost?Red:Gold);
    text(lost?"Your case is not over. Regroup and try a different approach.":"The city gives up its secrets reluctantly. You are learning to listen.",152,246,17,Muted);
    DrawLine(152,290,1287,290,Rule);
    wrap(pageText,{152,326,1128,310},19,Paper);
    if(pages>1) {
        if(button("PREVIOUS PAGE",{152,649,240,37},ui,rewardPage_>0)) --rewardPage_;
        text("PAGE "+to_string(rewardPage_+1)+" / "+to_string(pages),635,662,13,Muted);
        if(button("READ NEXT PAGE",{1047,649,240,37},ui,rewardPage_+1<pages)) ++rewardPage_;
    }
    if(lost) {
        if(button("REGROUP / RETRY",{912,711,375,52},ui,true,true)) game.retry();
        if(button("RETURN TO CASEBOARD",{152,711,330,52},ui)) game.page=Page::Map;
    } else if(button("CONTINUE INVESTIGATION",{912,711,375,52},ui,rewardPage_+1==pages,true)) game.finishReward();
    text("Autosaved at encounter checkpoints. Equipment can be changed between cases.",152,783,12,Muted);
}

void Renderer::ending(Game& game, UiState& ui) {
    sigil({1115,401},184,fade(Gold,.23f),ui.time*.025f);
    text("THE LAST PAGE OF THE CASE",88,166,15,Gold);
    text("THE NINTH BELL",84,222,57,Paper);
    text("FALLS SILENT.",84,290,57,Paper);
    const bool mercy=game.progress.compassion>=0;
    wrap(mercy?"Harrowmere wakes with its scars, and with the right to remember them. You could not save every lost hour. But you refused to let a city become an alibi. Somewhere in the dawn, Mira speaks her own name again.":"Harrowmere survives beneath a cleaner, colder sky. The Bureau seals the evidence you chose to bury. The bells keep perfect time now. Only you and your companions remember what that precision cost.",{88,394,820,220},23,Muted);
    text("32 CASES CLOSED / "+to_string(game.progress.clues)+" DEDUCTIONS / "+played(game.progress.playSeconds),88,650,16,Gold);
    text("Thank you for walking into the fog.",88,701,22,Paper);
    if(button("READ THE JOURNAL",{88,763,320,49},ui,true,true)) game.page=Page::Journal;
    if(button("TITLE SCREEN",{426,763,280,49},ui)) game.page=Page::Title;
}

void Renderer::draw(Game& game, UiState& ui) {
    // A pause overlay is modal: a click behind it must not spend actions or
    // activate one of the ordinary menu buttons before Resume is evaluated.
    const bool interactive=ui.interactive;
    if(ui.paused) ui.interactive=false;
    ClearBackground(Ink);
    if(game.page==Page::Title) title(game,ui);
    else {
        header(game,ui);
        switch(game.page) {
            case Page::Map: map(game,ui);break;
            case Page::Story: story(game,ui);break;
            case Page::Battle: battle(game,ui);break;
            case Page::Inventory: inventory(game,ui);break;
            case Page::Skills: skillTree(game,ui);break;
            case Page::Journal: journal(game,ui);break;
            case Page::Reward: reward(game,ui);break;
            case Page::Ending: ending(game,ui);break;
            case Page::Title:break;
        }
    }
    if(!game.message.empty()) wrap(game.message,{35,875,1355,23},12,Gold);
    ui.interactive=interactive;
    if(ui.paused) {
        DrawRectangle(0,0,1440,900,{5,9,16,215});
        panel({448,269,544,345});
        text("THE FOG CAN WAIT.",489,310,31,Paper);
        wrap("Combat is paused. Your campaign saves between encounters; closing a battle returns you to its checkpoint.",{489,366,458,110},17,Muted);
        if(button("ESC / RESUME",{489,516,460,51},ui,true,true)) ui.paused=false;
    }
}
} // namespace veil
