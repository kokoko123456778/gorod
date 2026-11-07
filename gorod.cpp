// =====================================================================
//  «МОЙ ГОРОД» — изометрический градостроитель
//  Один файл, C++14 + SDL2. Вся графика, шрифт и звук создаются кодом.
//  Сборка (ПК):  g++ -std=c++14 -O2 gorod.cpp -o gorod `sdl2-config --cflags --libs`
//  Android: файл подключается как единственный исходник в проект SDL2 (NDK).
// =====================================================================
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int64_t i64;

// ---------------------------------------------------------------------
//  Утилиты
// ---------------------------------------------------------------------
static inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static u32 hash32(u32 x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static u32 hash2(int a, int b) { return hash32((u32)a * 73856093U ^ (u32)b * 19349663U ^ 0x9e3779b9U); }
struct Rng {
    u32 s;
    explicit Rng(u32 seed = 1) : s(seed ? seed : 1) {}
    u32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }
    int i(int n) { return n <= 0 ? 0 : (int)(next() % (u32)n); }
};
static Rng grng(20251106);

#define ARGB(a, r, g, b) (((u32)(a) << 24) | ((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))
#define RGB(r, g, b) ARGB(255, r, g, b)
static inline u32 cshade(u32 c, float f) {
    int r = clampi((int)(((c >> 16) & 255) * f), 0, 255), g = clampi((int)(((c >> 8) & 255) * f), 0, 255), b = clampi((int)((c & 255) * f), 0, 255);
    return (c & 0xFF000000u) | (r << 16) | (g << 8) | b;
}
static inline u32 cmix(u32 a, u32 b, float t) {
    int r = (int)lerpf((float)((a >> 16) & 255), (float)((b >> 16) & 255), t);
    int g = (int)lerpf((float)((a >> 8) & 255), (float)((b >> 8) & 255), t);
    int bl = (int)lerpf((float)(a & 255), (float)(b & 255), t);
    int al = (int)lerpf((float)(a >> 24), (float)(b >> 24), t);
    return ARGB(al, r, g, bl);
}
static inline u32 calpha(u32 c, int a) { return (c & 0x00FFFFFFu) | ((u32)clampi(a, 0, 255) << 24); }

// UTF-8 -> кодовые точки
static int utf8next(const char*& p) {
    unsigned char c = (unsigned char)*p;
    if (!c) return 0;
    if (c < 0x80) { p++; return c; }
    if ((c & 0xE0) == 0xC0 && p[1]) { int cp = ((c & 0x1F) << 6) | (p[1] & 0x3F); p += 2; return cp; }
    if ((c & 0xF0) == 0xE0 && p[1] && p[2]) { int cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; return cp; }
    if ((c & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) { int cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; return cp; }
    p++; return '?';
}
static std::string fmtInt(i64 v) {
    bool neg = v < 0; if (neg) v = -v;
    char b[32]; snprintf(b, sizeof b, "%lld", (long long)v);
    std::string s(b), r; int n = (int)s.size();
    for (int i = 0; i < n; i++) { r += s[i]; if ((n - i - 1) % 3 == 0 && i != n - 1) r += ' '; }
    return neg ? "-" + r : r;
}
static std::string strf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#include <cstdarg>
static std::string strf(const char* fmt, ...) {
    char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); return b;
}

// ---------------------------------------------------------------------
//  Пиксельный шрифт 5x7 (кириллица, цифры, знаки)
// ---------------------------------------------------------------------
struct GlyphDef { int cp; const char* rows[7]; };
static const GlyphDef GLYPHS[] = {
    {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {'3', {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
    {'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
    {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {'6', {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
    {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {'9', {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
    {0x410, {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}}, // А
    {0x411, {"#####", "#....", "#....", "####.", "#...#", "#...#", "####."}}, // Б
    {0x412, {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}}, // В
    {0x413, {"#####", "#....", "#....", "#....", "#....", "#....", "#...."}}, // Г
    {0x414, {"..##.", ".#.#.", ".#.#.", ".#.#.", ".#.#.", "#####", "#...#"}}, // Д
    {0x415, {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}}, // Е
    {0x401, {".#.#.", "#####", "#....", "####.", "#....", "#....", "#####"}}, // Ё
    {0x416, {"#.#.#", "#.#.#", ".###.", "..#..", ".###.", "#.#.#", "#.#.#"}}, // Ж
    {0x417, {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."}}, // З
    {0x418, {"#...#", "#...#", "#..##", "#.#.#", "##..#", "#...#", "#...#"}}, // И
    {0x419, {".#.#.", "..#..", "#...#", "#..##", "#.#.#", "##..#", "#...#"}}, // Й
    {0x41A, {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}}, // К
    {0x41B, {"..###", ".#..#", ".#..#", ".#..#", ".#..#", ".#..#", "#...#"}}, // Л
    {0x41C, {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}}, // М
    {0x41D, {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}}, // Н
    {0x41E, {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}}, // О
    {0x41F, {"#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#...#"}}, // П
    {0x420, {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}}, // Р
    {0x421, {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}}, // С
    {0x422, {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}}, // Т
    {0x423, {"#...#", "#...#", "#...#", ".####", "....#", "#...#", ".###."}}, // У
    {0x424, {"..#..", ".###.", "#.#.#", "#.#.#", "#.#.#", ".###.", "..#.."}}, // Ф
    {0x425, {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}}, // Х
    {0x426, {"#..#.", "#..#.", "#..#.", "#..#.", "#..#.", "#####", "....#"}}, // Ц
    {0x427, {"#...#", "#...#", "#...#", ".####", "....#", "....#", "....#"}}, // Ч
    {0x428, {"#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####"}}, // Ш
    {0x429, {"#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....#"}}, // Щ
    {0x42A, {"##...", ".#...", ".#...", ".###.", ".#..#", ".#..#", ".###."}}, // Ъ
    {0x42B, {"#...#", "#...#", "#...#", "###.#", "#.#.#", "#.#.#", "###.#"}}, // Ы
    {0x42C, {"#....", "#....", "#....", "####.", "#...#", "#...#", "####."}}, // Ь
    {0x42D, {".###.", "#...#", "....#", ".####", "....#", "#...#", ".###."}}, // Э
    {0x42E, {"#..#.", "#.#.#", "#.#.#", "###.#", "#.#.#", "#.#.#", "#..#."}}, // Ю
    {0x42F, {".####", "#...#", "#...#", ".####", "..#.#", ".#..#", "#...#"}}, // Я
    {' ', {".....", ".....", ".....", ".....", ".....", ".....", "....."}},
    {'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
    {',', {".....", ".....", ".....", ".....", ".##..", "..#..", ".#..."}},
    {':', {".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."}},
    {';', {".....", ".##..", ".##..", ".....", ".##..", "..#..", ".#..."}},
    {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
    {'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
    {'-', {".....", ".....", ".....", ".###.", ".....", ".....", "....."}},
    {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {'%', {"##..#", "##.#.", "...#.", "..#..", ".#...", ".#.##", "#..##"}},
    {'/', {"....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...."}},
    {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
    {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
    {0xAB, {".....", "..#.#", ".#.#.", "#.#..", ".#.#.", "..#.#", "....."}}, // «
    {0xBB, {".....", "#.#..", ".#.#.", "..#.#", ".#.#.", "#.#..", "....."}}, // »
    {'"', {".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."}},
    {'\'', {"..#..", "..#..", ".....", ".....", ".....", ".....", "....."}},
    {'*', {".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."}},
    {'=', {".....", ".....", "#####", ".....", "#####", ".....", "....."}},
    {'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
    {'>', {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."}},
    {'#', {".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#."}},
    {'_', {".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
    {0x20BD, {"####.", "#...#", "#...#", "####.", "#....", "###..", "#...."}}, // ₽
    {0x221E, {".....", ".....", ".#.#.", "#.#.#", ".#.#.", ".....", "....."}}, // ∞
    {0x2191, {"..#..", ".###.", "#.#.#", "..#..", "..#..", "..#..", "..#.."}}, // ↑
    {0x2193, {"..#..", "..#..", "..#..", "..#..", "#.#.#", ".###.", "..#.."}}, // ↓
    {0xB7, {".....", ".....", ".....", "..#..", ".....", ".....", "....."}}, // ·
    {0x2014, {".....", ".....", ".....", "#####", ".....", ".....", "....."}}, // —
    {'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
    {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {'D', {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}},
    {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."}},
    {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'N', {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}},
    {'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'V', {"#...#", "#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."}},
    {'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"}},
};
static const int NGLYPHS = (int)(sizeof(GLYPHS) / sizeof(GLYPHS[0]));
static int glyphIndex(int cp) {
    if (cp >= 0x430 && cp <= 0x44F) cp -= 0x20;          // строчные -> заглавные
    if (cp == 0x451) cp = 0x401;                          // ё -> Ё
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    switch (cp) {                                         // латиница -> похожая кириллица
        case 'A': cp = 0x410; break; case 'B': cp = 0x412; break; case 'C': cp = 0x421; break;
        case 'E': cp = 0x415; break; case 'H': cp = 0x41D; break; case 'K': cp = 0x41A; break;
        case 'M': cp = 0x41C; break; case 'O': cp = 0x41E; break; case 'P': cp = 0x420; break;
        case 'T': cp = 0x422; break; case 'Y': cp = 0x423; break; case 'F': cp = 0x424; break;
        default: break;
    }
    for (int i = 0; i < NGLYPHS; i++) if (GLYPHS[i].cp == cp) return i;
    for (int i = 0; i < NGLYPHS; i++) if (GLYPHS[i].cp == '?') return i;
    return 0;
}

// ---------------------------------------------------------------------
//  Иконки интерфейса 12x12 (палитра символами)
// ---------------------------------------------------------------------
static u32 iconPal(char c) {
    switch (c) {
        case 'k': return RGB(28, 30, 40);   case 'w': return RGB(245, 245, 240); case 'y': return RGB(250, 210, 50);
        case 'o': return RGB(225, 140, 30); case 'r': return RGB(210, 60, 50);   case 'g': return RGB(90, 180, 70);
        case 'G': return RGB(40, 110, 50);  case 'b': return RGB(70, 130, 220);  case 'B': return RGB(30, 60, 130);
        case 'n': return RGB(130, 85, 45);  case 's': return RGB(170, 175, 180); case 'S': return RGB(90, 95, 105);
        case 'c': return RGB(150, 215, 240); case 'p': return RGB(240, 190, 160);
        default: return 0;
    }
}
enum { IC_INSPECT, IC_BULL, IC_FLAT, IC_UNDO, IC_PAUSE, IC_PLAY, IC_FF, IC_FFF, IC_LAYERS, IC_MENU, IC_POWER, IC_WATER,
       IC_SERVICE, IC_ZONE, IC_ROAD, IC_COIN, IC_PEOPLE, IC_SMILE, IC_PIPE, IC_CLOSE, IC_PLINE, IC_TROPHY, IC_COUNT };
static const char* ICONS[IC_COUNT][12] = {
    {"...kkkk.....", "..kwccck....", ".kwccccck...", ".kcccccck...", ".kcccccck...", ".kcccccck...", "..kcccck....", "...kkkkn....", ".......nn...", "........nn..", ".........nn.", "..........n."},
    {"............", "....kkkk....", "....kbbk....", "..yyyyyyyy..", "..yyyyyyyy.s", "..yyyyyyyy.s", "..kkkkkkkk.s", ".kSSSSSSSSks", ".kSkSkSkSkss", ".kSSSSSSSSk.", "..kkkkkkkk..", "............"},
    {"..........kk", ".........knk", "........knk.", ".......knk..", "......knk...", ".....knk....", "..sssnk.....", ".sssss......", "sssss.......", "ssss........", "gggggggggggg", "nnnnnnnnnnnn"},
    {"............", "....k.......", "...kk.......", "..kwkkkkk...", ".kwwwwwwwk..", "..kwkkkkwwk.", "...kk...kwk.", "....k...kwk.", "........kwk.", "......kkwwk.", "......kwwk..", "......kkk..."},
    {"............", "..kkk..kkk..", "..kwk..kwk..", "..kwk..kwk..", "..kwk..kwk..", "..kwk..kwk..", "..kwk..kwk..", "..kwk..kwk..", "..kwk..kwk..", "..kwk..kwk..", "..kkk..kkk..", "............"},
    {"..k.........", "..kk........", "..kwk.......", "..kwwk......", "..kwwwk.....", "..kwwwwk....", "..kwwwwk....", "..kwwwk.....", "..kwwk......", "..kwk.......", "..kk........", "..k........."},
    {"............", "k.....k.....", "kk....kk....", "kwk...kwk...", "kwwk..kwwk..", "kwwwk.kwwwk.", "kwwwk.kwwwk.", "kwwk..kwwk..", "kwk...kwk...", "kk....kk....", "k.....k.....", "............"},
    {"............", "k...k...k...", "kk..kk..kk..", "kwk.kwk.kwk.", "kwwkkwwkkwwk", "kwwkkwwkkwwk", "kwwkkwwkkwwk", "kwk.kwk.kwk.", "kk..kk..kk..", "k...k...k...", "............", "............"},
    {".....kk.....", "...kkcckk...", ".kkcccccckk.", "kcccccccccck", ".kkcccccckk.", "...kkcckk...", ".kk.kkkk.kk.", "kbbkk..kkbbk", ".kkbbkkbbkk.", "...kkbbkk...", ".....kk.....", "............"},
    {"............", ".kkkkkkkkkk.", ".kwwwwwwwwk.", ".kkkkkkkkkk.", "............", ".kkkkkkkkkk.", ".kwwwwwwwwk.", ".kkkkkkkkkk.", "............", ".kkkkkkkkkk.", ".kwwwwwwwwk.", ".kkkkkkkkkk."},
    {"......kkkk..", ".....kyyk...", "....kyyk....", "...kyyk.....", "..kyyyyyyk..", "..kkkkyyk...", ".....kyk....", "....kyk.....", "...kyk......", "..kyk.......", "..kk........", "............"},
    {".....kk.....", ".....kk.....", "....kbbk....", "....kbbk....", "...kbbbbk...", "..kbwbbbbk..", "..kbwbbbbk..", ".kbwbbbbbbk.", ".kbbbbbbbbk.", ".kbbbbbbbbk.", "..kbbbbbbk..", "...kkkkkk..."},
    {".kkkkkkkkkk.", ".kbbbbbbbbk.", ".kbbbwwbbbk.", ".kbbbwwbbbk.", ".kbwwwwwwbk.", ".kbwwwwwwbk.", ".kbbbwwbbbk.", ".kbbbwwbbbk.", "..kbbbbbbk..", "...kbbbbk...", "....kbbk....", ".....kk....."},
    {".....kk.....", "....krrk....", "...krrrrk...", "..krrrrrrk..", ".krrrrrrrrk.", "kkkkkkkkkkkk", ".kwwwwwwwwk.", ".kwbbwwbbwk.", ".kwbbwwbbwk.", ".kwwwkkwwwk.", ".kwwwkkwwwk.", ".kkkkkkkkkk."},
    {"..kSSSSSSk..", "..kSSyySSk..", "..kSSyySSk..", "..kSSSSSSk..", "..kSSSSSSk..", "..kSSyySSk..", "..kSSyySSk..", "..kSSSSSSk..", "..kSSSSSSk..", "..kSSyySSk..", "..kSSyySSk..", "..kSSSSSSk.."},
    {"...kkkkkk...", "..kyyyyyyk..", ".kyyooooyyk.", "kyyoyyyyoyyk", "kyyoyyyyyyyk", "kyyyooooyyyk", "kyyyyyyyoyyk", "kyyoyyyyoyyk", ".kyyooooyyk.", "..kyyyyyyk..", "...kkkkkk...", "............"},
    {"...kkk......", "..kpppk.....", "..kpppk.kkk.", "...kkk.kpppk", "..kbbbk.kpppk", ".kbbbbbkkkk.", ".kbbbbbkgggk", ".kbbbbbkgggk", ".kbbbbbkgggk", ".kkkkkkkgggk", "........kkk.", "............"},
    {"...kkkkkk...", "..kyyyyyyk..", ".kyyyyyyyyk.", "kyykyyyykyyk", "kyykyyyykyyk", "kyyyyyyyyyyk", "kykyyyyyykyk", "kyykyyyykyyk", ".kyykkkkyyk.", "..kyyyyyyk..", "...kkkkkk...", "............"},
    {"............", "............", "kkkk....kkkk", "kbbkkkkkkbbk", "kbbccccccbbk", "kbbbbbbbbbbk", "kbbbbbbbbbbk", "kbbkkkkkkbbk", "kkkk....kkkk", "............", "............", "............"},
    {"............", ".kk......kk.", ".kwk....kwk.", "..kwk..kwk..", "...kwkkwk...", "....kwwk....", "....kwwk....", "...kwkkwk...", "..kwk..kwk..", ".kwk....kwk.", ".kk......kk.", "............"},
    {".kkkkkkkkkk.", ".knnnnnnnnk.", ".kkkknnkkkk.", "....knnk....", "....knnk....", "....knnk....", "....knnk....", "....knnk....", "....knnk....", "....knnk....", "...knnnnk...", "...kkkkkk..."},
    {"kkkkkkkkkkkk", "kyyyyyyyyyyk", "kyyyyyyyyyyk", ".kyyyyyyyyk.", ".kyyyyyyyyk.", "..kyyyyyyk..", "...kyyyyk...", "....kyyk....", ".....kk.....", "....kyyk....", "..kyyyyyyk..", "..kkkkkkkk.."},
};

// ---------------------------------------------------------------------
//  Атлас спрайтов: рисуем кодом в память, потом загружаем в текстуры.
//  Каждый спрайт имеет «основной» слой и слой ночных огней.
// ---------------------------------------------------------------------
struct Spr { int pg, x, y, w, h, ax, ay; };
struct Page { std::vector<u32> m, l; SDL_Texture* tm = nullptr; SDL_Texture* tl = nullptr; bool hasL = false; };
enum { EM_SMOKE = 1, EM_STEAM, EM_BEACON, EM_ROTOR, EM_FLAG };
struct Emit { int type, x, y; };
static std::vector<Spr> S;
static std::vector<std::vector<Emit>> SMETA;
static std::vector<Page> pages;
static int PGS = 2048;
static int shX = 0, shY = 0, shH = 0;
static u32 *cm = nullptr, *cl = nullptr;
static int cW = 0, cH = 0, cAX = 0, cAY = 0, curN = 1, curId = 0;
static bool curHasL = false;

static void selSpr(int id) {
    Spr& s = S[id]; Page& p = pages[s.pg];
    cm = &p.m[s.y * PGS + s.x]; cl = &p.l[s.y * PGS + s.x];
    cW = s.w; cH = s.h; cAX = s.ax; cAY = s.ay; curId = id;
}
static int newSpr(int w, int h, int ax, int ay, int n = 1) {
    int pw = w + 2, ph = h + 2;
    if (shX + pw > PGS) { shX = 0; shY += shH; shH = 0; }
    if (pages.empty() || shY + ph > PGS) {
        pages.push_back(Page());
        pages.back().m.assign((size_t)PGS * PGS, 0u);
        pages.back().l.assign((size_t)PGS * PGS, 0u);
        shX = shY = shH = 0;
    }
    Spr s; s.pg = (int)pages.size() - 1; s.x = shX + 1; s.y = shY + 1; s.w = w; s.h = h; s.ax = ax; s.ay = ay;
    shX += pw; shH = std::max(shH, ph);
    S.push_back(s); SMETA.push_back(std::vector<Emit>());
    int id = (int)S.size() - 1; selSpr(id); curN = n;
    return id;
}
static void addEmit(int type, int x, int y) { SMETA[curId].push_back(Emit{type, x - cAX, y - cAY}); }

static inline void pset(int x, int y, u32 c) {
    if (x < 0 || y < 0 || x >= cW || y >= cH) return;
    u32 a = c >> 24; if (!a) return;
    u32& d = cm[y * PGS + x];
    if (a == 255 || !(d >> 24)) { d = c; return; }
    d = cmix(d, c | 0xFF000000u, a / 255.0f) | 0xFF000000u;
}
static inline u32 pget(int x, int y) { if (x < 0 || y < 0 || x >= cW || y >= cH) return 0; return cm[y * PGS + x]; }
static inline void lset(int x, int y, u32 c) {
    if (x < 0 || y < 0 || x >= cW || y >= cH) return;
    cl[y * PGS + x] = c; pages[S[curId].pg].hasL = true;
}
static inline void lclear(int x, int y) { if (x < 0 || y < 0 || x >= cW || y >= cH) return; cl[y * PGS + x] = 0; }
static void prect(int x, int y, int w, int h, u32 c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) pset(x + i, y + j, c); }
static void pline(int x0, int y0, int x1, int y1, u32 c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) { pset(x0, y0, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; } }
}
static void fillEllipse(float cx, float cy, float rx, float ry, u32 c) {
    for (int y = (int)floorf(cy - ry); y <= (int)ceilf(cy + ry); y++)
        for (int x = (int)floorf(cx - rx); x <= (int)ceilf(cx + rx); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) pset(x, y, c);
        }
}
// выпуклый многоугольник, правило центров пикселей
static void fillPoly(const float* xs, const float* ys, int n, u32 c) {
    float ymin = 1e9f, ymax = -1e9f;
    for (int i = 0; i < n; i++) { ymin = std::min(ymin, ys[i]); ymax = std::max(ymax, ys[i]); }
    for (int y = (int)floorf(ymin); y <= (int)ceilf(ymax); y++) {
        float yc = y + 0.5f, xa = 1e9f, xb = -1e9f;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n; float y0 = ys[i], y1 = ys[j];
            if ((yc >= y0 && yc < y1) || (yc >= y1 && yc < y0)) {
                float x = xs[i] + (yc - y0) / (y1 - y0) * (xs[j] - xs[i]);
                xa = std::min(xa, x); xb = std::max(xb, x);
            }
        }
        if (xa > xb) continue;
        for (int x = (int)ceilf(xa - 0.5f); x <= (int)floorf(xb - 0.5f); x++) pset(x, y, c);
    }
}
// изометрия внутри спрайта: u,v — в клетках (0..curN), z — в пикселях
static inline float isoX(float u, float v) { return cAX + (u - v) * 32.0f; }
static inline float isoY(float u, float v, float z) { return cAY - ((curN - u) + (curN - v)) * 16.0f - z; }
static void isoQuad(float u0, float v0, float z0, float u1, float v1, float z1, float u2, float v2, float z2, float u3, float v3, float z3, u32 c) {
    float xs[4] = {isoX(u0, v0), isoX(u1, v1), isoX(u2, v2), isoX(u3, v3)};
    float ys[4] = {isoY(u0, v0, z0), isoY(u1, v1, z1), isoY(u2, v2, z2), isoY(u3, v3, z3)};
    fillPoly(xs, ys, 4, c);
}
static void isoTri(float u0, float v0, float z0, float u1, float v1, float z1, float u2, float v2, float z2, u32 c) {
    float xs[3] = {isoX(u0, v0), isoX(u1, v1), isoX(u2, v2)};
    float ys[3] = {isoY(u0, v0, z0), isoY(u1, v1, z1), isoY(u2, v2, z2)};
    fillPoly(xs, ys, 3, c);
}
// Пиксельно-точный изометрический параллелепипед
struct Face { int fx, fy, wr, wl, hz; };
static Face box(float u0, float v0, float u1, float v1, int z0, int hz, u32 top, u32 left, u32 right) {
    Face f;
    f.fx = (int)lroundf(isoX(u1, v1)); f.fy = (int)lroundf(isoY(u1, v1, (float)z0));
    f.wr = ((int)lroundf((v1 - v0) * 32.0f)) & ~1; f.wl = ((int)lroundf((u1 - u0) * 32.0f)) & ~1; f.hz = hz;
    for (int t = 0; t < f.wr; t++) for (int z = 0; z < hz; z++) pset(f.fx + t, f.fy - 1 - t / 2 - z, right);
    for (int t = 0; t < f.wl; t++) for (int z = 0; z < hz; z++) pset(f.fx - 1 - t, f.fy - 1 - t / 2 - z, left);
    if (top >> 24) {
        int fy2 = f.fy - hz, d = f.wr - f.wl;
        for (int dx = -f.wl; dx < f.wr; dx++) {
            int t = dx >= 0 ? dx : -dx - 1;
            int bot = fy2 - 1 - t / 2;
            int tp = dx >= d ? fy2 - (f.wr + f.wl) / 2 + (dx - d) / 2 : fy2 - (f.wr + f.wl) / 2 + (d - dx - 1) / 2;
            for (int y = tp; y <= bot; y++) pset(f.fx + dx, y, top);
        }
    }
    return f;
}
// прямоугольник на грани (side 1 = правая, 0 = левая), t — по горизонтали, z — по высоте
static void faceRect(const Face& f, int side, int t0, int t1, int z0, int z1, u32 c, u32 lit = 0) {
    for (int t = t0; t < t1; t++) for (int z = z0; z < z1; z++) {
        int x = side ? f.fx + t : f.fx - 1 - t, y = f.fy - 1 - t / 2 - z;
        pset(x, y, c); if (lit) lset(x, y, lit);
    }
}
static const u32 LIT1 = RGB(255, 214, 120), LIT2 = RGB(255, 236, 170), LIT3 = RGB(190, 220, 255);
// сетка окон на грани
static void windows(const Face& f, int side, int z0, int floors, int floorH, int ww, int wh, int step, int margin, u32 c, Rng& r, float litP = 0.6f, u32 litc = LIT1) {
    int width = side ? f.wr : f.wl;
    for (int k = 0; k < floors; k++) {
        int zz = z0 + k * floorH + (floorH - wh) / 2 + 1;
        for (int t = margin; t + ww <= width - margin + 1; t += step)
            faceRect(f, side, t, t + ww, zz, zz + wh, c, r.f() < litP ? litc : 0);
    }
}
// цилиндр (база в изометрии): cx,cy — центр основания в пикселях спрайта
static void cylinder(float cx, float cy, float r, int h, u32 c, u32 topc) {
    for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
        float dx = (x + 0.5f - cx) / r; if (dx < -1 || dx > 1) continue;
        float ey = sqrtf(1 - dx * dx) * r * 0.5f;
        float sh = 0.7f + 0.45f * (0.5f - dx * 0.5f); if (dx > 0.3f) sh = 0.75f - dx * 0.2f;
        for (int y = (int)floorf(cy - h - ey); y <= (int)ceilf(cy + ey); y++) pset(x, y, cshade(c, sh));
    }
    fillEllipse(cx, cy - h, r, r * 0.5f, topc);
}
// шум яркости по непрозрачным пикселям спрайта
static void noiseSpr(float amt, u32 seed) {
    for (int y = 0; y < cH; y++) for (int x = 0; x < cW; x++) {
        u32& d = cm[y * PGS + x]; if (!(d >> 24)) continue;
        float f = 1.0f + ((hash2(x + seed * 7, y + seed * 13) & 255) / 255.0f - 0.5f) * amt;
        d = cshade(d, f);
    }
}
// расширение на 1 пиксель (убирает щели между тайлами при масштабировании)
static void dilateSpr() {
    std::vector<u32> cp((size_t)cW * cH);
    for (int y = 0; y < cH; y++) for (int x = 0; x < cW; x++) cp[y * cW + x] = cm[y * PGS + x];
    for (int y = 0; y < cH; y++) for (int x = 0; x < cW; x++) {
        if (cp[y * cW + x] >> 24) continue;
        u32 c = 0;
        if (x > 0 && (cp[y * cW + x - 1] >> 24) == 255) c = cp[y * cW + x - 1];
        else if (x < cW - 1 && (cp[y * cW + x + 1] >> 24) == 255) c = cp[y * cW + x + 1];
        else if (y > 0 && (cp[(y - 1) * cW + x] >> 24) == 255) c = cp[(y - 1) * cW + x];
        else if (y < cH - 1 && (cp[(y + 1) * cW + x] >> 24) == 255) c = cp[(y + 1) * cW + x];
        if (c) cm[y * PGS + x] = c;
    }
}
// тёмный контур по краям силуэта (пиксель-арт)
static void outlineSpr(u32 col) {
    std::vector<u32> cp((size_t)cW * cH);
    for (int y = 0; y < cH; y++) for (int x = 0; x < cW; x++) cp[y * cW + x] = cm[y * PGS + x];
    for (int y = 0; y < cH; y++) for (int x = 0; x < cW; x++) {
        if (cp[y * cW + x] >> 24) continue;
        bool n = (x > 0 && cp[y * cW + x - 1] >> 24) || (x < cW - 1 && cp[y * cW + x + 1] >> 24) || (y > 0 && cp[(y - 1) * cW + x] >> 24);
        if (n) cm[y * PGS + x] = col;
    }
}

// ---------------------------------------------------------------------
//  Данные мира
// ---------------------------------------------------------------------
enum { MAPN = 128, NT = MAPN * MAPN };
enum { T_GRASS = 0, T_WATER = 1, T_SAND = 2 };
enum { F_PLINE = 1, F_PIPE = 2, F_POWER = 4, F_WATER = 8, F_ABAND = 16, F_FIRE = 32 };
enum { R_NONE = 0, R_DIRT = 1, R_ROAD = 2, R_AVE = 3 };
struct Tile {
    u8 ter, h, tree, road, zone, flags, level, var, bt, build, bad, fireT;
    int8_t ox, oy; u16 pop;
};
enum BT { B_NONE, B_GROW, B_RUBBLE, B_WIND, B_SOLAR, B_COAL, B_NUCLEAR, B_PUMP, B_WTOWER, B_SEWAGE,
          B_POLICE, B_FIRE, B_HOSPITAL, B_SCHOOL, B_UNIV, B_CITYHALL, B_PARKS, B_PARKL, B_FOUNTAIN, B_STADIUM, B_TVTOWER, B_COUNT };
enum { C_POLICE, C_FIRE, C_HEALTH, C_EDU, C_PARK, C_LV, NCOV };
struct BDef {
    const char* name; int size, cost, upkeep, tier, power, water, cov, radius, strength, poll, happy;
};
// power: >0 выработка МВт, <0 потребление
static const BDef BD[B_COUNT] = {
    {"", 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0},
    {"", 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0},
    {"РУИНЫ", 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0},
    {"ВЕТРЯК", 1, 300, 8, 0, 30, 0, -1, 0, 0, 0, 0},
    {"СОЛНЕЧНАЯ ЭС", 2, 1500, 25, 1, 110, 0, -1, 0, 0, 0, 0},
    {"УГОЛЬНАЯ ЭС", 3, 4000, 120, 0, 500, 0, -1, 0, 0, 220, 0},
    {"АЭС", 3, 25000, 500, 3, 2600, 0, -1, 0, 0, 0, 0},
    {"ВОДОКАЧКА", 1, 400, 15, 0, -3, 150, -1, 0, 0, 0, 0},
    {"ВОДОБАШНЯ", 1, 300, 10, 0, -2, 90, -1, 0, 0, 0, 0},
    {"ОЧИСТНЫЕ", 2, 2500, 80, 0, -15, 0, -1, 0, 0, 0, 0},
    {"ПОЛИЦИЯ", 2, 600, 60, 0, -6, 0, C_POLICE, 13, 230, 0, 0},
    {"ПОЖАРНАЯ ЧАСТЬ", 2, 600, 60, 0, -6, 0, C_FIRE, 13, 230, 0, 0},
    {"БОЛЬНИЦА", 2, 1500, 120, 1, -12, 0, C_HEALTH, 16, 230, 0, 0},
    {"ШКОЛА", 2, 800, 70, 0, -6, 0, C_EDU, 11, 200, 0, 0},
    {"УНИВЕРСИТЕТ", 3, 5000, 250, 2, -25, 0, C_EDU, 24, 255, 0, 2},
    {"МЭРИЯ", 2, 3000, 100, 1, -8, 0, C_LV, 12, 160, 0, 3},
    {"СКВЕР", 1, 80, 2, 0, 0, 0, C_PARK, 5, 160, 0, 0},
    {"ПАРК", 2, 400, 12, 1, 0, 0, C_PARK, 9, 230, 0, 0},
    {"ФОНТАН", 1, 250, 8, 1, -1, 0, C_PARK, 6, 200, 0, 0},
    {"СТАДИОН", 3, 6000, 280, 2, -30, 0, C_PARK, 26, 255, 0, 4},
    {"ТЕЛЕБАШНЯ", 2, 15000, 200, 3, -20, 0, C_LV, 30, 255, 0, 5},
};
static const char* TIER_NAME[4] = {"ДЕРЕВНЯ", "ПОСЁЛОК", "ГОРОД", "МЕГАПОЛИС"};
static const int TIER_POP[4] = {0, 400, 2500, 15000};
static const char* ZT_NAME[3] = {"ЖИЛАЯ", "ТОРГОВАЯ", "ПРОМЫШЛЕННАЯ"};
static const char* DN_NAME[3] = {"НИЗКАЯ", "СРЕДНЯЯ", "ВЫСОКАЯ"};
static const char* GROW_NAME[3][6] = {
    {"", "ДОМИК", "КОТТЕДЖ", "ПЯТИЭТАЖКА", "ДЕВЯТИЭТАЖКА", "ВЫСОТКА"},
    {"", "ЛАРЁК", "МАГАЗИН", "ОФИСНЫЙ ДОМ", "БИЗНЕС-ЦЕНТР", "НЕБОСКРЁБ"},
    {"", "МАСТЕРСКАЯ", "ЦЕХ", "ЗАВОД", "ТЕХНОПАРК", "ТЕХНОПАРК"}};
static const int GROW_CAP[3][6] = {{0, 8, 24, 90, 240, 600}, {0, 4, 16, 60, 160, 420}, {0, 10, 30, 90, 200, 200}};
static const int ROAD_COST[4] = {0, 6, 15, 40};
static const char* ROAD_NAME[4] = {"", "ГРУНТОВКА", "ДОРОГА", "ПРОСПЕКТ"};
static const int ROAD_CAP[4] = {1, 40, 110, 260};

static Tile map_[NT];
static inline Tile& TL(int x, int y) { return map_[y * MAPN + x]; }
static inline bool inMap(int x, int y) { return x >= 0 && y >= 0 && x < MAPN && y < MAPN; }
static inline int effH(const Tile& t) { return (t.ter == T_WATER && t.road) ? 1 : t.h; }
static inline int zoneType(const Tile& t) { return t.zone ? (t.zone - 1) / 3 : -1; }
static inline int zoneDen(const Tile& t) { return t.zone ? (t.zone - 1) % 3 : 0; }
static inline bool isOrigin(const Tile& t) { return t.ox == 0 && t.oy == 0; }

// карты симуляции
static u8 cov[NCOV][NT];
static u8 pollution[NT], landv[NT], crime[NT], traffic[NT], access_[NT], wdist[NT];
static int nearRoad[NT];

// состояние города
struct City {
    u32 seed = 1; bool sandbox = false;
    i64 money = 20000; i64 debt = 0;
    int day = 0, hour = 0;
    int tax[3] = {9, 9, 9};
    int pop = 0, jobsC = 0, jobsI = 0;
    float demand[3] = {1, 0.5f, 0.8f};
    int happy = 60;
    int powerCap = 0, powerUse = 0, powerNeed = 0, waterCap = 0, waterUse = 0, waterNeed = 0, sewageCap = 0;
    int tier = 0; u32 ach = 0; int treesPlanted = 0;
    // отчёт за прошлый месяц
    int incR = 0, incC = 0, incI = 0, expServ = 0, expRoad = 0, expUtil = 0, expLoan = 0;
    float dayCycle = 0.30f;
};
static City city;
static int slot = 0; // текущий слот сохранения (0..2)

// ---------------------------------------------------------------------
//  Генерация всех спрайтов
// ---------------------------------------------------------------------
static int SP_GRASS[4][3], SP_SAND[4], SP_WATER[4], SP_CLIFFR, SP_CLIFFL, SP_CLIFFRS, SP_CLIFFLS;
static int SP_ROAD[4][16], SP_BRIDGE[4][2], SP_ZONE[10], SP_PLINE[16], SP_PIPE[16];
static int SP_GROW[3][6][3], SP_CONSTR[3], SP_RUBBLE, SP_BLD[B_COUNT];
static int SP_TREE[4][4], SP_CAR[8][2], SP_PERSON[4], SP_PUFF, SP_FLAME[3], SP_GLOW, SP_ROTOR, SP_DIAM, SP_DOUT, SP_BEACON;
static int SP_ICON[IC_COUNT], SP_FONT[128];

static const u32 GRASS_C[4] = {RGB(112, 178, 72), RGB(96, 160, 62), RGB(156, 150, 70), RGB(226, 232, 238)};

static void diamond(u32 c) { box(0, 0, (float)curN, (float)curN, 0, 0, c, c, c); }
static void plotGrass() { /* земля под зданием рисуется тайлом текущего сезона */ }
static void plotPave(u32 c = RGB(176, 176, 168)) {
    diamond(c);
    // швы плитки
    for (int k = 1; k < curN * 4; k++) {
        float t = k * 0.25f;
        float xs0 = isoX(t, 0), ys0 = isoY(t, 0, 0), xs1 = isoX(t, (float)curN), ys1 = isoY(t, (float)curN, 0);
        pline((int)xs0, (int)ys0, (int)xs1, (int)ys1, cshade(c, 0.92f));
        xs0 = isoX(0, t); ys0 = isoY(0, t, 0); xs1 = isoX((float)curN, t); ys1 = isoY((float)curN, t, 0);
        pline((int)xs0, (int)ys0, (int)xs1, (int)ys1, cshade(c, 0.92f));
    }
}

static void genTerrain() {
    for (int s = 0; s < 4; s++) for (int v = 0; v < 3; v++) {
        SP_GRASS[s][v] = newSpr(64, 32, 32, 32);
        diamond(GRASS_C[s]); noiseSpr(0.10f, s * 31 + v * 7 + 1);
        Rng r(100 + s * 10 + v);
        for (int k = 0; k < 26; k++) {
            int x = 8 + r.i(48), y = 4 + r.i(24);
            if (!(pget(x, y) >> 24) || !(pget(x, y - 1) >> 24)) continue;
            if (s == 3) { pset(x, y, RGB(200, 208, 220)); continue; }
            u32 c = cshade(GRASS_C[s], r.f() < 0.5f ? 0.82f : 1.15f);
            pset(x, y, c); pset(x, y - 1, c);
            if (s == 0 && r.f() < 0.25f) pset(x, y - 1, r.f() < 0.5f ? RGB(250, 240, 120) : RGB(240, 160, 200));
            if (s == 2 && r.f() < 0.3f) pset(x, y, RGB(200, 120, 40));
        }
        dilateSpr();
    }
    for (int s = 0; s < 4; s++) {
        SP_SAND[s] = newSpr(64, 32, 32, 32);
        diamond(s == 3 ? RGB(232, 230, 222) : RGB(222, 204, 150)); noiseSpr(0.12f, 77 + s); dilateSpr();
    }
    for (int f = 0; f < 4; f++) {
        SP_WATER[f] = newSpr(64, 32, 32, 32);
        diamond(RGB(48, 112, 185));
        for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) {
            if (!(pget(x, y) >> 24)) continue;
            float w = sinf(x * 0.35f + y * 0.9f + f * 1.5708f) + 0.6f * sinf(x * 0.13f - y * 0.4f + f * 0.8f);
            if (w > 1.2f) pset(x, y, RGB(90, 160, 220));
            else if (w < -1.1f) pset(x, y, RGB(38, 95, 165));
            if ((hash2(x, y + f * 101) % 211) == 0) pset(x, y, RGB(230, 245, 255));
        }
        dilateSpr();
    }
    // скалы (боковые грани поднятых клеток)
    for (int k = 0; k < 4; k++) {
        bool right = (k & 1) == 0, sand = k >= 2;
        int id = newSpr(32, 24, right ? 0 : 32, 24);
        u32 c = sand ? (right ? RGB(176, 150, 100) : RGB(200, 172, 118)) : (right ? RGB(118, 84, 54) : RGB(150, 108, 70));
        for (int t = 0; t < 32; t++) for (int z = 0; z < 8; z++) {
            int x = right ? t : 31 - t, y = 23 - t / 2 - z;
            u32 cc = c;
            if (z == 7) cc = sand ? cshade(c, 1.1f) : (right ? RGB(80, 130, 52) : RGB(96, 150, 60));
            else if ((hash2(t, z + k * 9) & 7) == 0) cc = cshade(c, 0.85f);
            else if (z == 3 && (t & 4)) cc = cshade(c, 0.92f);
            pset(x, y, cc);
        }
        dilateSpr();
        if (k == 0) SP_CLIFFR = id; else if (k == 1) SP_CLIFFL = id; else if (k == 2) SP_CLIFFRS = id; else SP_CLIFFLS = id;
    }
    SP_DIAM = newSpr(64, 32, 32, 32); diamond(RGB(255, 255, 255));
    SP_DOUT = newSpr(64, 32, 32, 32);
    diamond(RGB(255, 255, 255));
    for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) {
        bool edge = (pget(x, y) >> 24) && (!(pget(x - 1, y) >> 24) || !(pget(x + 1, y) >> 24) || !(pget(x, y - 1) >> 24) || !(pget(x, y + 1) >> 24) ||
                                          !(pget(x - 2, y) >> 24) || !(pget(x + 2, y) >> 24));
        if (!edge) cm[y * PGS + x] = 0;
    }
    // зоны: 1..9
    const u32 ZC[3] = {RGB(70, 200, 90), RGB(70, 130, 235), RGB(235, 200, 45)};
    for (int z = 1; z <= 9; z++) {
        int zt = (z - 1) / 3, dn = (z - 1) % 3;
        SP_ZONE[z] = newSpr(64, 32, 32, 32);
        diamond(calpha(ZC[zt], 60 + dn * 35));
        for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) {
            if (!(pget(x, y) >> 24)) continue;
            bool edge = !(pget(x - 2, y) >> 24) || !(pget(x + 2, y) >> 24) || !(pget(x, y - 1) >> 24) || !(pget(x, y + 1) >> 24);
            if (edge) cm[y * PGS + x] = calpha(ZC[zt], 230);
            else if (dn == 2 && ((x + 2 * y) % 8) < 2) cm[y * PGS + x] = calpha(ZC[zt], 170);
            else if (dn == 1 && ((x + 2 * y) % 12) < 1) cm[y * PGS + x] = calpha(ZC[zt], 170);
        }
    }
    SP_ZONE[0] = SP_DIAM;
}

static void roadShape(float hw, int mask, u32 c, float z = 0) {
    float c0 = 0.5f - hw, c1 = 0.5f + hw;
    isoQuad(c0, c0, z, c1, c0, z, c1, c1, z, c0, c1, z, c);
    if (mask & 1) isoQuad(c1 - 0.02f, c0, z, 1, c0, z, 1, c1, z, c1 - 0.02f, c1, z, c);
    if (mask & 2) isoQuad(0, c0, z, c0 + 0.02f, c0, z, c0 + 0.02f, c1, z, 0, c1, z, c);
    if (mask & 4) isoQuad(c0, c1 - 0.02f, z, c1, c1 - 0.02f, z, c1, 1, z, c0, 1, z, c);
    if (mask & 8) isoQuad(c0, 0, z, c1, 0, z, c1, c0 + 0.02f, z, c0, c0 + 0.02f, z, c);
}
static void isoLine(float u0, float v0, float u1, float v1, float z, u32 c) {
    pline((int)floorf(isoX(u0, v0)), (int)floorf(isoY(u0, v0, z)), (int)floorf(isoX(u1, v1)), (int)floorf(isoY(u1, v1, z)), c);
}
// разметка вдоль оси: axis 0 = u, 1 = v; from..to по оси, off — поперечная координата
static void dashLine(int axis, float from, float to, float off, float z, u32 c, bool dashed) {
    float step = dashed ? 0.25f : (to - from);
    for (float a = from; a < to - 0.01f; a += step) {
        float b = std::min(to, a + (dashed ? 0.13f : step));
        if (axis == 0) isoLine(a, off, b, off, z, c); else isoLine(off, a, off, b, z, c);
    }
}
static void roadMarks(int type, int mask, float z) {
    float hw = type == R_DIRT ? 0.18f : (type == R_ROAD ? 0.27f : 0.40f);
    float c0 = 0.5f - hw, c1 = 0.5f + hw;
    int cnt = ((mask & 1) != 0) + ((mask & 2) != 0) + ((mask & 4) != 0) + ((mask & 8) != 0);
    bool straightX = mask == 3, straightY = mask == 12;
    u32 W = RGB(235, 235, 225), Y = RGB(240, 200, 60);
    if (type == R_ROAD) {
        if (straightX) dashLine(0, 0.0f, 1.0f, 0.5f, z, W, true);
        else if (straightY) dashLine(1, 0.0f, 1.0f, 0.5f, z, W, true);
        else {
            if (mask & 1) dashLine(0, c1 + 0.08f, 1.0f, 0.5f, z, W, true);
            if (mask & 2) dashLine(0, 0.0f, c0 - 0.08f, 0.5f, z, W, true);
            if (mask & 4) dashLine(1, c1 + 0.08f, 1.0f, 0.5f, z, W, true);
            if (mask & 8) dashLine(1, 0.0f, c0 - 0.08f, 0.5f, z, W, true);
        }
    } else if (type == R_AVE) {
        u32 G = RGB(80, 140, 60);
        auto med = [&](int axis, float a, float b) {
            if (axis == 0) isoQuad(a, 0.46f, z, b, 0.46f, z, b, 0.54f, z, a, 0.54f, z, G);
            else isoQuad(0.46f, a, z, 0.54f, a, z, 0.54f, b, z, 0.46f, b, z, G);
            dashLine(axis, a, b, 0.29f, z, W, true); dashLine(axis, a, b, 0.71f, z, W, true);
        };
        if (straightX) med(0, 0, 1); else if (straightY) med(1, 0, 1);
        else {
            if (mask & 1) med(0, c1 + 0.02f, 1); if (mask & 2) med(0, 0, c0 - 0.02f);
            if (mask & 4) med(1, c1 + 0.02f, 1); if (mask & 8) med(1, 0, c0 - 0.02f);
        }
        (void)Y;
    }
    if (type >= R_ROAD && cnt >= 3) { // пешеходные переходы
        for (int d = 0; d < 4; d++) {
            if (!(mask & (1 << d))) continue;
            for (float s = c0 + 0.04f; s < c1 - 0.03f; s += 0.09f) {
                float a0 = s, a1 = s + 0.045f;
                if (d == 0) isoQuad(c1, a0, z, c1 + 0.1f, a0, z, c1 + 0.1f, a1, z, c1, a1, z, W);
                if (d == 1) isoQuad(c0 - 0.1f, a0, z, c0, a0, z, c0, a1, z, c0 - 0.1f, a1, z, W);
                if (d == 2) isoQuad(a0, c1, z, a1, c1, z, a1, c1 + 0.1f, z, a0, c1 + 0.1f, z, W);
                if (d == 3) isoQuad(a0, c0 - 0.1f, z, a1, c0 - 0.1f, z, a1, c0, z, a0, c0, z, W);
            }
        }
    }
}
static void genRoads() {
    for (int type = 1; type <= 3; type++) for (int mask = 0; mask < 16; mask++) {
        SP_ROAD[type][mask] = newSpr(64, 32, 32, 32);
        float hw = type == R_DIRT ? 0.18f : (type == R_ROAD ? 0.27f : 0.40f);
        if (type == R_DIRT) {
            roadShape(hw + 0.04f, mask, RGB(150, 120, 80));
            roadShape(hw - 0.02f, mask, RGB(176, 140, 92));
            noiseSpr(0.14f, 900 + mask);
        } else {
            diamond(RGB(168, 168, 160));
            noiseSpr(0.05f, 300 + mask);
            roadShape(hw + 0.03f, mask, RGB(128, 128, 124));
            roadShape(hw, mask, type == R_AVE ? RGB(62, 64, 70) : RGB(74, 76, 82));
            roadMarks(type, mask, 0);
            dilateSpr();
        }
    }
    for (int type = 1; type <= 3; type++) for (int ax = 0; ax < 2; ax++) {
        SP_BRIDGE[type][ax] = newSpr(64, 46, 32, 46);
        float hw = type == R_DIRT ? 0.22f : (type == R_ROAD ? 0.31f : 0.44f);
        float a0 = 0.5f - hw, a1 = 0.5f + hw;
        u32 deck = type == R_DIRT ? RGB(150, 105, 60) : RGB(74, 76, 82), side = type == R_DIRT ? RGB(110, 75, 45) : RGB(150, 150, 145);
        u32 pil = RGB(140, 135, 125);
        if (ax == 0) {
            box(0.40f, a0 + 0.06f, 0.60f, a1 - 0.06f, -2, 6, pil, cshade(pil, 0.9f), cshade(pil, 0.7f));
            box(0, a0, 1, a1, 4, 4, deck, cshade(side, 0.9f), cshade(side, 0.7f));
            if (type == R_DIRT) for (float u = 0.06f; u < 1; u += 0.12f) isoLine(u, a0, u, a1, 8, RGB(120, 82, 48));
            else roadMarks(type, 3, 8);
            box(0, a0 - 0.06f, 1, a0, 8, 3, side, side, cshade(side, 0.8f));
            box(0, a1, 1, a1 + 0.06f, 8, 3, side, side, cshade(side, 0.8f));
        } else {
            box(a0 + 0.06f, 0.40f, a1 - 0.06f, 0.60f, -2, 6, pil, cshade(pil, 0.9f), cshade(pil, 0.7f));
            box(a0, 0, a1, 1, 4, 4, deck, cshade(side, 0.9f), cshade(side, 0.7f));
            if (type == R_DIRT) for (float v = 0.06f; v < 1; v += 0.12f) isoLine(a0, v, a1, v, 8, RGB(120, 82, 48));
            else roadMarks(type, 12, 8);
            box(a0 - 0.06f, 0, a0, 1, 8, 3, side, cshade(side, 0.9f), side);
            box(a1, 0, a1 + 0.06f, 1, 8, 3, side, cshade(side, 0.9f), side);
        }
    }
    // ЛЭП
    for (int mask = 0; mask < 16; mask++) {
        SP_PLINE[mask] = newSpr(64, 64, 32, 64);
        u32 wood = RGB(112, 80, 50), wire = ARGB(230, 40, 40, 45);
        float px = isoX(0.5f, 0.5f), py = isoY(0.5f, 0.5f, 0);
        const float eu[4] = {1, 0, 0.5f, 0.5f}, ev[4] = {0.5f, 0.5f, 1, 0};
        for (int d = 0; d < 4; d++) if (mask & (1 << d)) {
            for (int w = 0; w < 2; w++) {
                float z = w ? 21.0f : 26.0f;
                float ex = isoX(eu[d], ev[d]), ey = isoY(eu[d], ev[d], z);
                float mx = (px + ex) * 0.5f, my = (py - z + ey) * 0.5f + 1.5f;
                pline((int)px, (int)(py - z), (int)mx, (int)my, wire); pline((int)mx, (int)my, (int)ex, (int)ey, wire);
            }
        }
        prect((int)px - 1, (int)py - 28, 2, 28, wood); prect((int)px, (int)py - 28, 1, 28, cshade(wood, 1.25f));
        prect((int)px - 5, (int)py - 26, 10, 2, cshade(wood, 0.9f));
        prect((int)px - 5, (int)py - 21, 10, 1, cshade(wood, 0.9f));
        pset((int)px - 5, (int)py - 27, RGB(220, 220, 230)); pset((int)px + 4, (int)py - 27, RGB(220, 220, 230));
    }
    // трубы (подземный вид)
    for (int mask = 0; mask < 16; mask++) {
        SP_PIPE[mask] = newSpr(64, 32, 32, 32);
        float px = 32, py = 16;
        const float ex[4] = {48, 16, 16, 48}, ey[4] = {24, 8, 24, 8};
        for (int d = 0; d < 4; d++) if (mask & (1 << d)) {
            for (int o = -2; o <= 2; o++) pline((int)px, (int)py + o, (int)ex[d], (int)ey[d] + o, o == -1 ? RGB(150, 205, 255) : (o == 2 ? RGB(30, 80, 160) : RGB(60, 140, 230)));
        }
        fillEllipse(px, py, 5, 3.5f, RGB(60, 140, 230)); fillEllipse(px - 1, py - 1, 2, 1.2f, RGB(160, 210, 255));
    }
}

// --- растущие здания ---
static const u32 WALL_R[6] = {RGB(236, 222, 184), RGB(172, 202, 226), RGB(232, 184, 170), RGB(240, 240, 234), RGB(232, 208, 124), RGB(196, 220, 170)};
static const u32 ROOF_R[5] = {RGB(172, 62, 46), RGB(92, 92, 104), RGB(62, 112, 72), RGB(72, 92, 152), RGB(142, 82, 50)};

static void gableRoof(float u0, float v0, float u1, float v1, float ze, float zr, u32 roof, u32 wall) {
    float o = 0.05f, vm = (v0 + v1) * 0.5f;
    isoQuad(u0 - o, v0 - o, ze, u1 + o, v0 - o, ze, u1 + o, vm, zr, u0 - o, vm, zr, cshade(roof, 0.75f));
    isoTri(u1, v0, ze, u1, v1, ze, u1, vm, zr, cshade(wall, 0.78f));
    isoQuad(u0 - o, v1 + o, ze, u1 + o, v1 + o, ze, u1 + o, vm, zr, u0 - o, vm, zr, roof);
    isoLine(u0 - o, vm, u1 + o, vm, zr, cshade(roof, 1.25f));
}
static Face sbox(float u0, float v0, float u1, float v1, int z0, int hz, u32 c) { return box(u0, v0, u1, v1, z0, hz, cshade(c, 1.08f), cshade(c, 0.92f), cshade(c, 0.74f)); }
static void flatRoof(float u0, float v0, float u1, float v1, int z, u32 c) {
    box(u0, v0, u1, v1, z, 0, cshade(c, 0.95f), 0, 0);
    box(u0 + 0.06f, v0 + 0.06f, u1 - 0.06f, v1 - 0.06f, z, 0, cshade(c, 0.78f), 0, 0);
}

static void genGrowR(int lvl, int var) {
    Rng r(1000 + lvl * 17 + var * 5);
    u32 wall = WALL_R[(var * 2 + lvl) % 6], roof = ROOF_R[(var + lvl) % 5];
    int H = lvl == 1 ? 64 : lvl == 2 ? 76 : lvl == 3 ? 96 : lvl == 4 ? 130 : 186;
    SP_GROW[0][lvl][var] = newSpr(64, H, 32, H);
    if (lvl <= 2) {
        plotGrass(); noiseSpr(0.08f, 11 + var);
        isoQuad(0.40f, 0.72f, 0, 0.52f, 0.72f, 0, 0.52f, 1.0f, 0, 0.40f, 1.0f, 0, RGB(190, 180, 160));
        // забор
        for (float u = 0.03f; u < 1; u += 0.12f) isoLine(u, 0.97f, u, 0.97f, 4, RGB(240, 240, 230));
        isoLine(0.0f, 0.97f, 0.38f, 0.97f, 3, RGB(220, 215, 200)); isoLine(0.54f, 0.97f, 1.0f, 0.97f, 3, RGB(220, 215, 200));
        fillEllipse(isoX(0.82f, 0.18f), isoY(0.82f, 0.18f, 6), 6, 5, RGB(60, 120, 50));
        fillEllipse(isoX(0.80f, 0.16f) - 1, isoY(0.80f, 0.16f, 8), 3, 2.5f, RGB(90, 160, 70));
        if (lvl == 1) {
            Face f = sbox(0.22f, 0.28f, 0.72f, 0.72f, 0, 13, wall);
            faceRect(f, 0, 6, 10, 0, 8, RGB(110, 70, 40));
            faceRect(f, 0, 12, 15, 4, 8, RGB(90, 130, 170), r.f() < 0.6f ? LIT1 : 0);
            faceRect(f, 1, 3, 6, 4, 8, RGB(90, 130, 170), r.f() < 0.6f ? LIT1 : 0);
            faceRect(f, 1, 9, 12, 4, 8, RGB(90, 130, 170), r.f() < 0.5f ? LIT1 : 0);
            box(0.3125f, 0.375f, 0.375f, 0.4375f, 14, 12, RGB(150, 80, 60), RGB(140, 70, 50), RGB(110, 55, 40));
            gableRoof(0.22f, 0.28f, 0.72f, 0.72f, 13, 23, roof, wall);
            addEmit(EM_SMOKE, (int)isoX(0.34f, 0.40f), (int)isoY(0.34f, 0.40f, 27));
        } else {
            Face f = sbox(0.16f, 0.22f, 0.78f, 0.74f, 0, 22, wall);
            for (int fl = 0; fl < 2; fl++) {
                for (int t = 4; t + 3 <= f.wl - 2; t += 7) faceRect(f, 0, t, t + 3, 3 + fl * 11, 8 + fl * 11, RGB(90, 130, 170), r.f() < 0.5f ? LIT1 : 0);
                for (int t = 3; t + 3 <= f.wr - 2; t += 6) faceRect(f, 1, t, t + 3, 3 + fl * 11, 8 + fl * 11, RGB(90, 130, 170), r.f() < 0.5f ? LIT1 : 0);
            }
            faceRect(f, 0, 2, 6, 0, 8, RGB(110, 70, 40));
            box(0.78f, 0.40f, 0.94f, 0.74f, 0, 9, RGB(200, 200, 195), RGB(185, 185, 180), RGB(150, 150, 145));
            gableRoof(0.16f, 0.22f, 0.78f, 0.74f, 22, 34, roof, wall);
        }
        return;
    }
    plotPave(RGB(170, 170, 162));
    isoQuad(0, 0, 0, 1, 0, 0, 1, 0.12f, 0, 0, 0.12f, 0, RGB(98, 160, 64));
    if (lvl == 3) {
        const u32 P[3] = {RGB(214, 206, 190), RGB(196, 120, 96), RGB(200, 210, 214)};
        u32 c = P[var];
        Face f = sbox(0.10f, 0.20f, 0.90f, 0.80f, 0, 50, c);
        windows(f, 0, 0, 5, 10, 4, 5, 7, 3, RGB(70, 100, 140), r, 0.55f);
        windows(f, 1, 0, 5, 10, 4, 5, 7, 3, RGB(70, 100, 140), r, 0.55f);
        for (int t = 6; t < f.wl - 6; t += 16) { faceRect(f, 0, t, t + 4, 0, 7, RGB(80, 60, 50)); faceRect(f, 0, t - 1, t + 5, 7, 8, RGB(120, 120, 120)); }
        flatRoof(0.10f, 0.20f, 0.90f, 0.80f, 50, RGB(120, 115, 110));
        sbox(0.6f, 0.3f, 0.75f, 0.45f, 50, 6, RGB(160, 150, 140));
    } else if (lvl == 4) {
        const u32 P[3] = {RGB(228, 226, 216), RGB(206, 196, 172), RGB(178, 196, 210)};
        u32 c = P[var];
        Face f = sbox(0.12f, 0.12f, 0.88f, 0.88f, 0, 81, c);
        windows(f, 0, 0, 9, 9, 4, 5, 7, 3, RGB(70, 100, 140), r, 0.5f);
        windows(f, 1, 0, 9, 9, 4, 5, 7, 3, RGB(70, 100, 140), r, 0.5f);
        for (int k = 1; k < 9; k++) for (int t = 3; t + 5 <= f.wl - 2; t += 14) faceRect(f, 0, t - 1, t + 6, k * 9, k * 9 + 1, cshade(c, 0.8f));
        faceRect(f, 0, 10, 15, 0, 7, RGB(80, 60, 50));
        flatRoof(0.12f, 0.12f, 0.88f, 0.88f, 81, RGB(110, 108, 104));
        sbox(0.4f, 0.4f, 0.6f, 0.6f, 81, 8, cshade(c, 0.9f));
    } else {
        const u32 P[3] = {RGB(206, 196, 180), RGB(176, 96, 76), RGB(206, 218, 230)};
        u32 c = P[var];
        Face f = sbox(0.18f, 0.18f, 0.82f, 0.82f, 0, 136, c);
        windows(f, 0, 0, 17, 8, 3, 4, 5, 2, var == 2 ? RGB(80, 120, 170) : RGB(70, 95, 130), r, 0.5f);
        windows(f, 1, 0, 17, 8, 3, 4, 5, 2, var == 2 ? RGB(80, 120, 170) : RGB(70, 95, 130), r, 0.5f);
        faceRect(f, 0, 0, 2, 0, 136, cshade(c, 1.15f));
        Face g = sbox(0.28f, 0.28f, 0.72f, 0.72f, 136, 10, cshade(c, 0.85f));
        (void)g;
        float ax = isoX(0.5f, 0.5f), ay = isoY(0.5f, 0.5f, 146);
        prect((int)ax, (int)ay - 18, 1, 18, RGB(80, 80, 85));
        addEmit(EM_BEACON, (int)ax, (int)ay - 19);
    }
}
static void genGrowC(int lvl, int var) {
    Rng r(2000 + lvl * 17 + var * 5);
    int H = lvl == 1 ? 60 : lvl == 2 ? 70 : lvl == 3 ? 100 : lvl == 4 ? 150 : 210;
    SP_GROW[1][lvl][var] = newSpr(64, H, 32, H);
    plotPave();
    const u32 BR[3] = {RGB(230, 90, 70), RGB(70, 160, 210), RGB(240, 180, 50)};
    if (lvl == 1) {
        u32 c = WALL_R[(var + 3) % 6];
        Face f = sbox(0.25f, 0.30f, 0.75f, 0.72f, 0, 12, c);
        faceRect(f, 0, 2, f.wl - 2, 2, 9, RGB(120, 170, 205), LIT2);
        faceRect(f, 1, 3, 7, 0, 8, RGB(110, 80, 50));
        flatRoof(0.25f, 0.30f, 0.75f, 0.72f, 12, RGB(140, 135, 130));
        for (int k = 0; k < 6; k++) {
            float a = 0.25f + k * (0.5f / 6), b = a + 0.5f / 6;
            isoQuad(a, 0.72f, 11, b, 0.72f, 11, b, 0.86f, 7, a, 0.86f, 7, (k & 1) ? RGB(250, 250, 245) : BR[var]);
        }
        Face s = box(0.30f, 0.62f, 0.70f, 0.70f, 12, 5, BR[var], cshade(BR[var], 0.9f), cshade(BR[var], 0.7f));
        faceRect(s, 0, 2, s.wl - 2, 1, 4, RGB(255, 255, 230), LIT2);
    } else if (lvl == 2) {
        u32 c = WALL_R[(var * 2 + 1) % 6];
        Face f = sbox(0.12f, 0.15f, 0.88f, 0.85f, 0, 24, c);
        faceRect(f, 0, 2, f.wl - 2, 2, 10, RGB(120, 170, 205), LIT2);
        faceRect(f, 1, 2, f.wr - 2, 2, 10, RGB(120, 170, 205), LIT2);
        faceRect(f, 0, 0, f.wl, 11, 14, BR[var], LIT2);
        faceRect(f, 1, 0, f.wr, 11, 14, cshade(BR[var], 0.85f));
        windows(f, 0, 14, 1, 10, 4, 5, 7, 3, RGB(80, 110, 150), r, 0.5f);
        windows(f, 1, 14, 1, 10, 4, 5, 7, 3, RGB(80, 110, 150), r, 0.5f);
        flatRoof(0.12f, 0.15f, 0.88f, 0.85f, 24, RGB(130, 130, 128));
        sbox(0.3f, 0.3f, 0.45f, 0.45f, 24, 4, RGB(190, 190, 195));
        sbox(0.55f, 0.35f, 0.7f, 0.5f, 24, 4, RGB(190, 190, 195));
    } else if (lvl == 3) {
        const u32 P[3] = {RGB(236, 236, 230), RGB(200, 200, 205), RGB(214, 200, 176)};
        u32 c = P[var];
        Face f = sbox(0.12f, 0.12f, 0.88f, 0.88f, 0, 56, c);
        faceRect(f, 0, 1, f.wl - 1, 2, 9, RGB(110, 160, 200), LIT2);
        faceRect(f, 1, 1, f.wr - 1, 2, 9, RGB(110, 160, 200), LIT2);
        for (int k = 1; k < 7; k++) {
            for (int t = 2; t + 4 <= f.wl - 1; t += 5) faceRect(f, 0, t, t + 4, k * 8 + 3, k * 8 + 7, RGB(60, 90, 130), r.f() < 0.55f ? LIT3 : 0);
            for (int t = 2; t + 4 <= f.wr - 1; t += 5) faceRect(f, 1, t, t + 4, k * 8 + 3, k * 8 + 7, RGB(60, 90, 130), r.f() < 0.55f ? LIT3 : 0);
        }
        flatRoof(0.12f, 0.12f, 0.88f, 0.88f, 56, RGB(120, 120, 118));
        Face s = box(0.12f, 0.80f, 0.6f, 0.88f, 56, 6, BR[var], BR[var], cshade(BR[var], 0.8f)); (void)s;
    } else if (lvl == 4) {
        const u32 G[3] = {RGB(80, 120, 160), RGB(70, 140, 140), RGB(110, 120, 140)};
        u32 g = G[var];
        Face f = box(0.15f, 0.15f, 0.85f, 0.85f, 0, 100, cshade(g, 1.1f), cshade(g, 1.0f), cshade(g, 0.75f));
        for (int k = 0; k < 12; k++) {
            faceRect(f, 0, 0, f.wl, k * 8 + 7, k * 8 + 8, cshade(g, 0.7f));
            faceRect(f, 1, 0, f.wr, k * 8 + 7, k * 8 + 8, cshade(g, 0.55f));
            for (int t = 1; t + 3 <= f.wl; t += 4) if (r.f() < 0.4f) faceRect(f, 0, t, t + 3, k * 8 + 2, k * 8 + 6, cshade(g, 1.2f), r.f() < 0.7f ? LIT3 : 0);
            for (int t = 1; t + 3 <= f.wr; t += 4) if (r.f() < 0.4f) faceRect(f, 1, t, t + 3, k * 8 + 2, k * 8 + 6, cshade(g, 0.9f), r.f() < 0.7f ? LIT3 : 0);
        }
        for (int t = 0; t < f.wl; t += 6) faceRect(f, 0, t, t + 1, 0, 100, cshade(g, 1.3f));
        flatRoof(0.15f, 0.15f, 0.85f, 0.85f, 100, RGB(110, 115, 120));
        Face g2 = box(0.28f, 0.28f, 0.72f, 0.72f, 100, 14, cshade(g, 1.1f), cshade(g, 0.95f), cshade(g, 0.7f));
        windows(g2, 0, 0, 1, 14, 3, 6, 5, 2, cshade(g, 1.2f), r, 0.6f, LIT3);
        flatRoof(0.28f, 0.28f, 0.72f, 0.72f, 114, RGB(120, 120, 120));
    } else {
        const u32 G[3] = {RGB(70, 110, 170), RGB(60, 130, 130), RGB(160, 140, 90)};
        u32 g = G[var];
        auto tier = [&](float a, float b, int z0, int hz) {
            Face f = box(a, a, b, b, z0, hz, cshade(g, 1.1f), cshade(g, 1.0f), cshade(g, 0.72f));
            for (int k = 0; k < hz / 6; k++) {
                for (int t = 1; t + 2 <= f.wl; t += 3) faceRect(f, 0, t, t + 2, k * 6 + 1, k * 6 + 4, cshade(g, 1.25f), r.f() < 0.45f ? LIT3 : 0);
                for (int t = 1; t + 2 <= f.wr; t += 3) faceRect(f, 1, t, t + 2, k * 6 + 1, k * 6 + 4, cshade(g, 0.92f), r.f() < 0.45f ? LIT3 : 0);
            }
            faceRect(f, 0, 0, 1, 0, hz, cshade(g, 1.4f));
            box(a, a, b, b, z0 + hz, 0, cshade(g, 0.8f), 0, 0);
        };
        tier(0.14f, 0.86f, 0, 120); tier(0.22f, 0.78f, 120, 36); tier(0.32f, 0.68f, 156, 18);
        float ax = isoX(0.5f, 0.5f), ay = isoY(0.5f, 0.5f, 174);
        prect((int)ax - 1, (int)ay - 26, 2, 26, RGB(200, 200, 210));
        addEmit(EM_BEACON, (int)ax, (int)ay - 27);
    }
}
static void chimney(float u, float v, int z0, int h, int rad, bool smoke) {
    float cx = isoX(u, v), cy = isoY(u, v, (float)z0);
    for (int k = 0; k < h; k += 8) {
        int hh = std::min(8, h - k);
        u32 c = ((k / 8) & 1) ? RGB(220, 220, 215) : RGB(190, 70, 55);
        if (k < h - 16) c = RGB(170, 90, 70);
        for (int x = (int)(cx - rad); x <= (int)(cx + rad); x++) {
            float dx = (x + 0.5f - cx) / rad; if (dx < -1 || dx > 1) continue;
            float sh = dx < 0.2f ? 1.0f : 0.72f;
            for (int y = (int)(cy - k - hh); y < (int)(cy - k); y++) pset(x, y, cshade(c, sh));
        }
    }
    fillEllipse(cx, cy - h, (float)rad, rad * 0.5f, RGB(40, 35, 35));
    if (smoke) addEmit(EM_SMOKE, (int)cx, (int)(cy - h - 1));
}
static void genGrowI(int lvl, int var) {
    Rng r(3000 + lvl * 17 + var * 5);
    int H = lvl == 1 ? 60 : lvl == 2 ? 100 : 110;
    if (lvl == 5) { SP_GROW[2][5][var] = SP_GROW[2][4][var]; return; }
    SP_GROW[2][lvl][var] = newSpr(64, H, 32, H);
    plotPave(RGB(158, 152, 140)); noiseSpr(0.08f, 50 + lvl + var);
    if (lvl == 1) {
        u32 c = var == 0 ? RGB(150, 160, 170) : var == 1 ? RGB(170, 140, 110) : RGB(140, 160, 140);
        Face f = sbox(0.18f, 0.25f, 0.82f, 0.72f, 0, 14, c);
        for (int t = 0; t < f.wl; t += 2) faceRect(f, 0, t, t + 1, 0, 14, cshade(c, 0.88f));
        faceRect(f, 0, 5, 12, 0, 10, RGB(90, 90, 95));
        faceRect(f, 1, 3, 6, 6, 10, RGB(120, 160, 190), LIT1);
        float vm = 0.485f;
        isoQuad(0.15f, 0.22f, 14, 0.85f, 0.22f, 14, 0.85f, vm, 20, 0.15f, vm, 20, cshade(c, 0.7f));
        isoTri(0.82f, 0.25f, 14, 0.82f, 0.72f, 14, 0.82f, vm, 20, cshade(c, 0.7f));
        isoQuad(0.15f, 0.75f, 14, 0.85f, 0.75f, 14, 0.85f, vm, 20, 0.15f, vm, 20, cshade(c, 1.0f));
        for (int k = 0; k < 3; k++) box(0.70f + k * 0.08f, 0.80f, 0.76f + k * 0.08f, 0.88f, 0, 4, RGB(170, 120, 70), RGB(150, 100, 60), RGB(120, 80, 45));
    } else if (lvl == 2) {
        u32 c = var == 1 ? RGB(150, 150, 160) : RGB(156, 84, 62);
        Face f = sbox(0.12f, 0.22f, 0.72f, 0.82f, 0, 22, c);
        windows(f, 0, 12, 1, 8, 4, 4, 7, 3, RGB(120, 160, 190), r, 0.6f);
        windows(f, 1, 12, 1, 8, 4, 4, 7, 3, RGB(120, 160, 190), r, 0.6f);
        faceRect(f, 0, 4, 12, 0, 10, RGB(80, 80, 85));
        flatRoof(0.12f, 0.22f, 0.72f, 0.82f, 22, RGB(110, 105, 100));
        for (int k = 0; k < 3; k++) box(0.2f + k * 0.16f, 0.4f, 0.28f + k * 0.16f, 0.6f, 22, 3, RGB(170, 200, 220), RGB(150, 180, 200), RGB(110, 130, 150));
        chimney(0.86f, 0.30f, 0, 54, 3, true);
    } else if (lvl == 3) {
        u32 c = var == 2 ? RGB(150, 140, 120) : RGB(130, 140, 155);
        Face f = sbox(0.08f, 0.10f, 0.62f, 0.92f, 0, 28, c);
        for (int t = 0; t < f.wl; t += 3) faceRect(f, 0, t, t + 1, 0, 28, cshade(c, 0.86f));
        windows(f, 1, 16, 1, 10, 3, 5, 5, 2, RGB(120, 160, 190), r, 0.7f);
        faceRect(f, 0, 3, 11, 0, 12, RGB(80, 80, 85));
        flatRoof(0.08f, 0.10f, 0.62f, 0.92f, 28, RGB(100, 100, 100));
        cylinder(isoX(0.80f, 0.80f), isoY(0.80f, 0.80f, 0), 7, 16, RGB(225, 225, 220), RGB(200, 200, 195));
        chimney(0.80f, 0.20f, 0, 62, 4, true);
        chimney(0.80f, 0.45f, 0, 52, 3, var != 1);
    } else {
        Face f = sbox(0.10f, 0.10f, 0.58f, 0.90f, 0, 20, RGB(236, 238, 240));
        faceRect(f, 0, 1, f.wl - 1, 6, 14, RGB(90, 150, 200), LIT3);
        faceRect(f, 1, 1, f.wr - 1, 6, 14, RGB(70, 120, 170), LIT3);
        box(0.10f, 0.10f, 0.58f, 0.90f, 20, 0, RGB(220, 222, 225), 0, 0);
        for (int k = 0; k < 4; k++) isoQuad(0.15f, 0.15f + k * 0.18f, 21, 0.53f, 0.15f + k * 0.18f, 21, 0.53f, 0.29f + k * 0.18f, 24, 0.15f, 0.29f + k * 0.18f, 24, RGB(40, 60, 120));
        Face g = box(0.62f, 0.30f, 0.92f, 0.82f, 0, 34, RGB(120, 170, 210), RGB(100, 150, 195), RGB(70, 115, 160));
        windows(g, 0, 0, 4, 8, 3, 5, 4, 1, RGB(170, 210, 240), r, 0.6f, LIT3);
        windows(g, 1, 0, 4, 8, 3, 5, 4, 1, RGB(140, 180, 220), r, 0.6f, LIT3);
        box(0.62f, 0.30f, 0.92f, 0.82f, 34, 0, RGB(200, 205, 210), 0, 0);
        isoQuad(0, 0.9f, 0, 1, 0.9f, 0, 1, 1, 0, 0, 1, 0, RGB(98, 160, 64));
    }
}
static void genConstr() {
    for (int k = 0; k < 3; k++) {
        int hz = k == 0 ? 12 : k == 1 ? 36 : 80;
        SP_CONSTR[k] = newSpr(64, 40 + hz + 20, 32, 40 + hz + 20);
        diamond(RGB(150, 120, 85)); noiseSpr(0.18f, 70 + k);
        u32 conc = RGB(180, 178, 172), sc = RGB(220, 140, 40);
        for (int z = 0; z < hz; z += 8) box(0.15f, 0.15f, 0.85f, 0.85f, z, 2, conc, cshade(conc, 0.9f), cshade(conc, 0.7f));
        const float cu[4] = {0.15f, 0.85f, 0.85f, 0.15f}, cv[4] = {0.15f, 0.15f, 0.85f, 0.85f};
        for (int c = 0; c < 4; c++) { float x = isoX(cu[c], cv[c]), y = isoY(cu[c], cv[c], 0); prect((int)x - 1, (int)y - hz - 2, 2, hz + 2, RGB(150, 150, 150)); }
        for (int z = 4; z < hz; z += 6) { isoLine(0.1f, 0.9f, 0.9f, 0.9f, (float)z, sc); isoLine(0.9f, 0.1f, 0.9f, 0.9f, (float)z, sc); }
        isoLine(0.1f, 0.9f, 0.9f, 0.9f, (float)hz, sc);
        if (k >= 1) {
            float x = isoX(0.2f, 0.25f), y = isoY(0.2f, 0.25f, 0);
            int top = (int)y - hz - 16;
            prect((int)x, top, 2, (int)y - top, RGB(240, 190, 30));
            prect((int)x - 10, top, 34, 2, RGB(240, 190, 30));
            prect((int)x + 18, top + 2, 1, 8, RGB(60, 60, 60));
            addEmit(EM_BEACON, (int)x + 1, top - 1);
        }
    }
    SP_RUBBLE = newSpr(64, 44, 32, 44);
    diamond(RGB(120, 108, 96)); noiseSpr(0.25f, 99);
    Rng r(55);
    for (int k = 0; k < 14; k++) {
        float u = 0.15f + r.f() * 0.7f, v = 0.15f + r.f() * 0.7f;
        u32 c = r.f() < 0.4f ? RGB(50, 45, 42) : (r.f() < 0.5f ? RGB(150, 140, 130) : RGB(130, 90, 70));
        fillEllipse(isoX(u, v), isoY(u, v, 2), 3 + r.f() * 4, 2 + r.f() * 2, c);
    }
    box(0.2f, 0.3f, 0.4f, 0.5f, 0, 9, RGB(110, 100, 95), RGB(95, 85, 80), RGB(70, 62, 58));
}
static void treeAt(float x, float y, int kind, int season, float sc);
static void genServices() {
    Rng r(4242);
    // ветряк
    SP_BLD[B_WIND] = newSpr(64, 96, 32, 96);
    plotGrass();
    {
        float x = isoX(0.5f, 0.5f), y = isoY(0.5f, 0.5f, 0);
        sbox(0.4f, 0.4f, 0.6f, 0.6f, 0, 4, RGB(200, 200, 195));
        for (int z = 0; z < 60; z++) { int w = z < 30 ? 2 : 1; prect((int)x - w, (int)y - z - 4, w * 2, 1, RGB(236, 238, 240)); pset((int)x + w - 1, (int)y - z - 4, RGB(200, 204, 210)); }
        prect((int)x - 3, (int)y - 68, 7, 5, RGB(225, 228, 232)); prect((int)x - 3, (int)y - 64, 7, 1, RGB(180, 185, 190));
        addEmit(EM_ROTOR, (int)x, (int)y - 66);
    }
    // солнечная
    SP_BLD[B_SOLAR] = newSpr(128, 80, 64, 80, 2);
    plotGrass(); noiseSpr(0.06f, 3);
    for (int row = 0; row < 4; row++) for (int col = 0; col < 2; col++) {
        float v0 = 0.15f + row * 0.45f, u0 = 0.12f + col * 0.92f;
        isoQuad(u0, v0, 3, u0 + 0.82f, v0, 3, u0 + 0.82f, v0 + 0.32f, 9, u0, v0 + 0.32f, 9, RGB(36, 58, 120));
        for (float u = u0 + 0.16f; u < u0 + 0.8f; u += 0.164f) isoLine(u, v0, u, v0 + 0.32f, 6, RGB(90, 120, 190));
        isoLine(u0, v0 + 0.32f, u0 + 0.82f, v0 + 0.32f, 9, RGB(150, 180, 230));
    }
    sbox(1.6f, 1.75f, 1.85f, 1.95f, 0, 6, RGB(200, 200, 195));
    // угольная
    SP_BLD[B_COAL] = newSpr(192, 220, 96, 220, 3);
    plotPave(RGB(140, 135, 128));
    {
        chimney(2.45f, 0.45f, 0, 120, 6, true); chimney(2.45f, 1.25f, 0, 104, 5, true);
        Face f = sbox(0.2f, 0.3f, 1.9f, 2.6f, 0, 40, RGB(156, 84, 62));
        windows(f, 0, 20, 2, 9, 3, 6, 6, 4, RGB(120, 160, 190), r, 0.6f);
        windows(f, 1, 20, 2, 9, 3, 6, 6, 4, RGB(120, 160, 190), r, 0.6f);
        faceRect(f, 0, 8, 22, 0, 16, RGB(70, 70, 75));
        flatRoof(0.2f, 0.3f, 1.9f, 2.6f, 40, RGB(100, 96, 92));
        Face g = sbox(1.9f, 1.6f, 2.8f, 2.6f, 0, 26, RGB(150, 155, 165));
        windows(g, 1, 6, 2, 9, 3, 5, 6, 3, RGB(120, 160, 190), r, 0.6f);
        flatRoof(1.9f, 1.6f, 2.8f, 2.6f, 26, RGB(110, 110, 112));
        for (int k = 0; k < 5; k++) fillEllipse(isoX(0.4f + k * 0.12f, 2.82f), isoY(0.4f + k * 0.12f, 2.82f, 3), 7, 4, k & 1 ? RGB(30, 28, 28) : RGB(45, 42, 40));
    }
    // АЭС
    SP_BLD[B_NUCLEAR] = newSpr(192, 200, 96, 200, 3);
    plotPave(RGB(170, 170, 165));
    {
        float cx = isoX(1.0f, 0.9f), cy = isoY(1.0f, 0.9f, 0);
        int H = 92;
        for (int z = 0; z < H; z++) {
            float t = z / (float)H, rr = 30 - 12 * sinf(t * 2.6f) - t * 4;
            for (int x = (int)(cx - rr); x <= (int)(cx + rr); x++) {
                float dx = (x + 0.5f - cx) / rr; if (dx < -1 || dx > 1) continue;
                float sh = 1.05f - 0.35f * (dx + 1) * 0.5f;
                u32 c = cshade(RGB(212, 210, 205), sh);
                if (z > H - 10 && z < H - 6) c = cshade(RGB(200, 70, 60), sh);
                int yb = (int)(cy - z + sqrtf(1 - dx * dx) * rr * 0.5f);
                pset(x, yb, c); pset(x, yb - 1, c);
            }
        }
        float rt = 30 - 12 * sinf(2.6f) - 4;
        fillEllipse(cx, cy - H, rt, rt * 0.5f, RGB(120, 120, 118)); fillEllipse(cx, cy - H + 1, rt - 2, rt * 0.5f - 1, RGB(70, 70, 72));
        addEmit(EM_STEAM, (int)cx, (int)(cy - H));
        Face f = sbox(1.8f, 1.6f, 2.8f, 2.8f, 0, 22, RGB(200, 205, 210));
        windows(f, 0, 6, 1, 10, 3, 5, 6, 3, RGB(90, 130, 170), r, 0.7f);
        flatRoof(1.8f, 1.6f, 2.8f, 2.8f, 22, RGB(150, 150, 150));
        float dx = isoX(0.9f, 2.2f), dy = isoY(0.9f, 2.2f, 0);
        cylinder(dx, dy, 22, 26, RGB(215, 215, 210), RGB(200, 200, 196));
        fillEllipse(dx, dy - 26, 22, 18, RGB(205, 205, 200)); fillEllipse(dx - 6, dy - 34, 8, 6, RGB(235, 235, 230));
        for (int k = 0; k < 3; k++) prect((int)dx - 18 + k * 14, (int)dy - 16, 6, 2, RGB(240, 200, 40));
    }
    // водокачка
    SP_BLD[B_PUMP] = newSpr(64, 64, 32, 64);
    plotPave(RGB(170, 175, 175));
    {
        Face f = sbox(0.25f, 0.25f, 0.70f, 0.70f, 0, 14, RGB(140, 170, 200));
        faceRect(f, 0, 4, 9, 0, 9, RGB(70, 80, 100));
        faceRect(f, 1, 4, 8, 6, 10, RGB(200, 230, 250), LIT3);
        flatRoof(0.25f, 0.25f, 0.70f, 0.70f, 14, RGB(90, 120, 160));
        cylinder(isoX(0.82f, 0.82f), isoY(0.82f, 0.82f, 0), 4, 8, RGB(60, 140, 230), RGB(120, 190, 250));
        isoLine(0.7f, 0.5f, 0.95f, 0.5f, 3, RGB(60, 140, 230)); isoLine(0.7f, 0.5f, 0.95f, 0.5f, 4, RGB(60, 140, 230));
    }
    // водонапорная башня
    SP_BLD[B_WTOWER] = newSpr(64, 96, 32, 96);
    plotGrass();
    {
        float cx = isoX(0.5f, 0.5f), cy = isoY(0.5f, 0.5f, 0);
        for (int k = -1; k <= 1; k += 2) { pline((int)cx + k * 10, (int)cy - 2, (int)cx + k * 6, (int)cy - 36, RGB(120, 120, 125)); pline((int)cx + k * 10 + k, (int)cy - 2, (int)cx + k * 6 + k, (int)cy - 36, RGB(90, 90, 95)); }
        pline((int)cx, (int)cy + 4, (int)cx, (int)cy - 36, RGB(100, 100, 105));
        pline((int)cx - 9, (int)cy - 14, (int)cx + 9, (int)cy - 24, RGB(110, 110, 115)); pline((int)cx + 9, (int)cy - 14, (int)cx - 9, (int)cy - 24, RGB(110, 110, 115));
        cylinder(cx, cy - 36, 13, 16, RGB(90, 150, 210), RGB(140, 190, 230));
        for (int y = 0; y < 8; y++) prect((int)cx - 13 + y * 13 / 8, (int)cy - 52 - y - 6, 26 - 2 * (y * 13 / 8), 1, cshade(RGB(70, 110, 170), 1.0f - y * 0.03f));
        prect((int)cx - 13, (int)cy - 44, 26, 1, RGB(230, 240, 250));
    }
    // очистные
    SP_BLD[B_SEWAGE] = newSpr(128, 80, 64, 80, 2);
    plotPave(RGB(150, 150, 142));
    {
        const float tu[3] = {0.55f, 1.35f, 0.55f}, tv[3] = {0.55f, 0.55f, 1.35f};
        for (int k = 0; k < 3; k++) {
            float cx = isoX(tu[k], tv[k]), cy = isoY(tu[k], tv[k], 0);
            cylinder(cx, cy, 22, 6, RGB(170, 170, 165), RGB(170, 170, 165));
            fillEllipse(cx, cy - 6, 18, 9, k == 1 ? RGB(80, 130, 90) : RGB(110, 100, 60));
            pline((int)cx - 16, (int)cy - 6, (int)cx + 16, (int)cy - 6, RGB(200, 200, 200));
        }
        Face f = sbox(1.25f, 1.25f, 1.85f, 1.85f, 0, 16, RGB(190, 190, 180));
        windows(f, 0, 2, 1, 12, 3, 5, 6, 3, RGB(90, 130, 170), r, 0.6f);
        flatRoof(1.25f, 1.25f, 1.85f, 1.85f, 16, RGB(120, 120, 115));
    }
    // полиция
    SP_BLD[B_POLICE] = newSpr(128, 110, 64, 110, 2);
    plotPave();
    {
        Face f = sbox(0.15f, 0.2f, 1.45f, 1.5f, 0, 30, RGB(220, 225, 232));
        faceRect(f, 0, 0, f.wl, 22, 26, RGB(40, 80, 170)); faceRect(f, 1, 0, f.wr, 22, 26, RGB(30, 60, 130));
        windows(f, 0, 0, 2, 10, 4, 5, 7, 4, RGB(80, 120, 170), r, 0.7f);
        windows(f, 1, 0, 2, 10, 4, 5, 7, 4, RGB(80, 120, 170), r, 0.7f);
        faceRect(f, 0, 18, 26, 0, 10, RGB(60, 70, 90));
        flatRoof(0.15f, 0.2f, 1.45f, 1.5f, 30, RGB(120, 125, 135));
        float x = isoX(0.8f, 0.85f), y = isoY(0.8f, 0.85f, 30);
        prect((int)x - 4, (int)y - 4, 4, 3, RGB(220, 40, 40)); prect((int)x, (int)y - 4, 4, 3, RGB(40, 80, 230));
        addEmit(EM_BEACON, (int)x, (int)y - 5);
        for (int k = 0; k < 3; k++) {
            float u = 1.6f + 0.0f, v = 0.3f + k * 0.45f;
            box(u, v, u + 0.3f, v + 0.16f, 0, 4, RGB(240, 240, 245), RGB(230, 230, 235), RGB(40, 70, 150));
            box(u + 0.08f, v + 0.02f, u + 0.22f, v + 0.14f, 4, 2, RGB(60, 70, 90), RGB(60, 70, 90), RGB(60, 70, 90));
        }
    }
    // пожарная
    SP_BLD[B_FIRE] = newSpr(128, 110, 64, 110, 2);
    plotPave();
    {
        Face t = sbox(0.2f, 0.2f, 0.6f, 0.6f, 0, 54, RGB(170, 70, 55));
        windows(t, 1, 30, 2, 10, 3, 5, 6, 3, RGB(90, 120, 160), r, 0.5f);
        flatRoof(0.2f, 0.2f, 0.6f, 0.6f, 54, RGB(100, 90, 85));
        Face f = sbox(0.3f, 0.6f, 1.7f, 1.5f, 0, 26, RGB(186, 78, 60));
        for (int k = 0; k < 3; k++) {
            int t0 = 4 + k * 14;
            faceRect(f, 0, t0, t0 + 10, 0, 14, RGB(235, 235, 230));
            for (int z = 2; z < 14; z += 3) faceRect(f, 0, t0, t0 + 10, z, z + 1, RGB(200, 200, 195));
        }
        windows(f, 0, 16, 1, 10, 4, 5, 7, 3, RGB(90, 120, 160), r, 0.6f);
        windows(f, 1, 0, 2, 12, 4, 5, 7, 3, RGB(90, 120, 160), r, 0.6f);
        flatRoof(0.3f, 0.6f, 1.7f, 1.5f, 26, RGB(110, 100, 95));
        box(0.4f, 1.62f, 1.0f, 1.86f, 0, 6, RGB(220, 40, 30), RGB(210, 35, 25), RGB(160, 25, 20));
    }
    // больница
    SP_BLD[B_HOSPITAL] = newSpr(128, 130, 64, 130, 2);
    plotPave(RGB(185, 185, 180));
    {
        Face f = sbox(0.15f, 0.15f, 1.2f, 1.85f, 0, 48, RGB(240, 242, 245));
        windows(f, 0, 0, 5, 9, 4, 5, 6, 3, RGB(110, 160, 210), r, 0.7f, LIT3);
        windows(f, 1, 0, 5, 9, 4, 5, 6, 3, RGB(110, 160, 210), r, 0.7f, LIT3);
        flatRoof(0.15f, 0.15f, 1.2f, 1.85f, 48, RGB(200, 200, 200));
        isoQuad(0.55f, 0.85f, 48, 0.8f, 0.85f, 48, 0.8f, 1.15f, 48, 0.55f, 1.15f, 48, RGB(230, 40, 40));
        isoQuad(0.45f, 0.95f, 48, 0.9f, 0.95f, 48, 0.9f, 1.05f, 48, 0.45f, 1.05f, 48, RGB(230, 40, 40));
        Face g = sbox(1.2f, 0.6f, 1.85f, 1.6f, 0, 24, RGB(225, 230, 236));
        windows(g, 1, 0, 2, 10, 4, 5, 6, 3, RGB(110, 160, 210), r, 0.7f, LIT3);
        faceRect(g, 0, 4, 14, 0, 9, RGB(120, 170, 210), LIT2);
        flatRoof(1.2f, 0.6f, 1.85f, 1.6f, 24, RGB(200, 200, 200));
        box(1.3f, 1.62f, 1.75f, 1.85f, 0, 7, RGB(240, 240, 245), RGB(235, 235, 240), RGB(220, 50, 50));
    }
    // школа
    SP_BLD[B_SCHOOL] = newSpr(128, 100, 64, 100, 2);
    plotGrass();
    {
        isoQuad(1.0f, 0.95f, 0, 1.9f, 0.95f, 0, 1.9f, 1.9f, 0, 1.0f, 1.9f, 0, RGB(80, 150, 70));
        isoLine(1.05f, 1.0f, 1.85f, 1.0f, 0, RGB(240, 240, 240)); isoLine(1.05f, 1.85f, 1.85f, 1.85f, 0, RGB(240, 240, 240));
        isoLine(1.05f, 1.0f, 1.05f, 1.85f, 0, RGB(240, 240, 240)); isoLine(1.85f, 1.0f, 1.85f, 1.85f, 0, RGB(240, 240, 240));
        isoLine(1.45f, 1.0f, 1.45f, 1.85f, 0, RGB(240, 240, 240));
        u32 c = RGB(232, 168, 80);
        Face f = sbox(0.1f, 0.1f, 1.9f, 0.85f, 0, 22, c);
        windows(f, 0, 0, 2, 11, 5, 6, 8, 3, RGB(90, 140, 190), r, 0.4f);
        windows(f, 1, 0, 2, 11, 5, 6, 8, 3, RGB(90, 140, 190), r, 0.4f);
        flatRoof(0.1f, 0.1f, 1.9f, 0.85f, 22, RGB(150, 80, 60));
        Face g = sbox(0.1f, 0.85f, 0.85f, 1.9f, 0, 22, c);
        windows(g, 0, 0, 2, 11, 5, 6, 8, 3, RGB(90, 140, 190), r, 0.4f);
        faceRect(g, 1, 4, 10, 0, 9, RGB(110, 70, 40));
        flatRoof(0.1f, 0.85f, 0.85f, 1.9f, 22, RGB(150, 80, 60));
    }
    // университет
    SP_BLD[B_UNIV] = newSpr(192, 170, 96, 170, 3);
    plotGrass();
    {
        isoQuad(1.3f, 1.3f, 0, 1.7f, 1.3f, 0, 1.7f, 3.0f, 0, 1.3f, 3.0f, 0, RGB(200, 190, 170));
        isoQuad(1.3f, 1.3f, 0, 3.0f, 1.3f, 0, 3.0f, 1.7f, 0, 1.3f, 1.7f, 0, RGB(200, 190, 170));
        u32 c = RGB(226, 212, 180);
        Face f = sbox(0.15f, 0.15f, 1.3f, 2.85f, 0, 36, c);
        windows(f, 1, 0, 3, 12, 4, 7, 8, 4, RGB(90, 120, 160), r, 0.5f);
        windows(f, 0, 0, 3, 12, 4, 7, 8, 4, RGB(90, 120, 160), r, 0.5f);
        for (int t = 6; t < f.wr - 4; t += 7) faceRect(f, 1, t, t + 3, 0, 32, RGB(250, 248, 240));
        flatRoof(0.15f, 0.15f, 1.3f, 2.85f, 36, RGB(160, 150, 130));
        float cx = isoX(0.72f, 1.5f), cy = isoY(0.72f, 1.5f, 36);
        cylinder(cx, cy, 16, 10, RGB(220, 210, 185), RGB(200, 190, 165));
        fillEllipse(cx, cy - 12, 16, 15, RGB(110, 150, 140)); fillEllipse(cx - 5, cy - 18, 6, 5, RGB(150, 190, 175));
        prect((int)cx, (int)cy - 32, 1, 6, RGB(200, 180, 60));
        Face g = sbox(1.8f, 0.2f, 2.8f, 1.2f, 0, 24, RGB(190, 110, 90));
        windows(g, 0, 0, 2, 12, 4, 6, 7, 3, RGB(90, 120, 160), r, 0.5f);
        windows(g, 1, 0, 2, 12, 4, 6, 7, 3, RGB(90, 120, 160), r, 0.5f);
        flatRoof(1.8f, 0.2f, 2.8f, 1.2f, 24, RGB(130, 80, 70));
        for (int k = 0; k < 3; k++) treeAt(isoX(2.3f + k * 0.2f, 2.3f - k * 0.3f), isoY(2.3f + k * 0.2f, 2.3f - k * 0.3f, 0), k & 1, 1, 1.0f);
    }
    // мэрия
    SP_BLD[B_CITYHALL] = newSpr(128, 140, 64, 140, 2);
    plotPave(RGB(200, 192, 176));
    {
        u32 c = RGB(232, 222, 196);
        Face f = sbox(0.2f, 0.2f, 1.5f, 1.8f, 0, 30, c);
        windows(f, 0, 0, 2, 13, 4, 7, 7, 4, RGB(90, 120, 160), r, 0.6f);
        windows(f, 1, 0, 2, 13, 4, 7, 7, 4, RGB(90, 120, 160), r, 0.6f);
        for (int t = 4; t < f.wr - 3; t += 6) faceRect(f, 1, t, t + 2, 0, 28, RGB(250, 248, 242));
        flatRoof(0.2f, 0.2f, 1.5f, 1.8f, 30, RGB(150, 110, 90));
        Face t = sbox(0.6f, 0.75f, 1.1f, 1.25f, 30, 34, c);
        float cx = t.fx + 7, cy = (float)t.fy - 3 - 3 - 24;
        fillEllipse(cx, cy, 4, 4, RGB(250, 250, 245)); pset((int)cx, (int)cy - 2, RGB(30, 30, 30)); pset((int)cx + 1, (int)cy, RGB(30, 30, 30));
        float px = isoX(0.85f, 1.0f), py = isoY(0.85f, 1.0f, 64);
        for (int y = 0; y < 8; y++) prect((int)px - 8 + y, (int)py - y, 16 - 2 * y, 1, RGB(170, 60, 50));
        prect((int)px, (int)py - 22, 1, 14, RGB(90, 90, 90));
        addEmit(EM_FLAG, (int)px + 1, (int)py - 22);
    }
    // сквер
    SP_BLD[B_PARKS] = newSpr(64, 64, 32, 64);
    plotGrass(); noiseSpr(0.1f, 5);
    isoQuad(0.44f, 0, 0, 0.56f, 0, 0, 0.56f, 1, 0, 0.44f, 1, 0, RGB(205, 190, 160));
    isoQuad(0, 0.44f, 0, 1, 0.44f, 0, 1, 0.56f, 0, 0, 0.56f, 0, RGB(205, 190, 160));
    box(0.15f, 0.62f, 0.38f, 0.70f, 0, 2, RGB(140, 90, 50), RGB(120, 80, 45), RGB(100, 65, 35));
    treeAt(isoX(0.22f, 0.22f), isoY(0.22f, 0.22f, 0), 0, 1, 0.9f);
    treeAt(isoX(0.78f, 0.78f), isoY(0.78f, 0.78f, 0), 1, 1, 1.0f);
    for (int k = 0; k < 8; k++) pset((int)isoX(0.7f + (k % 3) * 0.08f, 0.2f + (k / 3) * 0.06f), (int)isoY(0.7f + (k % 3) * 0.08f, 0.2f + (k / 3) * 0.06f, 1), k & 1 ? RGB(250, 80, 90) : RGB(250, 220, 60));
    // парк
    SP_BLD[B_PARKL] = newSpr(128, 84, 64, 84, 2);
    plotGrass(); noiseSpr(0.1f, 6);
    {
        fillEllipse(isoX(0.6f, 0.6f), isoY(0.6f, 0.6f, 0), 26, 12, RGB(120, 100, 70));
        fillEllipse(isoX(0.6f, 0.6f), isoY(0.6f, 0.6f, 0), 24, 11, RGB(70, 140, 200));
        fillEllipse(isoX(0.6f, 0.6f) - 6, isoY(0.6f, 0.6f, 0) - 3, 6, 2, RGB(140, 200, 240));
        isoQuad(1.0f, 0, 0, 1.12f, 0, 0, 1.12f, 2, 0, 1.0f, 2, 0, RGB(205, 190, 160));
        isoQuad(0, 1.3f, 0, 2, 1.3f, 0, 2, 1.42f, 0, 0, 1.42f, 0, RGB(205, 190, 160));
        Face g = sbox(1.45f, 0.4f, 1.75f, 0.7f, 0, 8, RGB(240, 235, 220)); (void)g;
        isoTri(1.40f, 0.35f, 8, 1.80f, 0.35f, 8, 1.6f, 0.55f, 16, RGB(160, 70, 55));
        isoTri(1.80f, 0.35f, 8, 1.80f, 0.75f, 8, 1.6f, 0.55f, 16, RGB(130, 55, 45));
        isoTri(1.40f, 0.75f, 8, 1.80f, 0.75f, 8, 1.6f, 0.55f, 16, RGB(180, 80, 62));
        const float tu[7] = {0.2f, 0.4f, 1.6f, 1.8f, 0.3f, 1.5f, 1.85f}, tv[7] = {1.7f, 1.85f, 1.7f, 1.85f, 1.15f, 1.0f, 1.1f};
        for (int k = 0; k < 7; k++) treeAt(isoX(tu[k], tv[k]), isoY(tu[k], tv[k], 0), k % 3 == 2 ? 2 : k & 1, 1, 1.0f);
    }
    // фонтан
    SP_BLD[B_FOUNTAIN] = newSpr(64, 64, 32, 64);
    plotPave(RGB(200, 190, 170));
    {
        float cx = isoX(0.5f, 0.5f), cy = isoY(0.5f, 0.5f, 0);
        cylinder(cx, cy, 20, 4, RGB(190, 185, 175), RGB(190, 185, 175));
        fillEllipse(cx, cy - 4, 17, 8, RGB(80, 150, 210)); fillEllipse(cx - 5, cy - 6, 4, 1.5f, RGB(180, 220, 250));
        prect((int)cx - 1, (int)cy - 16, 3, 12, RGB(170, 165, 160));
        for (int k = 0; k < 12; k++) { float a = k * 0.52f; pset((int)(cx + cosf(a) * (4 + k % 3)), (int)(cy - 16 + fabsf(sinf(a)) * 6 - 2), RGB(220, 240, 255)); }
        fillEllipse(cx, cy - 18, 3, 2, RGB(220, 240, 255));
        for (int k = 0; k < 10; k++) pset((int)isoX(0.08f + k * 0.03f, 0.9f), (int)isoY(0.08f + k * 0.03f, 0.9f, 1), k & 1 ? RGB(240, 80, 100) : RGB(250, 200, 50));
    }
    // стадион
    SP_BLD[B_STADIUM] = newSpr(192, 140, 96, 140, 3);
    plotPave(RGB(170, 170, 165));
    {
        float cx = isoX(1.5f, 1.5f), cy = isoY(1.5f, 1.5f, 0);
        for (int z = 0; z < 26; z++) {
            float rx = 84 - z * 0.6f;
            u32 c = (z / 3) & 1 ? RGB(200, 200, 205) : RGB(180, 60, 60);
            if (z < 6) c = RGB(170, 170, 175);
            fillEllipse(cx, cy - z, rx, rx * 0.5f, cshade(c, 0.9f + z * 0.006f));
        }
        for (int z = 26; z > 6; z--) {
            float rx = 70 - (26 - z) * 1.4f;
            u32 c = ((z / 2) & 1) ? RGB(150, 60, 60) : RGB(110, 110, 120);
            fillEllipse(cx, cy - 26 + (26 - z) * 0.6f, rx, rx * 0.5f, c);
        }
        fillEllipse(cx, cy - 14, 40, 20, RGB(70, 150, 60));
        for (int k = -3; k <= 3; k++) fillEllipse(cx + k * 11, cy - 14, 5, 18 - abs(k) * 2, (k & 1) ? RGB(80, 165, 70) : RGB(70, 150, 60));
        pline((int)cx, (int)cy - 32, (int)cx, (int)cy + 4, RGB(240, 240, 240));
        fillEllipse(cx, cy - 14, 6, 3, RGB(240, 240, 240)); fillEllipse(cx, cy - 14, 5, 2, RGB(70, 150, 60));
        const float lx[4] = {-80, 80, -40, 40}, ly[4] = {-6, -6, -40, 30};
        for (int k = 0; k < 4; k++) {
            prect((int)(cx + lx[k]), (int)(cy + ly[k] - 50), 2, 50, RGB(140, 140, 145));
            prect((int)(cx + lx[k]) - 3, (int)(cy + ly[k] - 54), 8, 4, RGB(230, 230, 220));
            for (int x = 0; x < 8; x++) for (int y = 0; y < 4; y++) lset((int)(cx + lx[k]) - 3 + x, (int)(cy + ly[k] - 54) + y, RGB(255, 255, 230));
        }
    }
    // телебашня
    SP_BLD[B_TVTOWER] = newSpr(128, 330, 64, 330, 2);
    plotPave(RGB(185, 185, 180));
    {
        Face f = sbox(0.5f, 0.5f, 1.5f, 1.5f, 0, 14, RGB(210, 212, 216));
        faceRect(f, 0, 2, f.wl - 2, 3, 10, RGB(110, 160, 210), LIT2);
        faceRect(f, 1, 2, f.wr - 2, 3, 10, RGB(100, 150, 200), LIT2);
        float cx = isoX(1.0f, 1.0f), cy = isoY(1.0f, 1.0f, 14);
        for (int z = 0; z < 210; z++) {
            float w = 9 - z * 0.025f;
            u32 c = (z > 190 && ((z / 6) & 1)) ? RGB(220, 70, 60) : RGB(232, 232, 236);
            for (int x = (int)(cx - w); x <= (int)(cx + w); x++) pset(x, (int)cy - z, cshade(c, x < cx ? 1.0f : 0.78f));
        }
        for (int k = 0; k < 3; k++) {
            float z = 150 + k * 6.0f, rr = k == 1 ? 22.0f : 18.0f;
            fillEllipse(cx, cy - z, rr, rr * 0.5f, k == 1 ? RGB(80, 110, 150) : RGB(200, 200, 205));
            if (k == 1) for (int x = -20; x <= 20; x += 3) lset((int)cx + x, (int)(cy - z), LIT3);
        }
        prect((int)cx - 1, (int)cy - 260, 2, 52, RGB(200, 200, 205));
        addEmit(EM_BEACON, (int)cx, (int)cy - 261); addEmit(EM_BEACON, (int)cx - 20, (int)cy - 157); addEmit(EM_BEACON, (int)cx + 20, (int)cy - 157);
    }
}
// деревья: kind 0 — большое лиственное, 1 — малое, 2 — ель, 3 — малая ель. Сезоны: весна, лето, осень, зима.
static void treeAt(float x, float y, int kind, int season, float sc) {
    fillEllipse(x + 1, y, 7 * sc, 3 * sc, ARGB(70, 20, 30, 10));
    if (kind < 2) {
        float R = (kind == 0 ? 8.5f : 6.0f) * sc, cy = y - (kind == 0 ? 15 : 11) * sc;
        prect((int)x - 1, (int)(cy + R * 0.3f), 2, (int)(y - cy - R * 0.3f) + 1, RGB(100, 70, 45));
        if (season == 3) {
            u32 b = RGB(110, 80, 60);
            pline((int)x, (int)(cy + R * 0.3f), (int)(x - R * 0.7f), (int)(cy - R * 0.5f), b);
            pline((int)x, (int)(cy + R * 0.3f), (int)(x + R * 0.7f), (int)(cy - R * 0.6f), b);
            pline((int)x, (int)(cy + R * 0.3f), (int)x, (int)(cy - R), b);
            pline((int)(x - R * 0.35f), (int)(cy - R * 0.1f), (int)(x - R * 0.6f), (int)(cy - R * 0.9f), b);
            return;
        }
        u32 base = season == 0 ? RGB(110, 180, 70) : season == 1 ? RGB(66, 135, 52) : (((int)(x * 7 + y * 3)) % 3 == 0 ? RGB(210, 110, 40) : ((int)(x + y) % 2 ? RGB(220, 170, 50) : RGB(190, 70, 40)));
        fillEllipse(x, cy, R, R * 0.9f, cshade(base, 0.62f));
        fillEllipse(x - R * 0.12f, cy - R * 0.1f, R * 0.88f, R * 0.78f, base);
        fillEllipse(x - R * 0.35f, cy - R * 0.35f, R * 0.45f, R * 0.38f, cshade(base, 1.22f));
        if (season == 0) for (int k = 0; k < 5; k++) pset((int)(x - R * 0.6f + (k * 37 % 11) * R * 0.11f), (int)(cy - R * 0.5f + (k * 53 % 7) * R * 0.14f), RGB(250, 200, 220));
    } else {
        float Hh = (kind == 2 ? 26 : 17) * sc, W = (kind == 2 ? 8 : 6) * sc;
        prect((int)x - 1, (int)(y - 4 * sc), 2, (int)(4 * sc) + 1, RGB(90, 60, 40));
        u32 g = RGB(40, 100, 60);
        for (int k = 0; k < 3; k++) {
            float yb = y - 3 * sc - k * Hh * 0.27f, yt = yb - Hh * 0.45f, w = W * (1.0f - k * 0.22f);
            float xs[3] = {x - w, x + w, x}, ys[3] = {yb, yb, yt};
            fillPoly(xs, ys, 3, cshade(g, 0.85f + k * 0.1f));
            float xs2[3] = {x - w * 0.8f, x, x}, ys2[3] = {yb - 1, yb - 1, yt + 1};
            fillPoly(xs2, ys2, 3, cshade(g, 1.15f + k * 0.08f));
            if (season == 3) { float xs3[3] = {x - w * 0.5f, x + w * 0.5f, x}, ys3[3] = {yt + (yb - yt) * 0.5f, yt + (yb - yt) * 0.5f, yt}; fillPoly(xs3, ys3, 3, RGB(240, 244, 250)); }
        }
    }
}
static void genObjects() {
    for (int k = 0; k < 4; k++) for (int s = 0; s < 4; s++) {
        SP_TREE[k][s] = newSpr(24, 40, 12, 36);
        treeAt(12, 36, k, s, 1.0f);
    }
    const u32 CC[6] = {RGB(210, 50, 45), RGB(50, 90, 200), RGB(240, 240, 235), RGB(240, 190, 40), RGB(60, 150, 80), RGB(60, 60, 70)};
    for (int k = 0; k < 8; k++) for (int ax = 0; ax < 2; ax++) {
        SP_CAR[k][ax] = newSpr(64, 40, 32, 24);
        cAY = 40; // рисуем в рамке клетки, якорь в центре клетки
        float L = k == 7 ? 0.22f : (k == 6 ? 0.18f : 0.14f), W = 0.07f;
        auto B = [&](float l0, float l1, float w0, float w1, int z0, int hz, u32 c) {
            if (ax == 0) return box(0.5f + l0, 0.5f + w0, 0.5f + l1, 0.5f + w1, z0, hz, cshade(c, 1.1f), cshade(c, 0.95f), cshade(c, 0.75f));
            return box(0.5f + w0, 0.5f + l0, 0.5f + w1, 0.5f + l1, z0, hz, cshade(c, 1.1f), cshade(c, 0.95f), cshade(c, 0.75f));
        };
        if (k < 6) {
            B(-L, L, -W, W, 1, 4, CC[k]);
            B(-L * 0.45f, L * 0.4f, -W + 0.01f, W - 0.01f, 5, 3, RGB(60, 80, 110));
            B(-L * 0.4f, L * 0.35f, -W + 0.015f, W - 0.015f, 8, 0, cshade(CC[k], 1.05f));
        } else if (k == 6) {
            B(-L, L * 0.2f, -W, W, 1, 9, RGB(235, 235, 230));
            B(L * 0.25f, L, -W, W, 1, 6, RGB(200, 70, 50));
            B(L * 0.3f, L * 0.75f, -W + 0.01f, W - 0.01f, 7, 2, RGB(60, 80, 110));
        } else {
            Face f = B(-L, L, -W, W, 1, 9, RGB(240, 200, 50));
            (void)f;
            B(-L + 0.01f, L - 0.01f, -W, W, 5, 3, RGB(70, 90, 120));
            B(-L, L, -W, W, 10, 0, RGB(235, 235, 230));
        }
        // фары (ночной слой)
        float fx = ax == 0 ? isoX(0.5f + L, 0.5f) : isoX(0.5f, 0.5f + L), fy = ax == 0 ? isoY(0.5f + L, 0.5f, 3) : isoY(0.5f, 0.5f + L, 3);
        lset((int)fx, (int)fy, RGB(255, 250, 200)); lset((int)fx - 1, (int)fy, RGB(255, 250, 200));
        float bx = ax == 0 ? isoX(0.5f - L, 0.5f) : isoX(0.5f, 0.5f - L), by = ax == 0 ? isoY(0.5f - L, 0.5f, 3) : isoY(0.5f, 0.5f - L, 3);
        lset((int)bx, (int)by, RGB(255, 60, 40));
        S[SP_CAR[k][ax]].ay = 24;
    }
    const u32 PC[4] = {RGB(200, 60, 60), RGB(60, 90, 190), RGB(240, 200, 60), RGB(80, 160, 90)};
    for (int k = 0; k < 4; k++) {
        SP_PERSON[k] = newSpr(3, 8, 1, 7);
        prect(0, 0, 3, 2, RGB(240, 200, 170)); prect(0, 2, 3, 3, PC[k]); pset(0, 5, RGB(50, 50, 60)); pset(2, 5, RGB(50, 50, 60)); pset(0, 6, RGB(50, 50, 60)); pset(2, 6, RGB(50, 50, 60));
    }
    SP_PUFF = newSpr(16, 16, 8, 8);
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        float d = sqrtf((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f)) / 8.0f;
        if (d < 1) pset(x, y, ARGB((int)(200 * (1 - d * d)), 235, 235, 235));
    }
    for (int f = 0; f < 3; f++) {
        SP_FLAME[f] = newSpr(16, 22, 8, 21);
        Rng r(700 + f);
        for (int k = 0; k < 5; k++) {
            float x = 4 + r.f() * 8, h = 10 + r.f() * 10, w = 2 + r.f() * 2.5f;
            float xs[3] = {x - w, x + w, x + (r.f() - 0.5f) * 3}, ys[3] = {21, 21, 21 - h};
            fillPoly(xs, ys, 3, RGB(230, 90, 30));
            float xs2[3] = {x - w * 0.5f, x + w * 0.5f, x}, ys2[3] = {21, 21, 21 - h * 0.6f};
            fillPoly(xs2, ys2, 3, RGB(255, 210, 60));
        }
        for (int y = 0; y < 22; y++) for (int x = 0; x < 16; x++) if (pget(x, y) >> 24) lset(x, y, cshade(pget(x, y), 0.8f));
    }
    SP_GLOW = newSpr(48, 24, 24, 12);
    for (int y = 0; y < 24; y++) for (int x = 0; x < 48; x++) {
        float d = sqrtf((x - 23.5f) * (x - 23.5f) / 576.0f + (y - 11.5f) * (y - 11.5f) / 144.0f);
        if (d < 1) pset(x, y, ARGB((int)(255 * (1 - d) * (1 - d)), 255, 200, 110));
    }
    SP_ROTOR = newSpr(48, 48, 24, 24);
    for (int b = 0; b < 3; b++) {
        float a = b * 2.0944f - 1.5708f;
        for (int k = 3; k < 23; k++) {
            float w = k < 8 ? 1.6f : 1.1f;
            float x = 24 + cosf(a) * k, y = 24 + sinf(a) * k;
            fillEllipse(x, y, w, w, RGB(245, 245, 248));
        }
    }
    fillEllipse(24, 24, 2.5f, 2.5f, RGB(200, 200, 205));
    SP_BEACON = newSpr(5, 5, 2, 2);
    for (int y = 0; y < 5; y++) for (int x = 0; x < 5; x++) {
        float d = sqrtf((x - 2.0f) * (x - 2.0f) + (y - 2.0f) * (y - 2.0f));
        if (d < 2.6f) pset(x, y, ARGB(d < 1.2f ? 255 : 140, 255, 60, 50));
    }
    for (int i = 0; i < IC_COUNT; i++) {
        SP_ICON[i] = newSpr(12, 12, 0, 0);
        for (int y = 0; y < 12; y++) { const char* row = ICONS[i][y]; int n = (int)strlen(row); for (int x = 0; x < 12 && x < n; x++) pset(x, y, iconPal(row[x])); }
    }
    for (int g = 0; g < NGLYPHS && g < 128; g++) {
        SP_FONT[g] = newSpr(5, 7, 0, 0);
        for (int y = 0; y < 7; y++) for (int x = 0; x < 5; x++) if (GLYPHS[g].rows[y][x] == '#') pset(x, y, RGB(255, 255, 255));
    }
}
static void genAllSprites() {
    S.clear(); SMETA.clear(); pages.clear(); shX = shY = shH = 0;
    genTerrain(); genRoads();
    for (int lvl = 1; lvl <= 5; lvl++) for (int v = 0; v < 3; v++) { genGrowR(lvl, v); genGrowC(lvl, v); genGrowI(lvl, v); }
    genConstr(); genServices(); genObjects();
    SP_BLD[B_RUBBLE] = SP_RUBBLE;
}

// ---------------------------------------------------------------------
//  Генерация карты
// ---------------------------------------------------------------------
static float vnoise(float x, float y, u32 seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi;
    auto h = [&](int a, int b) { return (hash32(hash2(a, b) ^ seed) & 0xFFFF) / 65535.0f; };
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    return lerpf(lerpf(h(xi, yi), h(xi + 1, yi), sx), lerpf(h(xi, yi + 1), h(xi + 1, yi + 1), sx), sy);
}
static float fbm(float x, float y, u32 seed, int oct) {
    float s = 0, a = 0.5f, f = 1, n = 0;
    for (int i = 0; i < oct; i++) { s += a * vnoise(x * f, y * f, seed + i * 1013); n += a; a *= 0.5f; f *= 2; }
    return s / n;
}
static void clearSimMaps() {
    memset(cov, 0, sizeof cov); memset(pollution, 0, sizeof pollution); memset(landv, 0, sizeof landv);
    memset(crime, 0, sizeof crime); memset(traffic, 0, sizeof traffic); memset(access_, 0, sizeof access_); memset(wdist, 0, sizeof wdist);
}
// типы местности для новой карты
enum { MT_RIVER, MT_LAKE, MT_SEA, MT_HILLS, MT_PLAIN, MT_COUNT };
static const char* MT_NAME[MT_COUNT] = {"У РЕКИ", "У ОЗЕРА", "У МОРЯ", "В ГОРАХ", "РАВНИНА"};
static void carveRiver(Rng& r, float wmin, float wmax, int fromSide) {
    // fromSide: 0 — сверху вниз (по y), 1 — слева направо (по x)
    bool vertical = fromSide == 0;
    float pos = MAPN * (0.3f + r.f() * 0.4f), dir = 0;
    float w = wmin + r.f() * (wmax - wmin);
    for (int k = 0; k < MAPN; k++) {
        dir += (r.f() - 0.5f) * 0.35f; dir = clampf(dir, -0.9f, 0.9f); pos += dir;
        if (pos < 10) dir += 0.2f; if (pos > MAPN - 10) dir -= 0.2f;
        for (int d = -(int)ceilf(w) - 1; d <= (int)ceilf(w) + 1; d++) {
            int a = (int)lroundf(pos) + d;
            int x = vertical ? a : k, y = vertical ? k : a;
            if (!inMap(x, y)) continue;
            Tile& t = TL(x, y);
            if (fabsf((float)d) <= w) { t.ter = T_WATER; t.h = 0; }
            else if (t.ter != T_WATER) t.h = 1;
        }
    }
}
static void generateMap(u32 seed, int type = MT_RIVER) {
    memset(map_, 0, sizeof map_);
    Rng r(seed);
    float ofx = r.f() * 1000, ofy = r.f() * 1000;
    float waterT = type == MT_PLAIN ? 0.22f : type == MT_LAKE ? 0.26f : type == MT_HILLS ? 0.27f : 0.30f;
    float h2 = type == MT_HILLS ? 0.46f : type == MT_PLAIN ? 0.70f : 0.58f;
    float hstep = type == MT_HILLS ? 0.06f : 0.075f;
    // озеро: центр, море: сторона карты
    float lx = MAPN * (0.35f + r.f() * 0.3f), ly = MAPN * (0.35f + r.f() * 0.3f), lr = MAPN * (0.17f + r.f() * 0.06f);
    int seaSide = r.i(4);
    for (int y = 0; y < MAPN; y++) for (int x = 0; x < MAPN; x++) {
        Tile& t = TL(x, y);
        float e = fbm(x / 38.0f + ofx, y / 38.0f + ofy, seed, 5);
        float hill = fbm(x / 22.0f + ofx + 50, y / 22.0f + ofy, seed + 7, 4);
        if (type == MT_LAKE) {
            float d = sqrtf((x - lx) * (x - lx) + (y - ly) * (y - ly)) / lr;
            e += clampf(d - 1.0f, -1, 0.6f) * 0.35f;              // впадина в центре
            if (d > 1.3f) e = std::max(e, waterT + 0.02f);         // вдали от озера — суша
        } else if (type == MT_SEA) {
            float c = seaSide == 0 ? x : seaSide == 1 ? MAPN - 1 - x : seaSide == 2 ? y : MAPN - 1 - y;
            float edge = MAPN * 0.28f + (fbm(x / 20.0f + ofy, y / 20.0f + ofx, seed + 31, 3) - 0.5f) * 30.0f;
            e = c < edge ? 0.0f : std::max(e, waterT + 0.02f + (c - edge < 3 ? 0 : 0.0f));
            if (c - edge > 4 && e < waterT) e = waterT + 0.02f;   // без случайных луж на суше
        } else if (type == MT_RIVER && e < waterT) e = waterT + 0.02f; // вода — только сама река
        if (e < waterT) { t.ter = T_WATER; t.h = 0; }
        else {
            t.ter = T_GRASS;
            int h = 1;
            if (hill > h2) h = 2; if (hill > h2 + hstep) h = 3; if (hill > h2 + 2 * hstep) h = 4;
            if (type == MT_HILLS && hill > h2 + 3 * hstep) h = 5;
            t.h = (u8)h;
        }
        t.var = (u8)(hash2(x, y) % 3);
    }
    if (type == MT_RIVER) { carveRiver(r, 2.0f, 3.2f, r.i(2)); if (r.f() < 0.5f) carveRiver(r, 1.0f, 1.6f, r.i(2)); }
    else if (type == MT_HILLS) carveRiver(r, 1.2f, 2.0f, r.i(2));
    else if (type == MT_SEA) carveRiver(r, 1.4f, 2.2f, (seaSide < 2) ? 1 : 0);
    // берега: высота 1, песок
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < MAPN; y++) for (int x = 0; x < MAPN; x++) {
            Tile& t = TL(x, y); if (t.ter == T_WATER) continue;
            int nw = 0;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (inMap(x + dx, y + dy) && TL(x + dx, y + dy).ter == T_WATER) nw++;
            if (nw) {
                t.h = 1;
                if (pass == 0 && (hash2(x * 3, y * 5 + seed) % 100) < 70) t.ter = T_SAND;
            }
        }
    // сглаживание: разница высот соседей не больше 1
    for (int it = 0; it < 4; it++)
        for (int y = 0; y < MAPN; y++) for (int x = 0; x < MAPN; x++) {
            Tile& t = TL(x, y); if (t.ter == T_WATER) continue;
            int mn = 9;
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; d++) if (inMap(x + dx[d], y + dy[d])) { Tile& n = TL(x + dx[d], y + dy[d]); mn = std::min(mn, n.ter == T_WATER ? 1 : (int)n.h); }
            if (t.h > mn + 1) t.h = (u8)(mn + 1);
        }
    // лес
    for (int y = 0; y < MAPN; y++) for (int x = 0; x < MAPN; x++) {
        Tile& t = TL(x, y); if (t.ter != T_GRASS) continue;
        float f = fbm(x / 14.0f + ofy, y / 14.0f + ofx, seed + 99, 3);
        float ft = type == MT_HILLS ? 0.50f : type == MT_PLAIN ? 0.60f : 0.56f;
        if (f > ft) t.tree = (u8)(1 + std::min(2, (int)((f - ft) * 20)));
        else if ((hash2(x + 7, y * 3 + seed) % 100) < 3) t.tree = 1;
    }
    clearSimMaps();
}
// автоматический выбор места: ровная суша, рядом вода, ближе к центру
static int startMarkX = -1, startMarkY = -1; // отметка выбранного места до первой постройки
static void findStartSpot(int& sx, int& sy) {
    int best = -1000000; sx = MAPN / 2; sy = MAPN / 2;
    for (int y = 10; y < MAPN - 10; y += 2) for (int x = 10; x < MAPN - 10; x += 2) {
        int flat = 0, water = 0;
        for (int j = -6; j <= 6; j++) for (int i = -6; i <= 6; i++) { const Tile& t = TL(x + i, y + j); flat += t.ter != T_WATER && t.h == 1 && !t.tree; flat += t.ter != T_WATER && t.h == 1; }
        for (int j = -10; j <= 10; j += 2) for (int i = -10; i <= 10; i += 2) water += TL(x + i, y + j).ter == T_WATER;
        int d = abs(x - MAPN / 2) + abs(y - MAPN / 2);
        int s = flat * 3 + (water > 0 && water < 40 ? 60 + water : -40) - d;
        if (TL(x, y).ter == T_WATER) s -= 1000;
        if (s > best) { best = s; sx = x; sy = y; }
    }
}

// ---------------------------------------------------------------------
//  Симуляция
// ---------------------------------------------------------------------
static const int PWR_NEED[6] = {0, 1, 2, 5, 10, 20};
static const int WAT_NEED[6] = {0, 1, 2, 5, 10, 20};
static const int MONTH_DAYS = 10;
static inline int monthIndex() { return city.day / MONTH_DAYS; }
static bool seasonsOn = true;
static inline int season() { if (!seasonsOn) return 1; int m = monthIndex() % 12; return (m < 2 || m == 11) ? 3 : m < 5 ? 0 : m < 8 ? 1 : 2; }
static double gameClock = 0; // реальные секунды
static const char* SEASON_NAME[4] = {"ВЕСНА", "ЛЕТО", "ОСЕНЬ", "ЗИМА"};
static const char* MONTH_NAME[12] = {"ЯНВ", "ФЕВ", "МАР", "АПР", "МАЙ", "ИЮН", "ИЮЛ", "АВГ", "СЕН", "ОКТ", "НОЯ", "ДЕК"};

struct Msg { std::string text; float t; u32 col; };
static std::vector<Msg> msgs;
static void notify(const std::string& s, u32 col = RGB(255, 255, 255)) { msgs.push_back(Msg{s, 4.0f, col}); if (msgs.size() > 4) msgs.erase(msgs.begin()); }
enum { SND_CLICK, SND_BUILD, SND_BULL, SND_ERR, SND_COIN, SND_FIRE, SND_ACH };
static void playSound(int s);

static inline bool hasRoad(int x, int y) { return inMap(x, y) && TL(x, y).road; }
static inline int roadMask(int x, int y) {
    const Tile& t = TL(x, y); int m = 0, h = effH(t);
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++) {
        int nx = x + dx[d], ny = y + dy[d];
        if (inMap(nx, ny) && TL(nx, ny).road && abs(effH(TL(nx, ny)) - h) <= 1) m |= 1 << d;
    }
    return m;
}
static bool isPowerProducer(int bt) { return bt == B_WIND || bt == B_SOLAR || bt == B_COAL || bt == B_NUCLEAR; }
// проводят ток: ЛЭП, зоны, здания и дороги (кабель вдоль дорог)
static bool isConductor(const Tile& t) { return (t.flags & F_PLINE) || t.zone || t.road || (t.bt != B_NONE && t.bt != B_RUBBLE); }
static int tileOriginIdx(int x, int y) { const Tile& t = TL(x, y); return (y + t.oy) * MAPN + (x + t.ox); }

static int qbuf[NT];
static u8 visit[NT];
static int pwrNeedOf(const Tile& t) {
    if (t.flags & F_FIRE) return 0;
    if (t.bt == B_GROW) return (t.flags & F_ABAND) ? 0 : PWR_NEED[t.level];
    if (t.bt > B_RUBBLE && isOrigin(t) && BD[t.bt].power < 0) return -BD[t.bt].power;
    return 0;
}
static int watNeedOf(const Tile& t) {
    if (t.bt == B_GROW) return (t.flags & F_ABAND) ? 0 : WAT_NEED[t.level];
    if (t.bt >= B_POLICE && isOrigin(t)) return 3;
    return 0;
}
static void stagePower() {
    memset(visit, 0, sizeof visit);
    for (int i = 0; i < NT; i++) map_[i].flags &= ~F_POWER;
    int totalCap = 0, totalUse = 0, totalNeed = 0;
    for (int i = 0; i < NT; i++) totalNeed += pwrNeedOf(map_[i]);
    for (int i = 0; i < NT; i++) {
        Tile& t = map_[i];
        if (visit[i] || !isPowerProducer(t.bt) || !isOrigin(t)) continue;
        // компонент связности
        int qh = 0, qt = 0, cap = 0;
        qbuf[qt++] = i; visit[i] = 1;
        while (qh < qt) {
            int c = qbuf[qh++]; int x = c % MAPN, y = c / MAPN;
            const Tile& ct = map_[c];
            if (isPowerProducer(ct.bt) && isOrigin(ct)) {
                int p = BD[ct.bt].power;
                if (ct.bt == B_SOLAR) { float df = city.dayCycle; bool night = df < 0.22f || df > 0.82f; p = night ? p / 4 : p; }
                if (ct.bt == B_WIND) p = (int)(p * (0.7f + 0.3f * sinf(city.day * 0.7f + c)));
                cap += p;
            }
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d], ny = y + dy[d]; if (!inMap(nx, ny)) continue;
                int n = ny * MAPN + nx; if (visit[n] || !isConductor(map_[n])) continue;
                visit[n] = 1; qbuf[qt++] = n;
            }
        }
        totalCap += cap;
        int left = cap;
        for (int k = 0; k < qt; k++) {
            Tile& ct = map_[qbuf[k]];
            int need = pwrNeedOf(ct);
            if (ct.bt > B_RUBBLE && ct.bt != B_GROW && !isOrigin(ct)) continue;
            if (need <= left) { left -= need; totalUse += need; ct.flags |= F_POWER; }
            else if (need == 0) ct.flags |= F_POWER;
        }
    }
    // многоклеточные здания: питание по клетке-основанию
    for (int y = 0; y < MAPN; y++) for (int x = 0; x < MAPN; x++) {
        Tile& t = TL(x, y);
        if (t.bt > B_RUBBLE && t.bt != B_GROW && !isOrigin(t)) {
            if (map_[tileOriginIdx(x, y)].flags & F_POWER) t.flags |= F_POWER; else t.flags &= ~F_POWER;
        }
    }
    city.powerCap = totalCap; city.powerUse = totalUse; city.powerNeed = totalNeed;
}
static bool nearWater(int ox, int oy, int n) {
    for (int y = oy - 1; y <= oy + n; y++) for (int x = ox - 1; x <= ox + n; x++)
        if (inMap(x, y) && TL(x, y).ter == T_WATER && !TL(x, y).road) return true;
    return false;
}
static void stageWater() {
    for (int i = 0; i < NT; i++) map_[i].flags &= ~F_WATER;
    memset(visit, 0, sizeof visit);
    int qh = 0, qt = 0, cap = 0, need = 0, sew = 0;
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i]; need += watNeedOf(t);
        if (!isOrigin(t)) continue;
        if (t.bt == B_SEWAGE && (t.flags & F_POWER)) sew += 400;
        if ((t.bt == B_PUMP || t.bt == B_WTOWER) && (t.flags & F_POWER)) {
            int x = i % MAPN, y = i / MAPN;
            if (t.bt == B_PUMP && !nearWater(x, y, 1)) continue;
            cap += BD[t.bt].water;
            visit[i] = 1; qbuf[qt++] = i;
        }
    }
    // сеть труб от насосов (насос сам подаёт воду в соседние трубы)
    while (qh < qt) {
        int c = qbuf[qh++]; int x = c % MAPN, y = c / MAPN;
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d]; if (!inMap(nx, ny)) continue;
            int n = ny * MAPN + nx; if (visit[n] || !(map_[n].flags & F_PIPE)) continue;
            visit[n] = 1; qbuf[qt++] = n;
        }
    }
    // радиус 5 клеток от трубы
    memset(wdist, 255, sizeof wdist);
    int nq = qt; qh = 0;
    for (int k = 0; k < nq; k++) wdist[qbuf[k]] = 0;
    std::vector<int> q2(qbuf, qbuf + nq);
    size_t h2 = 0;
    while (h2 < q2.size()) {
        int c = q2[h2++]; int x = c % MAPN, y = c / MAPN; if (wdist[c] >= 5) continue;
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d]; if (!inMap(nx, ny)) continue;
            int n = ny * MAPN + nx; if (wdist[n] != 255) continue;
            wdist[n] = (u8)(wdist[c] + 1); q2.push_back(n);
        }
    }
    int left = cap, used = 0;
    for (size_t k = 0; k < q2.size(); k++) {
        Tile& t = map_[q2[k]]; int w = watNeedOf(t);
        if (t.bt > B_RUBBLE && t.bt != B_GROW && !isOrigin(t)) continue;
        if (w <= left) { left -= w; used += w; t.flags |= F_WATER; }
    }
    for (int y = 0; y < MAPN; y++) for (int x = 0; x < MAPN; x++) {
        Tile& t = TL(x, y);
        if (t.bt > B_RUBBLE && t.bt != B_GROW && !isOrigin(t)) { if (map_[tileOriginIdx(x, y)].flags & F_WATER) t.flags |= F_WATER; }
    }
    city.waterCap = cap; city.waterUse = used; city.waterNeed = need; city.sewageCap = sew;
}
static void stageCoverage() {
    memset(cov, 0, sizeof cov);
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i];
        if (t.bt <= B_RUBBLE || !isOrigin(t)) continue;
        const BDef& b = BD[t.bt]; if (b.cov < 0) continue;
        int n = b.size; float cx = i % MAPN + n * 0.5f, cy = i / MAPN + n * 0.5f;
        float str = (float)b.strength; if (!(t.flags & F_POWER) && b.power < 0) str *= 0.3f;
        int R = b.radius;
        for (int y = (int)(cy - R); y <= (int)(cy + R); y++) for (int x = (int)(cx - R); x <= (int)(cx + R); x++) {
            if (!inMap(x, y)) continue;
            float d = sqrtf((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            if (d > R) continue;
            int v = (int)(str * (1.0f - 0.6f * d / R));
            u8& c = cov[b.cov][y * MAPN + x];
            c = (u8)std::min(255, std::max((int)c, v) + std::min((int)c, v) / 4);
        }
    }
}
static float tmpf[NT], tmpf2[NT];
static void blur(float* a, int R) {
    for (int y = 0; y < MAPN; y++) {
        float s = 0; int n = 0;
        for (int x = -R; x < MAPN + R; x++) {
            if (x + R < MAPN && x + R >= 0) { s += a[y * MAPN + x + R]; n++; }
            if (x - R - 1 >= 0 && x - R - 1 < MAPN) { s -= a[y * MAPN + x - R - 1]; n--; }
            if (x >= 0 && x < MAPN) tmpf2[y * MAPN + x] = s / (2 * R + 1);
        }
    }
    for (int x = 0; x < MAPN; x++) {
        float s = 0;
        for (int y = -R; y < MAPN + R; y++) {
            if (y + R < MAPN && y + R >= 0) s += tmpf2[(y + R) * MAPN + x];
            if (y - R - 1 >= 0 && y - R - 1 < MAPN) s -= tmpf2[(y - R - 1) * MAPN + x];
            if (y >= 0 && y < MAPN) a[y * MAPN + x] = s / (2 * R + 1);
        }
    }
}
static void stagePollution() {
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i]; float p = 0;
        if (t.bt == B_GROW && zoneType(t) == 2 && !(t.flags & F_ABAND)) p = t.level >= 4 ? 25.0f : t.level * 45.0f;
        if (t.bt > B_RUBBLE && t.bt != B_GROW) p = BD[t.bt].poll * 1.2f;
        if (t.road) p += traffic[i] * 0.35f;
        if (t.flags & F_FIRE) p += 80;
        if (t.tree) p -= 25.0f * t.tree;
        if (t.bt == B_PARKS || t.bt == B_PARKL) p -= 40;
        tmpf[i] = p;
    }
    blur(tmpf, 3); blur(tmpf, 2);
    for (int i = 0; i < NT; i++) pollution[i] = (u8)clampi((int)(tmpf[i] * 2.2f), 0, 255);
}
static void stageLandCrime() {
    // близость к воде
    for (int i = 0; i < NT; i++) tmpf[i] = map_[i].ter == T_WATER ? 100.0f : 0.0f;
    blur(tmpf, 3);
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i];
        float v = 60 + std::min(40.0f, tmpf[i] * 1.8f) + cov[C_PARK][i] * 0.30f + cov[C_LV][i] * 0.35f + cov[C_EDU][i] * 0.10f + cov[C_HEALTH][i] * 0.06f
                + t.h * 4 - pollution[i] * 0.55f - crime[i] * 0.15f;
        if (t.ter == T_WATER) v = 0;
        landv[i] = (u8)clampi((int)v, 0, 255);
    }
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i]; float c = 0;
        if (t.bt == B_GROW) c = (t.flags & F_ABAND) ? 110.0f : 8.0f + t.level * 14.0f + (zoneType(t) == 1 ? 10 : 0);
        tmpf[i] = c;
    }
    blur(tmpf, 3);
    float unemp = 0;
    if (city.pop > 100) { int workers = city.pop / 2, jobs = city.jobsC + city.jobsI; unemp = clampf((workers - jobs) / (float)workers, 0, 1); }
    for (int i = 0; i < NT; i++) crime[i] = (u8)clampi((int)(tmpf[i] * 1.5f * (1 + unemp) - cov[C_POLICE][i] * 1.1f), 0, 255);
}
static std::vector<int> roadList;
static void stageAccessTraffic() {
    // доступ к дороге (BFS до 3 клеток) и ближайшая дорога
    roadList.clear();
    static int dist[NT];
    int qh = 0, qt = 0;
    for (int i = 0; i < NT; i++) {
        dist[i] = 99; nearRoad[i] = -1; access_[i] = 0;
        if (map_[i].road) { dist[i] = 0; nearRoad[i] = i; qbuf[qt++] = i; roadList.push_back(i); }
    }
    while (qh < qt) {
        int c = qbuf[qh++]; if (dist[c] >= 3) continue;
        int x = c % MAPN, y = c / MAPN;
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d]; if (!inMap(nx, ny)) continue;
            int n = ny * MAPN + nx; if (dist[n] <= dist[c] + 1) continue;
            if (map_[n].ter == T_WATER) continue;
            dist[n] = dist[c] + 1; nearRoad[n] = nearRoad[c]; qbuf[qt++] = n;
        }
    }
    for (int i = 0; i < NT; i++) access_[i] = dist[i] <= 3;
    // поездки: от зданий к ближайшей дороге, затем растекание по дорожной сети
    for (int i = 0; i < NT; i++) tmpf[i] = 0;
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i];
        if (t.bt == B_GROW && !(t.flags & F_ABAND) && nearRoad[i] >= 0) tmpf[nearRoad[i]] += t.pop * 0.45f;
        if (t.bt >= B_POLICE && isOrigin(t) && nearRoad[i] >= 0) tmpf[nearRoad[i]] += t.bt == B_STADIUM ? 120 : 20;
    }
    for (int it = 0; it < 8; it++) {
        for (size_t k = 0; k < roadList.size(); k++) {
            int c = roadList[k], x = c % MAPN, y = c / MAPN; int m = roadMask(x, y);
            float s = tmpf[c] * 2, n = 2;
            if (m & 1) { s += tmpf[c + 1]; n++; } if (m & 2) { s += tmpf[c - 1]; n++; }
            if (m & 4) { s += tmpf[c + MAPN]; n++; } if (m & 8) { s += tmpf[c - MAPN]; n++; }
            tmpf2[c] = s / n * (n > 2 ? 1.0f + 0.12f * (n - 2) : 1.0f);
        }
        for (size_t k = 0; k < roadList.size(); k++) tmpf[roadList[k]] = tmpf2[roadList[k]];
    }
    for (int i = 0; i < NT; i++) {
        traffic[i] = map_[i].road ? (u8)clampi((int)(tmpf[i] / ROAD_CAP[map_[i].road] * 140.0f), 0, 255) : 0;
    }
}
static int carsTarget = 0;
static void checkAchievements();
static void stageCensus() {
    int pop = 0, jc = 0, ji = 0; i64 happySum = 0; int happyN = 0;
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i];
        if (t.bt != B_GROW || (t.flags & F_ABAND)) continue;
        int zt = zoneType(t);
        if (zt == 0) {
            pop += t.pop;
            float h = 60 + cov[C_HEALTH][i] * 0.08f + cov[C_EDU][i] * 0.06f + cov[C_PARK][i] * 0.10f + cov[C_LV][i] * 0.05f + landv[i] * 0.08f
                    - pollution[i] * 0.15f - crime[i] * 0.12f - (city.tax[0] - 9) * 2.5f;
            if (!(t.flags & F_WATER) && t.level > 2) h -= 15;
            happySum += (i64)(clampf(h, 0, 100) * t.pop); happyN += t.pop;
        } else if (zt == 1) jc += t.pop; else ji += t.pop;
    }
    city.pop = pop; city.jobsC = jc; city.jobsI = ji;
    float hp = happyN ? (float)happySum / happyN : 60;
    if (city.waterUse < city.waterNeed * 0.8f && city.pop > 300) hp -= 8;
    if (city.sewageCap < city.waterUse && city.pop > 2000) hp -= 6;
    if (city.powerNeed > city.powerCap) hp -= 10;
    if (city.debt > 0) hp -= 3;
    city.happy = clampi((int)lerpf((float)city.happy, hp, 0.5f), 0, 100);
    // спрос
    float workers = pop * 0.5f, jobs = (float)(jc + ji);
    float dR = (jobs * 1.15f + 40 - workers) / (workers + 80);
    float dC = (pop * 0.22f + 12 - jc) / (pop * 0.25f + 40);
    float dI = (pop * 0.30f + 25 - ji) / (pop * 0.35f + 50);
    float hb = (city.happy - 55) * 0.01f;
    dR += hb - (city.tax[0] - 9) * 0.07f; dC -= (city.tax[1] - 9) * 0.07f; dI -= (city.tax[2] - 9) * 0.07f;
    float d3[3] = {dR, dC, dI};
    for (int k = 0; k < 3; k++) city.demand[k] = lerpf(city.demand[k], clampf(d3[k], -1, 1), 0.35f);
    // ступени развития
    int tier = 0; for (int k = 0; k < 4; k++) if (pop >= TIER_POP[k]) tier = k;
    if (tier > city.tier) {
        city.tier = tier;
        notify(std::string("НОВЫЙ СТАТУС: ") + TIER_NAME[tier] + "! ОТКРЫТЫ НОВЫЕ ЗДАНИЯ", RGB(255, 230, 90));
        playSound(SND_ACH);
    }
    int totalTraffic = 0; for (size_t k = 0; k < roadList.size(); k++) totalTraffic += traffic[roadList[k]];
    carsTarget = std::min(500, (int)roadList.size() / 10 + totalTraffic / 14);
    if (city.pop == 0) carsTarget = std::min(carsTarget, (int)roadList.size() / 12);
    checkAchievements();
}
static int simStage = -1;
static void runStage() {
    switch (simStage) {
        case 0: stagePower(); break; case 1: stageWater(); break; case 2: stageCoverage(); break;
        case 3: stageAccessTraffic(); break; case 4: stagePollution(); break; case 5: stageLandCrime(); break;
        case 6: stageCensus(); break;
    }
    simStage++; if (simStage > 6) simStage = -1;
}
static void fullRecalc() { for (simStage = 0; simStage >= 0;) runStage(); }

// --- рост зданий ---
static int maxLevel(int den) { return den == 0 ? 2 : den == 1 ? 3 : 5; }
static int tierMaxLevel() { return city.tier == 0 ? 2 : city.tier == 1 ? 3 : city.tier == 2 ? 4 : 5; }
static void startFire(int i) {
    Tile& t = map_[i]; if (t.flags & F_FIRE) return;
    t.flags |= F_FIRE; t.fireT = 0;
}
static void growTile(int i) {
    Tile& t = map_[i];
    int zt = zoneType(t);
    if (t.flags & F_FIRE) return;
    if (t.bt == B_RUBBLE) { if (grng.f() < 0.08f) { t.bt = B_NONE; t.level = 0; } return; }
    if (zt < 0) return;
    if (t.bt != B_NONE && t.bt != B_GROW) return;
    if (t.build) { t.build--; if (!t.build) { t.pop = (u16)(GROW_CAP[zt][t.level] * (0.7f + 0.3f * grng.f())); } return; }
    bool road = access_[i], power = (t.flags & F_POWER) != 0, water = (t.flags & F_WATER) != 0;
    float dem = city.demand[zt];
    // желанность места
    float des;
    if (zt == 0) des = landv[i] - pollution[i] * 0.6f - crime[i] * 0.4f + cov[C_PARK][i] * 0.15f;
    else if (zt == 1) des = landv[i] * 0.7f + traffic[nearRoad[i] >= 0 ? nearRoad[i] : i] * 0.2f - crime[i] * 0.3f + 20;
    else des = 80 - landv[i] * 0.15f + (city.tier >= 2 ? cov[C_EDU][i] * 0.2f : 0);
    int lvlCap = std::min(maxLevel(zoneDen(t)), tierMaxLevel());
    if (zt == 2 && zoneDen(t) == 2 && cov[C_EDU][i] < 60) lvlCap = std::min(lvlCap, 3);
    if (!water) lvlCap = std::min(lvlCap, 2);
    int target = 1 + (int)((des - 20) / 22.0f); target = clampi(target, 1, lvlCap);
    bool ok = road && power;
    if (t.bt == B_NONE) {
        if (ok && dem > -0.05f && grng.f() < 0.10f + dem * 0.4f) {
            t.bt = B_GROW; t.level = 1; t.var = (u8)grng.i(3); t.build = 3; t.bad = 0; t.flags &= ~F_ABAND; t.tree = 0;
            t.pop = 0;
        }
        return;
    }
    // существующее здание
    bool bad = !ok || (zt == 0 && (pollution[i] > 200 || crime[i] > 200)) || dem < -0.85f || (t.level > 2 && !water);
    if (bad) { if (t.bad < 250) t.bad++; } else if (t.bad > 0) t.bad = (u8)std::max(0, t.bad - 3);
    if (t.flags & F_ABAND) {
        if (!bad && t.bad == 0 && dem > 0 && grng.f() < 0.15f) { t.flags &= ~F_ABAND; t.pop = (u16)(GROW_CAP[zt][t.level] / 2); }
        else if (t.bad > 120 && grng.f() < 0.05f) { if (t.level > 1) t.level--; else { t.bt = B_RUBBLE; } }
        return;
    }
    if (t.bad > 60) { t.flags |= F_ABAND; t.pop = 0; return; }
    // рост уровня
    if (!bad && t.level < target && dem > 0.05f && grng.f() < 0.05f + dem * 0.15f) {
        t.level++; t.var = (u8)grng.i(3); t.build = (u8)(2 + t.level); t.pop = (u16)(t.pop / 2);
        return;
    }
    // заполнение / убывание жителей
    int cap = GROW_CAP[zt][t.level];
    float occ = clampf(0.55f + dem * 0.5f + (city.happy - 50) * 0.006f, 0.15f, 1.0f);
    int want = (int)(cap * occ);
    if (t.pop < want) t.pop = (u16)std::min(want, t.pop + 1 + cap / 10);
    else if (t.pop > want) t.pop = (u16)std::max(want, t.pop - 1 - cap / 20);
    // пожар
    float fc = 1.0f - cov[C_FIRE][i] / 255.0f;
    float fireRisk = 0.000015f * t.level * fc * fc * (season() == 1 ? 1.4f : 1.0f);
    if (city.pop > 150 && grng.f() < fireRisk) {
        startFire(i);
        static double lastFireMsg = -100;
        if (gameClock - lastFireMsg > 8) { lastFireMsg = gameClock; notify("ПОЖАР! НУЖНА ПОЖАРНАЯ ЧАСТЬ", RGB(255, 120, 80)); playSound(SND_FIRE); }
    }
}
static void fireTick(int i) {
    Tile& t = map_[i];
    if (!(t.flags & F_FIRE)) return;
    int x = i % MAPN, y = i / MAPN;
    float fc = cov[C_FIRE][i] / 255.0f;
    t.fireT = (u8)std::min(255, t.fireT + 2);
    if (grng.f() < fc * 0.25f) { t.flags &= ~F_FIRE; t.fireT = 0; return; }
    if (grng.f() < 0.05f * (1 - fc)) {
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        int d = grng.i(4), nx = x + dx[d], ny = y + dy[d];
        if (inMap(nx, ny)) { Tile& n = TL(nx, ny); if ((n.bt == B_GROW && n.level) || n.tree) startFire(ny * MAPN + nx); }
    }
    if (t.fireT > 40) {
        t.flags &= ~F_FIRE; t.fireT = 0; t.tree = 0;
        if (t.bt == B_GROW) { t.bt = B_RUBBLE; t.pop = 0; t.build = 0; t.flags &= ~F_ABAND; }
    }
}
static int growCursor = 0;
static float growAcc = 0;
static void simGrowth(float hours) {
    growAcc += hours * NT / 2.0f; // полный проход раз в 2 игровых часа
    int n = std::min((int)growAcc, (int)NT);
    growAcc -= n;
    for (int k = 0; k < n; k++) {
        int i = growCursor; growCursor = (growCursor + 1) % NT;
        growTile(i); fireTick(i);
    }
}
// --- бюджет ---
static int roadCount[4];
static void monthlyBudget() {
    int inc[3] = {0, 0, 0}; int serv = 0, util = 0;
    memset(roadCount, 0, sizeof roadCount);
    for (int i = 0; i < NT; i++) {
        const Tile& t = map_[i];
        if (t.road) roadCount[t.road]++;
        if (t.bt == B_GROW && !(t.flags & F_ABAND) && !t.build) {
            int zt = zoneType(t); float k = zt == 0 ? 0.13f : zt == 1 ? 0.17f : 0.14f;
            inc[zt] += (int)(t.pop * city.tax[zt] * k * (0.6f + landv[i] / 255.0f));
        }
        if (t.bt > B_RUBBLE && t.bt != B_GROW && isOrigin(t)) {
            if (t.bt <= B_SEWAGE) util += BD[t.bt].upkeep; else serv += BD[t.bt].upkeep;
        }
    }
    int roadE = roadCount[1] / 10 + roadCount[2] / 4 + roadCount[3] / 2;
    int loanE = 0;
    if (city.debt > 0) { loanE = (int)(city.debt / 40 + city.debt / 100); city.debt -= city.debt / 40; if (city.debt < 100) { loanE += (int)city.debt; city.debt = 0; } }
    city.incR = inc[0]; city.incC = inc[1]; city.incI = inc[2];
    city.expServ = serv; city.expRoad = roadE; city.expUtil = util; city.expLoan = loanE;
    if (!city.sandbox) city.money += inc[0] + inc[1] + inc[2] - serv - roadE - util - loanE;
    if (city.money < 0 && !city.sandbox) notify("ДЕНЬГИ ЗАКОНЧИЛИСЬ! ВОЗЬМИТЕ КРЕДИТ", RGB(255, 110, 90));
}
static float hourAcc = 0;
static int speed = 1; // 0 пауза, 1..3
static const float SEC_PER_HOUR = 0.6f;
static void simUpdate(float dt) {
    if (speed == 0) { if (simStage >= 0) runStage(); return; }
    float mul = speed == 1 ? 1.0f : speed == 2 ? 2.5f : 6.0f;
    float hours = dt * mul / SEC_PER_HOUR;
    hourAcc += hours;
    city.dayCycle = (city.hour + hourAcc) / 24.0f;
    simGrowth(hours);
    while (hourAcc >= 1.0f) {
        hourAcc -= 1.0f; city.hour++;
        if (city.hour >= 24) {
            city.hour = 0; city.day++;
            if (simStage < 0) simStage = 0;
            if (city.day % MONTH_DAYS == 0) monthlyBudget();
        }
        if (city.hour % 8 == 0 && simStage < 0) simStage = 0;
    }
    city.dayCycle = (city.hour + hourAcc) / 24.0f;
    if (simStage >= 0) runStage();
}

// --- достижения ---
struct AchDef { const char* name; const char* desc; };
static const AchDef ACH[] = {
    {"ПЕРВЫЕ ЖИТЕЛИ", "НАСЕЛЕНИЕ 10"}, {"ДЕРЕВНЯ РАСТЁТ", "НАСЕЛЕНИЕ 400"}, {"НАСТОЯЩИЙ ГОРОД", "НАСЕЛЕНИЕ 2 500"},
    {"МЕГАПОЛИС", "НАСЕЛЕНИЕ 15 000"}, {"СТОЛИЦА", "НАСЕЛЕНИЕ 50 000"}, {"ДА БУДЕТ СВЕТ", "ПОСТРОИТЬ ЭЛЕКТРОСТАНЦИЮ"},
    {"МОСТОСТРОИТЕЛЬ", "ПОСТРОИТЬ МОСТ"}, {"САДОВНИК", "ПОСАДИТЬ 100 ДЕРЕВЬЕВ"}, {"БОГАЧ", "НАКОПИТЬ 100 000"},
    {"СЧАСТЛИВЫЙ ГОРОД", "СЧАСТЬЕ 80% ПРИ 1 000 ЖИТЕЛЕЙ"}, {"АТОМНЫЙ ВЕК", "ПОСТРОИТЬ АЭС"}, {"БОЛЕЛЬЩИК", "ПОСТРОИТЬ СТАДИОН"},
};
static const int NACH = (int)(sizeof(ACH) / sizeof(ACH[0]));
static void unlockAch(int k) {
    if (city.ach & (1u << k)) return;
    city.ach |= 1u << k;
    notify(std::string("ДОСТИЖЕНИЕ: ") + ACH[k].name, RGB(255, 215, 80)); playSound(SND_ACH);
}
static void checkAchievements() {
    if (city.pop >= 10) unlockAch(0); if (city.pop >= 400) unlockAch(1); if (city.pop >= 2500) unlockAch(2);
    if (city.pop >= 15000) unlockAch(3); if (city.pop >= 50000) unlockAch(4);
    if (city.treesPlanted >= 100) unlockAch(7);
    if (!city.sandbox && city.money >= 100000) unlockAch(8);
    if (city.happy >= 80 && city.pop >= 1000) unlockAch(9);
}

// ---------------------------------------------------------------------
//  Рендер
// ---------------------------------------------------------------------
static SDL_Window* win = nullptr;
static SDL_Renderer* ren = nullptr;
static int SW = 1280, SH = 720;
static float camX = 0, camY = 0, zoom = 1.0f;
static double& gtime = gameClock;
static int layer = 0; // 0 нет, 1 электричество, 2 вода, 3 загрязнение, 4 стоимость земли, 5 преступность, 6 пожарная, 7 трафик, 8 образование, 9 здоровье
static const char* LAYER_NAME[10] = {"ОБЫЧНЫЙ ВИД", "ЭЛЕКТРИЧЕСТВО", "ВОДА", "ЗАГРЯЗНЕНИЕ", "СТОИМОСТЬ ЗЕМЛИ", "ПРЕСТУПНОСТЬ", "ПОЖАРНАЯ БЕЗОПАСНОСТЬ", "ТРАФИК", "ОБРАЗОВАНИЕ", "ЗДОРОВЬЕ"};
static u8 ambR = 255, ambG = 255, ambB = 255; static float nightK = 0;

static void uploadPages() {
    for (size_t p = 0; p < pages.size(); p++) {
        Page& pg = pages[p];
        if (pg.tm) SDL_DestroyTexture(pg.tm); if (pg.tl) SDL_DestroyTexture(pg.tl);
        pg.tm = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, PGS, PGS);
        SDL_UpdateTexture(pg.tm, nullptr, pg.m.data(), PGS * 4);
        SDL_SetTextureBlendMode(pg.tm, SDL_BLENDMODE_BLEND);
        if (pg.hasL) {
            pg.tl = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, PGS, PGS);
            SDL_UpdateTexture(pg.tl, nullptr, pg.l.data(), PGS * 4);
            SDL_SetTextureBlendMode(pg.tl, SDL_BLENDMODE_ADD);
        }
        // CPU-копии больше не нужны
        std::vector<u32>().swap(pg.m); std::vector<u32>().swap(pg.l);
    }
}
static inline void w2s(float wx, float wy, float& sx, float& sy) { sx = (wx - camX) * zoom + SW * 0.5f; sy = (wy - camY) * zoom + SH * 0.5f; }
static inline void s2w(float sx, float sy, float& wx, float& wy) { wx = (sx - SW * 0.5f) / zoom + camX; wy = (sy - SH * 0.5f) / zoom + camY; }
// угол клетки (cx,cy) с высотой h -> мировые пиксели
static inline void cornerW(float cx, float cy, float h, float& wx, float& wy) { wx = (cx - cy) * 32.0f; wy = (cx + cy) * 16.0f - h * 8.0f; }

static void drawSprW(int id, float wx, float wy, u8 r = 255, u8 g = 255, u8 b = 255, u8 a = 255, float sc = 1.0f) {
    const Spr& s = S[id]; Page& p = pages[s.pg];
    float sx, sy; w2s(wx - s.ax * sc, wy - s.ay * sc, sx, sy);
    float ex, ey; w2s(wx + (s.w - s.ax) * sc, wy + (s.h - s.ay) * sc, ex, ey);
    SDL_Rect src = {s.x, s.y, s.w, s.h};
    SDL_Rect dst = {(int)floorf(sx), (int)floorf(sy), (int)floorf(ex) - (int)floorf(sx), (int)floorf(ey) - (int)floorf(sy)};
    if (dst.x > SW || dst.y > SH || dst.x + dst.w < 0 || dst.y + dst.h < 0) return;
    SDL_SetTextureColorMod(p.tm, r, g, b); SDL_SetTextureAlphaMod(p.tm, a);
    SDL_RenderCopy(ren, p.tm, &src, &dst);
}
static void drawLitW(int id, float wx, float wy, float k) {
    const Spr& s = S[id]; Page& p = pages[s.pg]; if (!p.tl || k <= 0.01f) return;
    float sx, sy; w2s(wx - s.ax, wy - s.ay, sx, sy);
    float ex, ey; w2s(wx + (s.w - s.ax), wy + (s.h - s.ay), ex, ey);
    SDL_Rect src = {s.x, s.y, s.w, s.h};
    SDL_Rect dst = {(int)floorf(sx), (int)floorf(sy), (int)floorf(ex) - (int)floorf(sx), (int)floorf(ey) - (int)floorf(sy)};
    if (dst.x > SW || dst.y > SH || dst.x + dst.w < 0 || dst.y + dst.h < 0) return;
    SDL_SetTextureAlphaMod(p.tl, (u8)clampi((int)(k * 255), 0, 255));
    SDL_RenderCopy(ren, p.tl, &src, &dst);
}
// экранное рисование (интерфейс)
static void drawSprS(int id, int x, int y, float sc, u8 r = 255, u8 g = 255, u8 b = 255, u8 a = 255) {
    const Spr& s = S[id]; Page& p = pages[s.pg];
    SDL_Rect src = {s.x, s.y, s.w, s.h};
    SDL_Rect dst = {x, y, (int)lroundf(s.w * sc), (int)lroundf(s.h * sc)};
    SDL_SetTextureColorMod(p.tm, r, g, b); SDL_SetTextureAlphaMod(p.tm, a);
    SDL_RenderCopy(ren, p.tm, &src, &dst);
}

// --- частицы ---
struct Particle { float x, y, vx, vy, life, max, size; u8 kind; };
static std::vector<Particle> parts;
static void spawnPuff(float wx, float wy, u8 kind) {
    if (parts.size() > 1500) return;
    Particle p; p.x = wx + (grng.f() - 0.5f) * 3; p.y = wy; p.vx = 4 + grng.f() * 6; p.vy = -10 - grng.f() * 8;
    p.max = p.life = kind == 1 ? 3.5f : 2.5f; p.size = kind == 1 ? 1.6f : 0.7f; p.kind = kind;
    parts.push_back(p);
}
static void updateParticles(float dt) {
    for (size_t i = 0; i < parts.size();) {
        Particle& p = parts[i]; p.life -= dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vy *= 0.995f;
        if (p.life <= 0) { parts[i] = parts.back(); parts.pop_back(); } else i++;
    }
}
static void drawParticles() {
    for (size_t i = 0; i < parts.size(); i++) {
        const Particle& p = parts[i]; float t = 1 - p.life / p.max;
        float sc = p.size * (0.5f + t * 1.6f);
        u8 c = p.kind == 2 ? 70 : (p.kind == 1 ? 245 : 200);
        int r = (int)(c * ambR / 255), g = (int)(c * ambG / 255), b = (int)(c * ambB / 255);
        drawSprW(SP_PUFF, p.x, p.y, (u8)r, (u8)g, (u8)b, (u8)(200 * (1 - t)), sc);
    }
}

// --- машинки ---
struct Car { short x, y; u8 din, dout, kind; float t, spd; };
static std::vector<Car> cars;
static int carHead[NT]; static std::vector<int> carNext;
static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
static inline int opp(int d) { return d ^ 1; }
static int pickExit(int x, int y, int din) {
    int m = roadMask(x, y);
    int back = opp(din);
    int opts[4], n = 0;
    for (int d = 0; d < 4; d++) if ((m & (1 << d)) && d != back) { opts[n++] = d; if (d == din) { opts[n++] = d; opts[n++] = d; } }
    if (!n) return (m & (1 << back)) ? back : -1;
    return opts[grng.i(n)];
}
static void spawnCars() {
    if (roadList.empty()) { cars.clear(); return; }
    int tries = 0;
    while ((int)cars.size() < carsTarget && tries++ < 40) {
        int c = roadList[grng.i((int)roadList.size())];
        if (grng.i(255) > traffic[c] + 30) continue;
        int x = c % MAPN, y = c / MAPN; int m = roadMask(x, y); if (!m) continue;
        int d; do d = grng.i(4); while (!(m & (1 << d)));
        Car car; car.x = (short)x; car.y = (short)y; car.din = (u8)d; car.dout = (u8)d; car.t = grng.f();
        int r = grng.i(100); car.kind = (u8)(r < 6 ? 6 : r < 10 ? 7 : grng.i(6)); car.spd = 0.8f + grng.f() * 0.5f;
        cars.push_back(car);
    }
    while ((int)cars.size() > carsTarget + 20) cars.pop_back();
}
static void updateCars(float dt) {
    float mul = speed == 0 ? 0 : (speed == 1 ? 1.0f : speed == 2 ? 1.6f : 2.4f);
    for (size_t i = 0; i < cars.size();) {
        Car& c = cars[i];
        if (!inMap(c.x, c.y) || !TL(c.x, c.y).road) { cars[i] = cars.back(); cars.pop_back(); continue; }
        int ti = c.y * MAPN + c.x; float cong = traffic[ti] / 100.0f;
        float v = c.spd * (TL(c.x, c.y).road == R_DIRT ? 0.6f : TL(c.x, c.y).road == R_AVE ? 1.4f : 1.0f) / (1.0f + std::max(0.0f, cong - 0.6f) * 3.0f);
        c.t += dt * v * mul * 1.3f;
        if (c.t >= 0.5f && c.dout == c.din && c.t - dt * v * mul * 1.3f < 0.5f) {
            int e = pickExit(c.x, c.y, c.din); if (e < 0) { cars[i] = cars.back(); cars.pop_back(); continue; }
            c.dout = (u8)e;
        }
        if (c.t >= 1.0f) {
            int m = roadMask(c.x, c.y);
            if (!(m & (1 << c.dout))) { int e = pickExit(c.x, c.y, c.din); if (e < 0) { cars[i] = cars.back(); cars.pop_back(); continue; } c.dout = (u8)e; c.t = 0.5f; }
            else { c.x = (short)(c.x + DX[c.dout]); c.y = (short)(c.y + DY[c.dout]); c.din = c.dout; c.t -= 1.0f; }
        }
        i++;
    }
}
static void carWorld(const Car& c, float& u, float& v, int& dir) {
    float cx = c.x + 0.5f, cy = c.y + 0.5f;
    if (c.t < 0.5f) { dir = c.din; float k = c.t - 0.5f; u = cx + DX[dir] * k; v = cy + DY[dir] * k; }
    else { dir = c.dout; float k = c.t - 0.5f; u = cx + DX[dir] * k; v = cy + DY[dir] * k; }
    // правостороннее движение: смещение вправо от направления
    float off = TL(c.x, c.y).road == R_AVE ? 0.20f : TL(c.x, c.y).road == R_ROAD ? 0.12f : 0.06f;
    float px = -DY[dir], py = DX[dir];
    u += px * off; v += py * off;
}
// --- пешеходы ---
struct Ped { short x, y; u8 d, kind; float t, side; };
static std::vector<Ped> peds;
static void updatePeds(float dt) {
    int target = std::min(250, city.pop / 20);
    int tries = 0;
    while ((int)peds.size() < target && tries++ < 20 && !roadList.empty()) {
        int c = roadList[grng.i((int)roadList.size())]; if (map_[c].road < R_ROAD) continue;
        int x = c % MAPN, y = c / MAPN; int m = roadMask(x, y); if (!m) continue;
        int d; do d = grng.i(4); while (!(m & (1 << d)));
        Ped p; p.x = (short)x; p.y = (short)y; p.d = (u8)d; p.t = grng.f(); p.kind = (u8)grng.i(4); p.side = grng.f() < 0.5f ? -1.0f : 1.0f;
        peds.push_back(p);
    }
    while ((int)peds.size() > target) peds.pop_back();
    float mul = speed == 0 ? 0 : 1.0f;
    for (size_t i = 0; i < peds.size();) {
        Ped& p = peds[i];
        if (!inMap(p.x, p.y) || TL(p.x, p.y).road < R_ROAD) { peds[i] = peds.back(); peds.pop_back(); continue; }
        p.t += dt * 0.25f * mul;
        if (p.t >= 1) {
            int nx = p.x + DX[p.d], ny = p.y + DY[p.d];
            if (inMap(nx, ny) && TL(nx, ny).road >= R_ROAD && grng.f() < 0.8f) { p.x = (short)nx; p.y = (short)ny; p.t = 0; }
            else { int m = roadMask(p.x, p.y); int d = grng.i(4); if (m & (1 << d)) p.d = (u8)d; else p.d = (u8)opp(p.d); p.t = 0; }
        }
        i++;
    }
}

// --- превью инструмента ---
struct Preview { std::vector<int> tiles; std::vector<u8> ok; int bt = 0, bx = 0, by = 0; bool bok = false; int cost = 0; bool active = false; };
static Preview pv;

static float tileWX(int x, int y) { return (x - y) * 32.0f; }
static float tileWY(int x, int y, int h) { return (x + y + 2) * 16.0f - h * 8.0f; }
static int growSprite(const Tile& t) {
    int zt = zoneType(t); if (zt < 0) zt = 0;
    if (t.build) return SP_CONSTR[t.level <= 2 ? 0 : t.level <= 3 ? 1 : 2];
    return SP_GROW[zt][clampi(t.level, 1, 5)][t.var % 3];
}
static void layerColor(int i, u8& r, u8& g, u8& b, u8& a) {
    const Tile& t = map_[i]; a = 0; int v = 0;
    switch (layer) {
        case 1:
            if (t.flags & F_PLINE) { r = 255; g = 230; b = 60; a = 200; return; }
            if (isConductor(t)) { if (t.flags & F_POWER) { r = 250; g = 220; b = 60; a = 110; } else { r = 230; g = 50; b = 40; a = 150; } }
            return;
        case 2:
            if (t.ter == T_WATER) return;
            if (wdist[i] <= 5) { r = 60; g = 150; b = 240; a = (u8)(140 - wdist[i] * 15); }
            if (watNeedOf(t) > 0 && !(t.flags & F_WATER)) { r = 230; g = 60; b = 40; a = 150; }
            return;
        case 3: v = pollution[i]; break;
        case 4: v = 255 - landv[i]; break;
        case 5: v = crime[i]; break;
        case 6: v = 255 - cov[C_FIRE][i]; break;
        case 7: if (!t.road) return; v = traffic[i]; break;
        case 8: v = 255 - cov[C_EDU][i]; break;
        case 9: v = 255 - cov[C_HEALTH][i]; break;
    }
    if (t.ter == T_WATER && layer != 7) return;
    float f = v / 255.0f;
    if (f < 0.5f) { r = (u8)(60 + 380 * f); g = 200; b = 70; } else { r = 250; g = (u8)(200 - (f - 0.5f) * 360); b = 50; }
    if (layer == 7) a = 210;
    else if (layer == 3 || layer == 5) a = (u8)clampi(v * 3, 0, 170);          // только там, где есть проблема
    else if (layer == 6 || layer == 8 || layer == 9) {                          // покрытие службами
        int c = 255 - v; if (c < 8) { if (t.bt == B_GROW) { r = 230; g = 70; b = 50; a = 90; } return; }
        r = 60; g = 140; b = 240; a = (u8)clampi(40 + c / 2, 0, 170);
    } else a = 120;
}
static void renderWorld() {
    // освещение
    float dcv = (city.hour + hourAcc) / 24.0f; // 0..1
    float sun = sinf((dcv - 0.25f) * 6.2832f); // 1 в полдень, -1 в полночь
    float day = clampf(sun * 2.2f + 0.45f, 0, 1);
    nightK = clampf(1.0f - day * 1.3f, 0, 1);
    float dusk = clampf(1 - fabsf(sun) * 3.0f, 0, 1);
    ambR = (u8)lerpf(85, 255, day); ambG = (u8)lerpf(95, 255 - dusk * 40, day); ambB = (u8)lerpf(150, 255 - dusk * 70, day);
    if (layer) { ambR = ambG = ambB = 255; nightK = 0; }
    int ss = season();

    // видимый диапазон
    float wx0, wy0, wx1, wy1; s2w(0, 0, wx0, wy0); s2w((float)SW, (float)SH, wx1, wy1);
    int dmin = (int)floorf(wx0 / 32.0f) - 3, dmax = (int)ceilf(wx1 / 32.0f) + 3;
    int smin = (int)floorf(wy0 / 16.0f) - 4, smax = (int)ceilf((wy1 + 360) / 16.0f) + 2;
    smin = std::max(smin, 0); smax = std::min(smax, 2 * MAPN - 2);
    float wfr = (float)(gtime * 2.5);

    // ---- проход 1: земля ----
    for (int s = smin; s <= smax; s++) {
        int xa = std::max(0, std::max(s - (MAPN - 1), (s + dmin) / 2)), xb = std::min(MAPN - 1, std::min(s, (s + dmax + 1) / 2));
        for (int x = xa; x <= xb; x++) {
            int y = s - x; if (y < 0 || y >= MAPN) continue;
            const Tile& t = TL(x, y);
            int h = t.h;
            float wx = tileWX(x, y), wy = tileWY(x, y, h);
            // скалы
            for (int side = 0; side < 2; side++) {
                int nx = side == 0 ? x + 1 : x, ny = side == 0 ? y : y + 1;
                int nh = inMap(nx, ny) ? TL(nx, ny).h : -1;
                bool sand = t.ter == T_SAND;
                for (int k = std::max(nh, -1); k < h; k++)
                    drawSprW(side == 0 ? (sand ? SP_CLIFFRS : SP_CLIFFR) : (sand ? SP_CLIFFLS : SP_CLIFFL), wx, tileWY(x, y, k), ambR, ambG, ambB);
            }
            if (layer == 2) { // подземный вид
                drawSprW(t.ter == T_WATER ? SP_WATER[0] : SP_DIAM, wx, wy, 70, 62, 55);
                if (t.flags & F_PIPE) {
                    int m = 0; for (int d = 0; d < 4; d++) { int nx = x + DX[d], ny = y + DY[d]; if (inMap(nx, ny) && ((TL(nx, ny).flags & F_PIPE) || ((TL(nx, ny).bt == B_PUMP || TL(nx, ny).bt == B_WTOWER)))) m |= 1 << d; }
                    bool wet = wdist[y * MAPN + x] == 0;
                    drawSprW(SP_PIPE[m], wx, wy, wet ? 255 : 150, wet ? 255 : 150, wet ? 255 : 150);
                }
                continue;
            }
            int sp;
            if (t.ter == T_WATER) {
                int f = ((int)(wfr + (hash2(x, y) & 3))) & 3;
                sp = SP_WATER[f];
            } else if (t.ter == T_SAND) sp = SP_SAND[ss];
            else sp = SP_GRASS[ss][t.var % 3];
            drawSprW(sp, wx, wy, ambR, ambG, ambB);
            if (t.road && t.ter != T_WATER) drawSprW(SP_ROAD[t.road][roadMask(x, y)], wx, wy, ambR, ambG, ambB);
            if (t.zone && t.bt == B_NONE) drawSprW(SP_ZONE[t.zone], wx, wy, ambR, ambG, ambB);
            if (t.road && t.ter != T_WATER && nightK > 0.3f && t.road >= R_ROAD && ((x + y) % 3 == 0)) drawLitW(SP_GLOW, wx, wy - 16, nightK * 0.35f);
        }
    }
    // ---- слой данных ----
    if (layer && layer != 2) {
        for (int s = smin; s <= smax; s++) {
            int xa = std::max(0, std::max(s - (MAPN - 1), (s + dmin) / 2)), xb = std::min(MAPN - 1, std::min(s, (s + dmax + 1) / 2));
            for (int x = xa; x <= xb; x++) {
                int y = s - x; if (y < 0 || y >= MAPN) continue;
                u8 r, g, b, a; layerColor(y * MAPN + x, r, g, b, a);
                if (a) drawSprW(SP_DIAM, tileWX(x, y), tileWY(x, y, TL(x, y).h), r, g, b, a);
            }
        }
    }
    if (layer == 2) {
        for (int s = smin; s <= smax; s++) {
            int xa = std::max(0, std::max(s - (MAPN - 1), (s + dmin) / 2)), xb = std::min(MAPN - 1, std::min(s, (s + dmax + 1) / 2));
            for (int x = xa; x <= xb; x++) {
                int y = s - x; if (y < 0 || y >= MAPN) continue;
                u8 r, g, b, a; layerColor(y * MAPN + x, r, g, b, a);
                if (a) drawSprW(SP_DIAM, tileWX(x, y), tileWY(x, y, TL(x, y).h), r, g, b, a / 2);
                const Tile& t = TL(x, y);
                if (t.bt > B_RUBBLE && t.bt != B_GROW && isOrigin(t)) {
                    int n = BD[t.bt].size; for (int j = 0; j < n; j++) for (int i2 = 0; i2 < n; i2++)
                        drawSprW(SP_DOUT, tileWX(x + i2, y + j), tileWY(x + i2, y + j, t.h), (t.bt == B_PUMP || t.bt == B_WTOWER) ? 80 : 200, 200, 255, 220);
                }
            }
        }
    }
    // отметка места основания города
    if (startMarkX >= 0 && !layer) {
        float pulse = 0.5f + 0.5f * sinf((float)gtime * 4);
        for (int j = -6; j <= 6; j++) for (int i = -6; i <= 6; i++) {
            int x = startMarkX + i, y = startMarkY + j; if (!inMap(x, y) || TL(x, y).ter == T_WATER) continue;
            bool edge = abs(i) == 6 || abs(j) == 6;
            drawSprW(edge ? SP_DOUT : SP_DIAM, tileWX(x, y), tileWY(x, y, TL(x, y).h), 255, 230, 90, (u8)(edge ? 120 + 120 * pulse : 25 + 20 * pulse));
        }
    }
    // превью клеток (подсветка)
    if (pv.active) {
        for (size_t k = 0; k < pv.tiles.size(); k++) {
            int i = pv.tiles[k], x = i % MAPN, y = i / MAPN;
            drawSprW(SP_DIAM, tileWX(x, y), tileWY(x, y, TL(x, y).h), pv.ok[k] ? 90 : 250, pv.ok[k] ? 250 : 70, pv.ok[k] ? 120 : 60, 120);
        }
    }
    if (layer == 2) return;

    // ---- проход 2: объекты по глубине ----
    // распределяем машинки по клеткам
    for (size_t k = 0; k < cars.size(); k++) carHead[cars[k].y * MAPN + cars[k].x] = -1;
    carNext.resize(cars.size());
    for (size_t k = 0; k < cars.size(); k++) { int ti = cars[k].y * MAPN + cars[k].x; carNext[k] = carHead[ti]; carHead[ti] = (int)k; }
    u8 oa = layer ? 110 : 255;
    float tSec = (float)gtime;
    for (int s = smin; s <= smax; s++) {
        int xa = std::max(0, std::max(s - (MAPN - 1), (s + dmin) / 2)), xb = std::min(MAPN - 1, std::min(s, (s + dmax + 1) / 2));
        for (int x = xa; x <= xb; x++) {
            int y = s - x; if (y < 0 || y >= MAPN) continue;
            const Tile& t = TL(x, y); int ti = y * MAPN + x;
            // мосты
            if (t.road && t.ter == T_WATER) {
                int m = roadMask(x, y); int ax = (m & 12) && !(m & 3) ? 1 : 0;
                drawSprW(SP_BRIDGE[t.road][ax], tileWX(x, y), tileWY(x, y, 0), ambR, ambG, ambB, oa);
            }
            // машинки на этой клетке
            if (!cars.empty() && zoom >= 0.45f) for (int k = carHead[ti]; k >= 0; k = carNext[k]) {
                const Car& c = cars[k]; float u, v; int dir; carWorld(c, u, v, dir);
                float wx, wy; cornerW(u, v, (float)effH(t), wx, wy);
                int sp = SP_CAR[c.kind][dir >= 2 ? 1 : 0];
                drawSprW(sp, wx, wy, ambR, ambG, ambB, oa);
                if (nightK > 0.2f) drawLitW(sp, wx, wy, nightK);
            }
            // здания
            if (t.bt == B_GROW || t.bt == B_RUBBLE) {
                int sp = t.bt == B_RUBBLE ? SP_RUBBLE : growSprite(t);
                bool ab = (t.flags & F_ABAND) != 0;
                float wx = tileWX(x, y), wy = tileWY(x, y, t.h);
                if (ab) drawSprW(sp, wx, wy, (u8)(ambR * 0.55f), (u8)(ambG * 0.5f), (u8)(ambB * 0.45f), oa);
                else drawSprW(sp, wx, wy, ambR, ambG, ambB, oa);
                if (!ab && !t.build && t.bt == B_GROW && (t.flags & F_POWER)) {
                    if (nightK > 0.05f) drawLitW(sp, wx, wy, nightK * (0.6f + 0.4f * ((hash2(x, y) & 3) / 3.0f)));
                    const std::vector<Emit>& em = SMETA[sp];
                    for (size_t e = 0; e < em.size(); e++) {
                        float ex = wx + em[e].x, ey = wy + em[e].y;
                        if (em[e].type == EM_SMOKE) {
                            bool ind = zoneType(t) == 2;
                            float rate = ind ? 2.5f : (ss == 3 ? 0.5f : 0.06f);
                            if (speed && grng.f() < rate * 0.016f * (speed == 3 ? 2 : 1)) spawnPuff(ex, ey, ind ? 2 : 0);
                        } else if (em[e].type == EM_BEACON) {
                            if (fmodf(tSec + (x * 0.37f), 1.6f) < 0.5f) drawSprW(SP_BEACON, ex, ey, 255, 255, 255, 255);
                        }
                    }
                }
            } else if (t.bt > B_RUBBLE) {
                int n = BD[t.bt].size;
                if (t.ox == -(n - 1) && t.oy == -(n - 1)) {
                    int ox = x + t.ox, oy = y + t.oy;
                    int sp = SP_BLD[t.bt];
                    float wx = (ox - oy) * 32.0f, wy = (ox + oy + 2 * n) * 16.0f - t.h * 8.0f;
                    bool pw = (t.flags & F_POWER) || BD[t.bt].power >= 0;
                    drawSprW(sp, wx, wy, ambR, ambG, ambB, oa);
                    if (pw && nightK > 0.05f) drawLitW(sp, wx, wy, nightK);
                    const std::vector<Emit>& em = SMETA[sp];
                    for (size_t e = 0; e < em.size(); e++) {
                        float ex = wx + em[e].x, ey = wy + em[e].y;
                        switch (em[e].type) {
                            case EM_SMOKE: if (speed && grng.f() < 0.09f) spawnPuff(ex, ey, 2); break;
                            case EM_STEAM: if (speed && grng.f() < 0.12f) spawnPuff(ex + (grng.f() - 0.5f) * 20, ey, 1); break;
                            case EM_BEACON: if (fmodf(tSec, 1.4f) < 0.55f) drawSprW(SP_BEACON, ex, ey); break;
                            case EM_ROTOR: {
                                const Spr& r = S[SP_ROTOR]; Page& p = pages[r.pg];
                                float sx, sy; w2s(ex - r.ax, ey - r.ay, sx, sy);
                                SDL_Rect src = {r.x, r.y, r.w, r.h}; SDL_Rect dst = {(int)sx, (int)sy, (int)(r.w * zoom), (int)(r.h * zoom)};
                                SDL_SetTextureColorMod(p.tm, ambR, ambG, ambB); SDL_SetTextureAlphaMod(p.tm, oa);
                                double ang = pw && speed ? fmod(gtime * 140.0 + ox * 31, 360.0) : 20.0;
                                SDL_RenderCopyEx(ren, p.tm, &src, &dst, ang, nullptr, SDL_FLIP_NONE);
                                break;
                            }
                            case EM_FLAG: {
                                float sx, sy; w2s(ex, ey, sx, sy);
                                for (int k = 0; k < 8; k++) {
                                    float off = sinf(tSec * 6 + k * 0.8f) * 1.2f * zoom;
                                    SDL_SetRenderDrawColor(ren, (u8)(200 * ambR / 255), (u8)(40 * ambG / 255), (u8)(40 * ambB / 255), 255);
                                    SDL_Rect rr = {(int)(sx + k * zoom), (int)(sy + off), (int)ceilf(zoom), (int)ceilf(5 * zoom)};
                                    SDL_RenderFillRect(ren, &rr);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            // деревья
            if (t.tree && t.bt == B_NONE && !t.road) {
                u32 hsh = hash2(x * 7, y * 11);
                int nt = t.tree == 1 ? 1 : t.tree == 2 ? 2 : 3;
                static const float TU[3] = {0.5f, 0.3f, 0.72f}, TV[3] = {0.5f, 0.68f, 0.3f};
                for (int k = 0; k < nt; k++) {
                    float u = x + (nt == 1 ? 0.5f + ((hsh >> 4) & 7) * 0.03f - 0.1f : TU[k]), v = y + (nt == 1 ? 0.5f : TV[k]);
                    float wx, wy; cornerW(u, v, t.h, wx, wy);
                    int kind = ((hsh >> (k * 3)) & 3);
                    if (t.h >= 3) kind |= 2;
                    drawSprW(SP_TREE[kind][ss], wx, wy, ambR, ambG, ambB, oa);
                }
            }
            // ЛЭП
            if (t.flags & F_PLINE) {
                int m = 0;
                for (int d = 0; d < 4; d++) { int nx = x + DX[d], ny = y + DY[d]; if (inMap(nx, ny) && (TL(nx, ny).flags & F_PLINE)) m |= 1 << d; }
                drawSprW(SP_PLINE[m], tileWX(x, y), tileWY(x, y, effH(t)), ambR, ambG, ambB, oa);
            }
            // огонь
            if (t.flags & F_FIRE) {
                float wx, wy; cornerW(x + 0.5f, y + 0.5f, t.h, wx, wy);
                int f = ((int)(tSec * 10) + x) % 3;
                drawLitW(SP_GLOW, wx, wy - 4, 0.9f);
                drawSprW(SP_FLAME[f], wx - 6, wy + 2); drawSprW(SP_FLAME[(f + 1) % 3], wx + 7, wy - 1);
                drawSprW(SP_FLAME[(f + 2) % 3], wx, wy - 10, 255, 255, 255, 255, 1.3f);
                if (speed && grng.f() < 0.2f) spawnPuff(wx, wy - 26, 2);
            }
        }
    }
    for (size_t k = 0; k < cars.size(); k++) carHead[cars[k].y * MAPN + cars[k].x] = -1;
    // пешеходы (поверх, мелкие)
    for (size_t k = 0; zoom >= 0.6f && k < peds.size(); k++) {
        const Ped& p = peds[k]; if (!inMap(p.x, p.y)) continue;
        float u = p.x + 0.5f + DX[p.d] * (p.t - 0.5f), v = p.y + 0.5f + DY[p.d] * (p.t - 0.5f);
        float side = TL(p.x, p.y).road == R_AVE ? 0.46f : 0.36f;
        u += -DY[p.d] * side * p.side; v += DX[p.d] * side * p.side;
        float wx, wy; cornerW(u, v, (float)effH(TL(p.x, p.y)), wx, wy);
        float bob = ((int)((p.t * 40)) & 1) ? 0.5f : 0;
        drawSprW(SP_PERSON[p.kind], wx, wy - bob, ambR, ambG, ambB, oa);
    }
    drawParticles();
    // превью здания
    if (pv.active && pv.bt > B_RUBBLE) {
        int n = BD[pv.bt].size; const Tile& t = TL(clampi(pv.bx, 0, MAPN - 1), clampi(pv.by, 0, MAPN - 1));
        float wx = (pv.bx - pv.by) * 32.0f, wy = (pv.bx + pv.by + 2 * n) * 16.0f - t.h * 8.0f;
        drawSprW(SP_BLD[pv.bt], wx, wy, pv.bok ? 200 : 255, pv.bok ? 255 : 120, pv.bok ? 200 : 120, 170);
    }
}

// ---------------------------------------------------------------------
//  Инструменты, действия, отмена
// ---------------------------------------------------------------------
enum TK { TK_INSPECT, TK_BULL, TK_ROAD, TK_ZONE, TK_DEZONE, TK_BLD, TK_PLINE, TK_PIPE, TK_TREE, TK_FLAT, TK_RAISE, TK_LOWER };
struct ToolDef { int kind, arg; const char* name; };
struct Category { const char* name; int icon; std::vector<ToolDef> tools; };
static std::vector<Category> cats;
static void initTools() {
    cats.clear();
    cats.push_back(Category{"ДОРОГИ", IC_ROAD, {{TK_ROAD, R_DIRT, "ГРУНТОВКА"}, {TK_ROAD, R_ROAD, "ДОРОГА"}, {TK_ROAD, R_AVE, "ПРОСПЕКТ"}}});
    cats.push_back(Category{"ЗОНЫ", IC_ZONE, {{TK_ZONE, 1, "ЖИЛАЯ НИЗК."}, {TK_ZONE, 2, "ЖИЛАЯ СРЕД."}, {TK_ZONE, 3, "ЖИЛАЯ ВЫС."},
                                                {TK_ZONE, 4, "ТОРГ. НИЗК."}, {TK_ZONE, 5, "ТОРГ. СРЕД."}, {TK_ZONE, 6, "ТОРГ. ВЫС."},
                                                {TK_ZONE, 7, "ПРОМ. НИЗК."}, {TK_ZONE, 8, "ПРОМ. СРЕД."}, {TK_ZONE, 9, "ПРОМ. ВЫС."}, {TK_DEZONE, 0, "СНЯТЬ ЗОНУ"}}});
    cats.push_back(Category{"ЭНЕРГИЯ", IC_POWER, {{TK_PLINE, 0, "ЛЭП"}, {TK_BLD, B_WIND, "ВЕТРЯК"}, {TK_BLD, B_SOLAR, "СОЛНЕЧНАЯ"}, {TK_BLD, B_COAL, "УГОЛЬНАЯ"}, {TK_BLD, B_NUCLEAR, "АЭС"}}});
    cats.push_back(Category{"ВОДА", IC_WATER, {{TK_PIPE, 0, "ТРУБЫ"}, {TK_BLD, B_PUMP, "ВОДОКАЧКА"}, {TK_BLD, B_WTOWER, "ВОДОБАШНЯ"}, {TK_BLD, B_SEWAGE, "ОЧИСТНЫЕ"}}});
    cats.push_back(Category{"СЛУЖБЫ", IC_SERVICE, {{TK_BLD, B_POLICE, "ПОЛИЦИЯ"}, {TK_BLD, B_FIRE, "ПОЖАРНАЯ"}, {TK_BLD, B_HOSPITAL, "БОЛЬНИЦА"},
                                                     {TK_BLD, B_SCHOOL, "ШКОЛА"}, {TK_BLD, B_UNIV, "УНИВЕРСИТЕТ"}, {TK_BLD, B_CITYHALL, "МЭРИЯ"}, {TK_BLD, B_TVTOWER, "ТЕЛЕБАШНЯ"}}});
    cats.push_back(Category{"ОТДЫХ", IC_SMILE, {{TK_BLD, B_PARKS, "СКВЕР"}, {TK_BLD, B_PARKL, "ПАРК"}, {TK_BLD, B_FOUNTAIN, "ФОНТАН"}, {TK_BLD, B_STADIUM, "СТАДИОН"}}});
    cats.push_back(Category{"ЗЕМЛЯ", IC_FLAT, {{TK_TREE, 0, "ДЕРЕВЬЯ"}, {TK_FLAT, 0, "ВЫРОВНЯТЬ"}, {TK_RAISE, 0, "ПОДНЯТЬ"}, {TK_LOWER, 0, "ОПУСТИТЬ"}}});
}
static ToolDef curTool = {TK_INSPECT, 0, "ОСМОТР"};

struct UndoRec { std::vector<std::pair<int, Tile>> tiles; i64 cost; };
static std::vector<UndoRec> undoStack;
static UndoRec* curUndo = nullptr;
static std::vector<u8> touched;
static void touchTile(int i) {
    if (!curUndo) return;
    if (touched.empty()) touched.assign(NT, 0);
    if (touched[i]) return;
    touched[i] = 1; curUndo->tiles.push_back(std::make_pair(i, map_[i]));
}
static void clearBuildingAt(int x, int y) {
    Tile& t = TL(x, y);
    if (t.bt > B_RUBBLE && t.bt != B_GROW) {
        int ox = x + t.ox, oy = y + t.oy, n = BD[t.bt].size;
        for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
            if (!inMap(ox + i, oy + j)) continue;
            touchTile((oy + j) * MAPN + ox + i);
            Tile& u = TL(ox + i, oy + j); u.bt = B_NONE; u.ox = u.oy = 0; u.flags &= ~(F_FIRE); u.level = 0; u.pop = 0;
        }
    } else {
        touchTile(y * MAPN + x);
        t.bt = B_NONE; t.level = 0; t.pop = 0; t.build = 0; t.bad = 0; t.flags &= ~(F_ABAND | F_FIRE);
    }
}
static bool toolIsLine() { return curTool.kind == TK_ROAD || curTool.kind == TK_PLINE || curTool.kind == TK_PIPE; }
static bool toolIsRect() { return curTool.kind == TK_ZONE || curTool.kind == TK_DEZONE || curTool.kind == TK_BULL || curTool.kind == TK_TREE || curTool.kind == TK_FLAT || curTool.kind == TK_RAISE || curTool.kind == TK_LOWER; }
static int toolBaseCost(const ToolDef& td) {
    switch (td.kind) {
        case TK_ROAD: return ROAD_COST[td.arg]; case TK_ZONE: return 3 + ((td.arg - 1) % 3) * 2; case TK_BLD: return BD[td.arg].cost;
        case TK_PLINE: return 5; case TK_PIPE: return 4; case TK_TREE: return 3; case TK_FLAT: return 10; case TK_RAISE: case TK_LOWER: return 20;
        case TK_BULL: return 5; case TK_DEZONE: return 1; default: return 0;
    }
}
static bool toolLocked(const ToolDef& td) { return td.kind == TK_BLD && BD[td.arg].tier > city.tier && !city.sandbox; }

static int dragSX = 0, dragSY = 0, dragEX = 0, dragEY = 0, flatH = 1;
static void collectTiles(std::vector<int>& out) {
    out.clear();
    int x0 = dragSX, y0 = dragSY, x1 = dragEX, y1 = dragEY;
    if (toolIsLine()) {
        bool xFirst = abs(x1 - x0) >= abs(y1 - y0);
        int x = x0, y = y0;
        out.push_back(y * MAPN + x);
        if (xFirst) { while (x != x1) { x += x1 > x ? 1 : -1; out.push_back(y * MAPN + x); } while (y != y1) { y += y1 > y ? 1 : -1; out.push_back(y * MAPN + x); } }
        else { while (y != y1) { y += y1 > y ? 1 : -1; out.push_back(y * MAPN + x); } while (x != x1) { x += x1 > x ? 1 : -1; out.push_back(y * MAPN + x); } }
    } else {
        for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++) for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++) out.push_back(y * MAPN + x);
    }
}
// оценка (apply=false) или выполнение (apply=true) действия
static int evalTiles(bool apply) {
    std::vector<int> tl; collectTiles(tl);
    pv.tiles = tl; pv.ok.assign(tl.size(), 0);
    int cost = 0; bool bridge = false;
    for (size_t k = 0; k < tl.size(); k++) {
        int i = tl[k]; Tile& t = map_[i]; int x = i % MAPN, y = i / MAPN;
        bool svc = t.bt > B_RUBBLE && t.bt != B_GROW;
        int c = 0; bool ok = false;
        switch (curTool.kind) {
            case TK_ROAD: {
                if (svc) break;
                if (t.road == curTool.arg) { ok = true; break; }
                if (t.ter == T_WATER) {
                    // мост только прямой
                    bool prevX = k > 0 && (tl[k - 1] / MAPN == y), nextX = k + 1 < tl.size() && (tl[k + 1] / MAPN == y);
                    bool prevY = k > 0 && (tl[k - 1] % MAPN == x), nextY = k + 1 < tl.size() && (tl[k + 1] % MAPN == x);
                    if ((prevX || nextX) && (prevY || nextY)) break;
                    c = ROAD_COST[curTool.arg] * 5; bridge = true;
                } else c = ROAD_COST[curTool.arg];
                if (t.bt == B_GROW) c += 5 + t.level * 5;
                if (t.tree) c += 2;
                ok = true;
                if (apply) { if (t.bt) clearBuildingAt(x, y); touchTile(i); t.road = (u8)curTool.arg; t.zone = 0; t.tree = 0; t.flags &= ~F_ABAND; }
                break;
            }
            case TK_ZONE: {
                if (t.ter == T_WATER || t.road || svc) break;
                if (t.zone == curTool.arg) { ok = true; break; }
                if (t.bt == B_GROW && zoneType(t) != (curTool.arg - 1) / 3) break;
                ok = true; c = toolBaseCost(curTool);
                if (apply) { touchTile(i); t.zone = (u8)curTool.arg; }
                break;
            }
            case TK_DEZONE: {
                if (!t.zone) break;
                ok = true; c = 1 + (t.bt == B_GROW ? 5 : 0);
                if (apply) { if (t.bt == B_GROW || t.bt == B_RUBBLE) clearBuildingAt(x, y); touchTile(i); t.zone = 0; }
                break;
            }
            case TK_BULL: {
                if (layer == 2) { if (t.flags & F_PIPE) { ok = true; c = 2; if (apply) { touchTile(i); t.flags &= ~F_PIPE; } } break; }
                if (svc) { ok = true; c = isOrigin(t) ? 50 : 0; if (apply) clearBuildingAt(x, y); break; }
                if (t.bt == B_GROW) { c += 10 + t.level * 10; ok = true; }
                if (t.bt == B_RUBBLE) { c += 5; ok = true; }
                if (t.road) { c += 5; ok = true; }
                if (t.flags & F_PLINE) { c += 3; ok = true; }
                if (t.tree) { c += 2; ok = true; }
                if (t.zone) { ok = true; c += 1; }
                if (ok && apply) { if (t.bt) clearBuildingAt(x, y); touchTile(i); t.road = 0; t.tree = 0; t.zone = 0; t.flags &= ~F_PLINE; }
                break;
            }
            case TK_PLINE: {
                if (svc || (t.bt == B_GROW) || (t.flags & F_PLINE)) { ok = (t.flags & F_PLINE) != 0; break; }
                ok = true; c = t.ter == T_WATER ? 10 : 5;
                if (apply) { touchTile(i); t.flags |= F_PLINE; t.tree = 0; }
                break;
            }
            case TK_PIPE: {
                if (t.flags & F_PIPE) { ok = true; break; }
                if (t.ter == T_WATER) break;
                ok = true; c = 4;
                if (apply) { touchTile(i); t.flags |= F_PIPE; }
                break;
            }
            case TK_TREE: {
                if (t.ter != T_GRASS || t.road || t.bt != B_NONE || t.tree >= 3) break;
                ok = true; c = 3;
                if (apply) { touchTile(i); t.tree++; city.treesPlanted++; }
                break;
            }
            case TK_FLAT: {
                if (t.ter == T_WATER || t.road || t.bt != B_NONE || t.h == flatH) { ok = t.h == flatH && t.ter != T_WATER; break; }
                ok = true; c = 10 * abs(t.h - flatH);
                if (apply) { touchTile(i); t.h = (u8)flatH; }
                break;
            }
            case TK_RAISE: {
                if (t.road || t.bt != B_NONE || (t.flags & F_PLINE)) break;
                if (t.ter == T_WATER) { ok = true; c = 60; if (apply) { touchTile(i); t.ter = T_SAND; t.h = 1; } break; }
                if (t.h >= 6) break;
                ok = true; c = 20; if (apply) { touchTile(i); t.h++; }
                break;
            }
            case TK_LOWER: {
                if (t.road || t.bt != B_NONE || t.ter == T_WATER || (t.flags & F_PLINE)) break;
                ok = true;
                if (t.h <= 1) { c = 40; if (apply) { touchTile(i); t.ter = T_WATER; t.h = 0; t.zone = 0; t.tree = 0; } }
                else { c = 20; if (apply) { touchTile(i); t.h--; } }
                break;
            }
        }
        pv.ok[k] = ok; cost += c;
    }
    if (apply && bridge) { if (!(city.ach & (1u << 6))) { city.ach |= 1u << 6; notify("ДОСТИЖЕНИЕ: МОСТОСТРОИТЕЛЬ", RGB(255, 215, 80)); } }
    return cost;
}
static bool evalBuilding(int cx, int cy, bool apply, int& cost) {
    int bt = curTool.arg, n = BD[bt].size;
    int ox = cx - (n - 1) / 2, oy = cy - (n - 1) / 2;
    pv.bt = bt; pv.bx = ox; pv.by = oy;
    pv.tiles.clear(); pv.ok.clear();
    bool ok = true; cost = BD[bt].cost;
    int h0 = inMap(cx, cy) ? TL(cx, cy).h : 1; if (h0 < 1) h0 = 1;
    for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
        int x = ox + i, y = oy + j;
        if (!inMap(x, y)) { ok = false; continue; }
        const Tile& t = TL(x, y);
        bool g = t.ter != T_WATER && !t.road && (t.bt == B_NONE || t.bt == B_RUBBLE);
        if (!g) ok = false;
        if (t.h != h0) cost += 15 * abs(t.h - h0);
        if (t.tree) cost += 2;
        pv.tiles.push_back(y * MAPN + x); pv.ok.push_back(g);
    }
    if (bt == B_PUMP && ok && !nearWater(ox, oy, n)) ok = false;
    pv.bok = ok;
    if (ok && apply) {
        for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
            int x = ox + i, y = oy + j; touchTile(y * MAPN + x);
            Tile& t = TL(x, y);
            t.bt = (u8)bt; t.ox = (int8_t)-i; t.oy = (int8_t)-j; t.h = (u8)h0; t.tree = 0; t.zone = 0; t.level = 1; t.pop = 0; t.build = 0;
            t.flags &= ~(F_ABAND | F_FIRE);
        }
        if (isPowerProducer(bt)) { if (!(city.ach & (1u << 5))) unlockAch(5); }
        if (bt == B_NUCLEAR) unlockAch(10);
        if (bt == B_STADIUM) unlockAch(11);
    }
    return ok;
}
static bool canAfford(int cost) { return city.sandbox || city.money >= cost; }
static void beginUndo() { undoStack.push_back(UndoRec()); curUndo = &undoStack.back(); if (!touched.empty()) std::fill(touched.begin(), touched.end(), 0); }
static void endUndo(int cost) {
    curUndo->cost = city.sandbox ? 0 : cost;
    if (curUndo->tiles.empty()) undoStack.pop_back();
    curUndo = nullptr;
    if (undoStack.size() > 30) undoStack.erase(undoStack.begin());
}
static void applyTool() {
    if (curTool.kind == TK_BLD) {
        int cost; bool ok = evalBuilding(dragEX, dragEY, false, cost);
        if (toolLocked(curTool)) { notify(std::string("НУЖЕН СТАТУС: ") + TIER_NAME[BD[curTool.arg].tier], RGB(255, 150, 90)); playSound(SND_ERR); return; }
        if (!ok) { notify(curTool.arg == B_PUMP ? "ВОДОКАЧКУ НАДО СТАВИТЬ У ВОДЫ" : "ЗДЕСЬ СТРОИТЬ НЕЛЬЗЯ", RGB(255, 150, 90)); playSound(SND_ERR); return; }
        if (!canAfford(cost)) { notify("НЕДОСТАТОЧНО ДЕНЕГ", RGB(255, 150, 90)); playSound(SND_ERR); return; }
        beginUndo(); evalBuilding(dragEX, dragEY, true, cost); endUndo(cost);
        if (!city.sandbox) city.money -= cost;
        playSound(SND_BUILD);
    } else {
        int cost = evalTiles(false);
        int nok = 0; for (size_t k = 0; k < pv.ok.size(); k++) nok += pv.ok[k];
        if (!nok || cost == 0) { if (!nok) playSound(SND_ERR); return; }
        if (!canAfford(cost)) { notify("НЕДОСТАТОЧНО ДЕНЕГ", RGB(255, 150, 90)); playSound(SND_ERR); return; }
        beginUndo(); evalTiles(true); endUndo(cost);
        if (curTool.kind == TK_ROAD || curTool.kind == TK_ZONE) startMarkX = -1;
        if (!city.sandbox) city.money -= cost;
        playSound(curTool.kind == TK_BULL ? SND_BULL : SND_BUILD);
    }
    fullRecalc();
}
static void undoLast() {
    if (undoStack.empty()) { notify("НЕЧЕГО ОТМЕНЯТЬ"); return; }
    UndoRec& u = undoStack.back();
    for (size_t k = u.tiles.size(); k-- > 0;) map_[u.tiles[k].first] = u.tiles[k].second;
    if (!city.sandbox) city.money += u.cost;
    undoStack.pop_back();
    notify("ДЕЙСТВИЕ ОТМЕНЕНО"); playSound(SND_CLICK);
    fullRecalc();
}
static bool pickTile(float sx, float sy, int& tx, int& ty) {
    float wx, wy; s2w(sx, sy, wx, wy);
    for (int h = 6; h >= 0; h--) {
        float wy2 = wy + h * 8.0f;
        float fx = (wx / 32.0f + wy2 / 16.0f) * 0.5f, fy = (wy2 / 16.0f - wx / 32.0f) * 0.5f;
        int x = (int)floorf(fx), y = (int)floorf(fy);
        if (inMap(x, y) && TL(x, y).h == h) { tx = x; ty = y; return true; }
    }
    float wy2 = wy + 8.0f;
    tx = clampi((int)floorf((wx / 32.0f + wy2 / 16.0f) * 0.5f), 0, MAPN - 1); ty = clampi((int)floorf((wy2 / 16.0f - wx / 32.0f) * 0.5f), 0, MAPN - 1);
    return false;
}

// ---------------------------------------------------------------------
//  Сохранение
// ---------------------------------------------------------------------
static std::string basePath;
static bool soundOn = true;
static std::string slotPath(int s) { return basePath + strf("gorod_slot%d.sav", s + 1); }
static const u32 SAVE_MAGIC = 0x31445247; // "GRD1"
static const u32 SAVE_VER = 2;
static bool saveGame(int s) {
    std::string p = slotPath(s), tmp = p + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb"); if (!f) return false;
    u32 hdr[3] = {SAVE_MAGIC, SAVE_VER, (u32)sizeof(City)};
    bool ok = fwrite(hdr, sizeof hdr, 1, f) == 1 && fwrite(&city, sizeof city, 1, f) == 1 && fwrite(map_, sizeof map_, 1, f) == 1;
    float cam[3] = {camX, camY, zoom}; ok = ok && fwrite(cam, sizeof cam, 1, f) == 1;
    ok = fclose(f) == 0 && ok;
    if (ok) { remove(p.c_str()); ok = rename(tmp.c_str(), p.c_str()) == 0; }
    return ok;
}
static bool readSaveHeader(int s, City* out) {
    FILE* f = fopen(slotPath(s).c_str(), "rb"); if (!f) return false;
    u32 hdr[3]; bool ok = fread(hdr, sizeof hdr, 1, f) == 1 && hdr[0] == SAVE_MAGIC && hdr[1] == SAVE_VER && hdr[2] == sizeof(City);
    if (ok && out) ok = fread(out, sizeof(City), 1, f) == 1;
    fclose(f); return ok;
}
static bool loadGame(int s) {
    FILE* f = fopen(slotPath(s).c_str(), "rb"); if (!f) return false;
    u32 hdr[3]; City c; static Tile tmp[NT]; float cam[3];
    bool ok = fread(hdr, sizeof hdr, 1, f) == 1 && hdr[0] == SAVE_MAGIC && hdr[1] == SAVE_VER && hdr[2] == sizeof(City)
              && fread(&c, sizeof c, 1, f) == 1 && fread(tmp, sizeof tmp, 1, f) == 1 && fread(cam, sizeof cam, 1, f) == 1;
    fclose(f);
    if (!ok) return false;
    city = c; memcpy(map_, tmp, sizeof map_);
    camX = cam[0]; camY = cam[1]; zoom = clampf(cam[2], 0.4f, 3.0f);
    undoStack.clear(); cars.clear(); peds.clear(); parts.clear();
    clearSimMaps(); fullRecalc(); fullRecalc();
    return true;
}
static void saveConfig() {
    FILE* f = fopen((basePath + "gorod.cfg").c_str(), "wb"); if (!f) return;
    int v[4] = {slot, soundOn, seasonsOn, 1}; fwrite(v, sizeof v, 1, f); fclose(f);
}
static void loadConfig() {
    FILE* f = fopen((basePath + "gorod.cfg").c_str(), "rb"); if (!f) return;
    int v[4]; if (fread(v, sizeof v, 1, f) == 1) { slot = clampi(v[0], 0, 2); soundOn = v[1] != 0; seasonsOn = v[2] != 0; }
    fclose(f);
}
static void centerCamera() { camX = 0; camY = MAPN * 16.0f; zoom = 1.0f; }
static void newGame(u32 seed, bool sandbox, int type = MT_RIVER, int sx = -1, int sy = -1) {
    city = City(); city.seed = seed; city.sandbox = sandbox; city.money = sandbox ? 0 : 20000;
    city.hour = 8; city.dayCycle = 8 / 24.0f; city.day = 4 * MONTH_DAYS; // начинаем в мае
    generateMap(seed, type);
    if (sx < 0) findStartSpot(sx, sy);
    undoStack.clear(); cars.clear(); peds.clear(); parts.clear();
    centerCamera();
    camX = (sx - sy) * 32.0f; camY = (sx + sy + 1) * 16.0f - 8.0f;
    startMarkX = sx; startMarkY = sy;
    fullRecalc();
    msgs.clear();
    notify("ДОБРО ПОЖАЛОВАТЬ В «МОЙ ГОРОД»!", RGB(255, 230, 120));
    notify("ЖЁЛТАЯ РАМКА — ВЫБРАННОЕ МЕСТО ДЛЯ ГОРОДА", RGB(255, 225, 110));
    notify("ПРОЛОЖИТЕ ДОРОГУ, РАЗМЕТЬТЕ ЗОНЫ И ДАЙТЕ ТОК", RGB(220, 230, 255));
}

// ---------------------------------------------------------------------
//  Звук: синтез в реальном времени
// ---------------------------------------------------------------------
struct Voice { int type; float t, dur; };
static Voice voices[16];
static SDL_AudioDeviceID audioDev = 0;
static float ambVol = 0;
static u32 noiseState = 12345;
static inline float noise1() { noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5; return (noiseState & 0xFFFF) / 32768.0f - 1.0f; }
static float lp1 = 0, lp2 = 0;
static void audioCB(void*, Uint8* stream, int len) {
    Sint16* out = (Sint16*)stream; int n = len / 2;
    const float SR = 22050.0f, dt = 1.0f / SR;
    for (int k = 0; k < n; k++) {
        float s = 0;
        // фон города
        float nz = noise1(); lp1 += (nz - lp1) * 0.02f; lp2 += (lp1 - lp2) * 0.05f;
        s += lp2 * ambVol * 2.5f;
        for (int v = 0; v < 16; v++) {
            Voice& vo = voices[v]; if (vo.dur <= 0) continue;
            float t = vo.t, e = 1.0f - t / vo.dur, val = 0;
            switch (vo.type) {
                case SND_CLICK: val = (fmodf(t * 1400, 1.0f) < 0.5f ? 0.25f : -0.25f) * e * e; break;
                case SND_BUILD: val = sinf(6.2832f * (160 - 90 * t / vo.dur) * t) * 0.5f * e + noise1() * 0.15f * e * e * e; break;
                case SND_BULL: { float f = noise1(); val = f * 0.35f * e * (0.6f + 0.4f * sinf(t * 90)); break; }
                case SND_ERR: val = (fmodf(t * (t < 0.1f ? 300.0f : 210.0f), 1.0f) < 0.5f ? 0.18f : -0.18f) * e; break;
                case SND_COIN: val = sinf(6.2832f * (t < 0.07f ? 990.0f : 1320.0f) * t) * 0.3f * e; break;
                case SND_FIRE: val = sinf(6.2832f * (650 + 250 * sinf(t * 9)) * t) * 0.22f * e; break;
                case SND_ACH: { const float nt[4] = {523, 659, 784, 1046}; int i = clampi((int)(t / 0.11f), 0, 3); val = sinf(6.2832f * nt[i] * t) * 0.28f * (i == 3 ? e * 2 : 1) * fminf(1.0f, e * 3); break; }
            }
            s += val; vo.t += dt; if (vo.t >= vo.dur) vo.dur = 0;
        }
        s = clampf(s, -1, 1);
        out[k] = (Sint16)(s * 12000);
    }
}
static void playSound(int s) {
    if (!audioDev || !soundOn) return;
    static const float DUR[7] = {0.05f, 0.18f, 0.35f, 0.22f, 0.16f, 1.0f, 0.55f};
    SDL_LockAudioDevice(audioDev);
    for (int v = 0; v < 16; v++) if (voices[v].dur <= 0) { voices[v].type = s; voices[v].t = 0; voices[v].dur = DUR[s]; break; }
    SDL_UnlockAudioDevice(audioDev);
}
static void initAudio() {
    SDL_AudioSpec want, have; memset(&want, 0, sizeof want);
    want.freq = 22050; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024; want.callback = audioCB;
    audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audioDev) SDL_PauseAudioDevice(audioDev, 0);
}

// ---------------------------------------------------------------------
//  Интерфейс
// ---------------------------------------------------------------------
static int U = 3, SU = 2;             // масштаб интерфейса и мелкого шрифта
static int padL = 20, padR = 20, padT = 10, padB = 6;
static short glyphCache[0x2300];
static int glyphOf(int cp) {
    if (cp < 0 || cp >= 0x2300) return glyphIndex(cp);
    if (glyphCache[cp] == 0) glyphCache[cp] = (short)(glyphIndex(cp) + 1);
    return glyphCache[cp] - 1;
}
static int textW(const std::string& s, int sc) {
    int n = 0; const char* p = s.c_str(); while (utf8next(p)) n++;
    return n ? n * 6 * sc - sc : 0;
}
static int drawText(int x, int y, const std::string& s, int sc, u32 col = RGB(255, 255, 255), int align = 0) {
    int w = textW(s, sc);
    if (align == 1) x -= w / 2; else if (align == 2) x -= w;
    const char* p = s.c_str(); int cp;
    u8 r = (col >> 16) & 255, g = (col >> 8) & 255, b = col & 255, a = col >> 24;
    while ((cp = utf8next(p)) != 0) {
        if (cp != ' ') drawSprS(SP_FONT[glyphOf(cp)], x, y, (float)sc, r, g, b, a);
        x += 6 * sc;
    }
    return w;
}
static void fillR(int x, int y, int w, int h, u32 c) {
    SDL_SetRenderDrawColor(ren, (c >> 16) & 255, (c >> 8) & 255, c & 255, c >> 24);
    SDL_Rect r = {x, y, w, h}; SDL_RenderFillRect(ren, &r);
}
static void panel(int x, int y, int w, int h, u32 bg = ARGB(235, 34, 40, 56), u32 border = RGB(90, 110, 150)) {
    int b = std::max(1, U / 2);
    fillR(x + b, y, w - 2 * b, h, bg); fillR(x, y + b, b, h - 2 * b, bg); fillR(x + w - b, y + b, b, h - 2 * b, bg);
    fillR(x + b, y, w - 2 * b, b, border); fillR(x + b, y + h - b, w - 2 * b, b, border);
    fillR(x, y + b, b, h - 2 * b, border); fillR(x + w - b, y + b, b, h - 2 * b, border);
}
enum BtnId { BT_NONE, BT_CAT, BT_TOOL, BT_SPEED, BT_MENU, BT_LAYERS, BT_LAYER, BT_UNDO, BT_INSPECT, BT_BULL, BT_BUDGET, BT_CLOSE,
             BT_MITEM, BT_TAX, BT_LOAN_TAKE, BT_LOAN_PAY, BT_SLOT, BT_SEED, BT_NEWSLOT, BT_MODE, BT_CREATE, BT_BACKDROP, BT_INFO_BULL, BT_MAPTYPE, BT_PREVIEW };
struct Btn { SDL_Rect r; int id, arg; bool slider; };
static std::vector<Btn> btns;
static void addBtn(int x, int y, int w, int h, int id, int arg = 0, bool slider = false) { Btn b; b.r = SDL_Rect{x, y, w, h}; b.id = id; b.arg = arg; b.slider = slider; btns.push_back(b); }
static bool pressedOn = false; static Btn pressedB; // нажатая кнопка
static bool btnDown(int id, int arg) { return pressedOn && pressedB.id == id && pressedB.arg == arg; }
static void button(int x, int y, int w, int h, int id, int arg, bool active = false, u32 bg = RGB(52, 62, 86)) {
    bool dn = btnDown(id, arg);
    u32 c = active ? RGB(70, 130, 200) : bg; if (dn) c = cshade(c, 1.3f);
    panel(x, y, w, h, c, active ? RGB(160, 210, 255) : RGB(100, 116, 150));
    addBtn(x, y, w, h, id, arg);
}
static int fitScale(const std::string& s, int sc, int maxW) { while (sc > 1 && textW(s, sc) > maxW) sc--; return sc; }
static void textButton(int x, int y, int w, int h, int id, int arg, const std::string& s, bool active = false, u32 bg = RGB(52, 62, 86)) {
    button(x, y, w, h, id, arg, active, bg);
    int sc = fitScale(s, std::min(U, std::max(1, (h - 2 * U) / 7)), w - 2 * U);
    drawText(x + w / 2, y + (h - 7 * sc) / 2, s, sc, RGB(255, 255, 255), 1);
}
static void iconAt(int ic, int x, int y, int sz) { drawSprS(SP_ICON[ic], x, y, sz / 12.0f); }

enum { P_NONE, P_MENU, P_BUDGET, P_ACH, P_SAVE, P_LOAD, P_NEW };
static int panelOpen = P_NONE, openCat = -1, infoTile = -1; static bool layersOpen = false;
static int prevSpeed = 1;
static u32 newSeed = 1; static bool newSandbox = false; static int newSlot = 0;
static bool quitReq = false;
static std::string hint;
static City slotInfo[3]; static bool slotValid[3];
static void refreshSlots() { for (int s = 0; s < 3; s++) slotValid[s] = readSaveHeader(s, &slotInfo[s]); }
static void openPanel(int p) {
    if (panelOpen == P_NONE && p != P_NONE) { prevSpeed = speed; speed = 0; }
    if (p == P_NONE && panelOpen != P_NONE) speed = prevSpeed;
    panelOpen = p; if (p == P_SAVE || p == P_LOAD || p == P_NEW) refreshSlots();
}
static std::string moneyStr() { return city.sandbox ? std::string("∞") : fmtInt(city.money) + " ₽"; }
static std::string dateStr() {
    int m = monthIndex(); return strf("%d %s %d", city.day % MONTH_DAYS + 1, MONTH_NAME[m % 12], 2026 + m / 12);
}
static void demandBar(int x, int y, int w, int h, float d, u32 c) {
    fillR(x, y, w, h, ARGB(255, 60, 66, 84));
    int mid = y + h / 2; int len = (int)(fabsf(d) * h / 2);
    if (d >= 0) fillR(x, mid - len, w, len, c); else fillR(x, mid, w, len, cshade(c, 0.6f));
    fillR(x, mid, w, 1, RGB(200, 200, 200));
}
static void toolPreview(const ToolDef& td, int x, int y, int w, int h) {
    int sp = -1;
    switch (td.kind) {
        case TK_ROAD: sp = SP_ROAD[td.arg][3]; break; case TK_ZONE: sp = SP_GROW[(td.arg - 1) / 3][((td.arg - 1) % 3) * 2 + 1][0]; break;
        case TK_BLD: sp = SP_BLD[td.arg]; break; case TK_PLINE: sp = SP_PLINE[3]; break; case TK_PIPE: sp = SP_PIPE[3]; break;
        case TK_TREE: sp = SP_TREE[0][season()]; break;
        default: break;
    }
    if (sp < 0) {
        int ic = td.kind == TK_DEZONE ? IC_CLOSE : td.kind == TK_FLAT ? IC_FLAT : td.kind == TK_RAISE ? IC_PLAY : IC_PLAY;
        int sz = std::min(w, h) * 2 / 3;
        if (td.kind == TK_RAISE || td.kind == TK_LOWER) {
            int sc = std::max(2, sz / 8);
            drawText(x + w / 2, y + (h - 7 * sc) / 2, td.kind == TK_RAISE ? "↑" : "↓", sc, RGB(255, 255, 255), 1);
        } else iconAt(ic, x + (w - sz) / 2, y + (h - sz) / 2, sz);
        return;
    }
    const Spr& s = S[sp];
    float sc = std::min((float)w / s.w, (float)h / s.h);
    if (td.kind == TK_ZONE) {
        // плитка зоны под зданием
        drawSprS(SP_ZONE[td.arg], x + (int)((w - 64 * sc) / 2), y + h - (int)(32 * sc), sc);
    }
    int dw = (int)(s.w * sc), dh = (int)(s.h * sc);
    drawSprS(sp, x + (w - dw) / 2, y + h - dh, sc);
}
static void drawTopBar() {
    int th = 15 * U, y = padT;
    panel(padL, y, SW - padL - padR, th, ARGB(225, 28, 34, 48));
    int x = padL + U;
    button(x, y + U, 13 * U, th - 2 * U, BT_MENU, 0); iconAt(IC_MENU, x + U * 2 + U / 2, y + U * 2 + U / 2, 8 * U); x += 15 * U;
    int ty = y + (th - 7 * U) / 2;
    iconAt(IC_COIN, x, y + (th - 9 * U) / 2, 9 * U); x += 10 * U;
    u32 mc = city.money < 0 ? RGB(255, 110, 100) : RGB(255, 235, 150);
    x += drawText(x, ty, moneyStr(), U, mc) + 4 * U;
    addBtn(x - 60 * U, y, 60 * U, th, BT_BUDGET, 0);
    iconAt(IC_PEOPLE, x, y + (th - 9 * U) / 2, 9 * U); x += 10 * U;
    x += drawText(x, ty, fmtInt(city.pop), U) + 4 * U;
    iconAt(IC_SMILE, x, y + (th - 9 * U) / 2, 9 * U); x += 10 * U;
    u32 hc = city.happy >= 60 ? RGB(150, 255, 150) : city.happy >= 40 ? RGB(255, 230, 120) : RGB(255, 120, 110);
    x += drawText(x, ty, strf("%d%%", city.happy), U, hc) + 4 * U;
    // спрос
    int bw = 4 * U, bh = th - 4 * U, by = y + (th - bh) / 2;
    drawText(x, ty, "Ж", SU, RGB(140, 240, 140)); demandBar(x + 4 * SU + U / 2, by, bw, bh, city.demand[0], RGB(80, 220, 90)); x += 4 * SU + bw + 2 * U;
    drawText(x, ty, "К", SU, RGB(140, 190, 255)); demandBar(x + 4 * SU + U / 2, by, bw, bh, city.demand[1], RGB(80, 140, 240)); x += 4 * SU + bw + 2 * U;
    drawText(x, ty, "П", SU, RGB(250, 225, 110)); demandBar(x + 4 * SU + U / 2, by, bw, bh, city.demand[2], RGB(240, 200, 50)); x += 4 * SU + bw + 4 * U;
    // скорость справа
    int sx = SW - padR - U - 4 * 13 * U;
    for (int k = 0; k < 4; k++) {
        int bx = sx + k * 13 * U;
        button(bx, y + U, 12 * U, th - 2 * U, BT_SPEED, k, speed == k && panelOpen == P_NONE);
        iconAt(k == 0 ? IC_PAUSE : k == 1 ? IC_PLAY : k == 2 ? IC_FF : IC_FFF, bx + 2 * U, y + U * 2 + U / 2, 8 * U);
    }
    // дата и статус посередине оставшегося места
    std::string d = dateStr(), st = std::string(TIER_NAME[city.tier]) + (seasonsOn ? std::string(" · ") + SEASON_NAME[season()] : "");
    int rx = sx - 3 * U;
    int dw = std::max(textW(d, SU), textW(st, SU));
    if (rx - dw > x) {
        drawText(rx, y + U * 2, d, SU, RGB(230, 230, 240), 2);
        drawText(rx, y + th - U * 2 - 7 * SU, st, SU, RGB(170, 200, 255), 2);
    }
    // время суток
    int hh = city.hour, mm = (int)(hourAcc * 60);
    if (rx - dw - textW("00:00", SU) - 4 * U > x) drawText(rx - dw - 3 * U, y + (th - 7 * SU) / 2, strf("%02d:%02d", hh, mm % 60), SU, RGB(200, 200, 210), 2);
}
static int bottomH() { return 24 * U; }
static void drawBottomBar() {
    int bh = bottomH(), y = SH - bh - padB;
    const int NB = 12;
    int avail = SW - padL - padR;
    int bw = std::min(30 * U, avail / NB - U);
    int total = NB * (bw + U) - U, x = padL + (avail - total) / 2;
    panel(x - U, y - U, total + 2 * U, bh + 2 * U, ARGB(215, 28, 34, 48));
    int isz = std::min(bw, bh) * 11 / 20;
    auto cell = [&](int k, int id, int arg, int ic, const char* label, bool active) {
        int bx = x + k * (bw + U);
        button(bx, y, bw, bh, id, arg, active);
        iconAt(ic, bx + (bw - isz) / 2, y + U * 2, isz);
        int sc = SU; std::string l = label; while (sc > 1 && textW(l, sc) > bw - U) sc--;
        if (textW(l, sc) <= bw) drawText(bx + bw / 2, y + bh - 7 * sc - U * 3 / 2, l, sc, RGB(225, 230, 240), 1);
    };
    cell(0, BT_INSPECT, 0, IC_INSPECT, "ОСМОТР", curTool.kind == TK_INSPECT && openCat < 0);
    cell(1, BT_BULL, 0, IC_BULL, "СНОС", curTool.kind == TK_BULL);
    for (int c = 0; c < (int)cats.size(); c++) {
        bool act = openCat == c;
        for (size_t k = 0; k < cats[c].tools.size(); k++) if (cats[c].tools[k].kind == curTool.kind && cats[c].tools[k].arg == curTool.arg && curTool.kind != TK_INSPECT) act = true;
        cell(2 + c, BT_CAT, c, cats[c].icon, cats[c].name, act);
    }
    cell(9, BT_LAYERS, 0, IC_LAYERS, "СЛОИ", layersOpen || layer);
    cell(10, BT_UNDO, 0, IC_UNDO, "ОТМЕНА", false);
    cell(11, BT_BUDGET, 0, IC_COIN, "БЮДЖЕТ", panelOpen == P_BUDGET);
    // подменю категории
    if (openCat >= 0) {
        const Category& c = cats[openCat];
        int n = (int)c.tools.size();
        int cw = std::min(44 * U, (avail - 2 * U) / n - U), ch = 44 * U;
        int tw = n * (cw + U) - U, tx = padL + (avail - tw) / 2, ty = y - 2 * U - ch;
        int csc = SU; for (int k = 0; k < n; k++) csc = std::min(csc, fitScale(c.tools[k].name, SU, cw - U));
        panel(tx - U, ty - U, tw + 2 * U, ch + 2 * U, ARGB(220, 28, 34, 48));
        for (int k = 0; k < n; k++) {
            const ToolDef& td = c.tools[k];
            int bx = tx + k * (cw + U);
            bool act = curTool.kind == td.kind && curTool.arg == td.arg;
            bool locked = toolLocked(td);
            button(bx, ty, cw, ch, BT_TOOL, k, act, locked ? RGB(40, 42, 50) : RGB(52, 62, 86));
            int sc = csc; std::string nm = td.name;
            int prevH = ch - 7 * sc * 2 - 6 * U;
            toolPreview(td, bx + U * 2, ty + U * 2, cw - 4 * U, prevH);
            if (locked) { fillR(bx + U, ty + U, cw - 2 * U, prevH + U, ARGB(150, 20, 20, 30)); drawText(bx + cw / 2, ty + prevH / 2, TIER_NAME[BD[td.arg].tier], std::max(1, sc - (sc > 1)), RGB(255, 170, 120), 1); }
            drawText(bx + cw / 2, ty + ch - 7 * sc * 2 - 3 * U, nm, sc, RGB(255, 255, 255), 1);
            int cost = toolBaseCost(td);
            std::string cs = td.kind == TK_BLD ? fmtInt(cost) + " ₽" : (cost ? strf("%d ₽/КЛ.", cost) : "");
            if (td.kind == TK_BLD && BD[td.arg].power > 0) cs = strf("%d МВТ", BD[td.arg].power);
            if (textW(cs, sc) > cw - U) cs = fmtInt(cost) + " ₽";
            drawText(bx + cw / 2, ty + ch - 7 * sc - 2 * U, cs, sc, RGB(255, 225, 140), 1);
        }
    }
    if (layersOpen) {
        int n = 10, lw = 70 * U, lh = 11 * U;
        int lx = x + 9 * (bw + U) + bw / 2 - lw / 2; lx = std::min(lx, SW - padR - lw); int ly = y - 2 * U - n * (lh + U);
        for (int k = 0; k < n; k++) {
            int yy = ly + k * (lh + U);
            button(lx, yy, lw, lh, BT_LAYER, k, layer == k);
            drawText(lx + 2 * U, yy + (lh - 7 * SU) / 2, LAYER_NAME[k], SU);
        }
    }
}
static void infoLine(int x, int& y, int w, const std::string& a, const std::string& b, u32 cb = RGB(255, 255, 255)) {
    drawText(x, y, a, SU, RGB(170, 185, 210)); drawText(x + w, y, b, SU, cb, 2); y += 9 * SU;
}
static std::string pct(int v) { return strf("%d%%", v * 100 / 255); }
static void drawInfo() {
    if (infoTile < 0) return;
    int x0 = infoTile % MAPN, y0 = infoTile / MAPN; const Tile& t = map_[infoTile];
    int i = infoTile;
    int ow = 110 * U, ox = SW - padR - ow, oy = padT + 17 * U;
    int lines = 11; int oh = (lines + 2) * 9 * SU + 16 * U;
    oh = std::min(oh, SH - oy - bottomH() - padB - 6 * U);
    panel(ox, oy, ow, oh);
    button(ox + ow - 12 * U, oy + 2 * U, 10 * U, 10 * U, BT_CLOSE, 0); iconAt(IC_CLOSE, ox + ow - 11 * U, oy + 3 * U, 8 * U);
    std::string title; u32 tc = RGB(255, 235, 150);
    int x = ox + 4 * U, y = oy + 4 * U, w = ow - 8 * U;
    if (t.bt == B_GROW) title = GROW_NAME[zoneType(t)][t.level];
    else if (t.bt > B_RUBBLE) title = BD[t.bt].name;
    else if (t.bt == B_RUBBLE) title = "РУИНЫ";
    else if (t.road) title = t.ter == T_WATER ? std::string("МОСТ") : ROAD_NAME[t.road];
    else if (t.zone) title = std::string(ZT_NAME[zoneType(t)]) + " ЗОНА";
    else title = t.ter == T_WATER ? "ВОДА" : t.ter == T_SAND ? "ПЕСОК" : t.tree ? "ЛЕС" : "ТРАВА";
    int tsc = U; while (tsc > 1 && textW(title, tsc) > w - 12 * U) tsc--;
    drawText(x, y, title, tsc, tc); y += 7 * tsc + 4 * U;
    if (t.flags & F_FIRE) { drawText(x, y, "ГОРИТ!", SU, RGB(255, 120, 80)); y += 9 * SU; }
    if (t.bt == B_GROW) {
        int zt = zoneType(t);
        infoLine(x, y, w, "ЗОНА", std::string(ZT_NAME[zt]) + ", " + DN_NAME[zoneDen(t)]);
        infoLine(x, y, w, "УРОВЕНЬ", strf("%d", t.level));
        if (t.build) infoLine(x, y, w, "СОСТОЯНИЕ", "СТРОИТСЯ", RGB(255, 220, 120));
        else if (t.flags & F_ABAND) infoLine(x, y, w, "СОСТОЯНИЕ", "ЗАБРОШЕНО", RGB(255, 120, 100));
        infoLine(x, y, w, zt == 0 ? "ЖИТЕЛИ" : "РАБОЧИЕ МЕСТА", strf("%d / %d", t.pop, GROW_CAP[zt][t.level]));
    } else if (t.bt > B_RUBBLE) {
        const BDef& b = BD[t.bt];
        if (b.power > 0) infoLine(x, y, w, "МОЩНОСТЬ", strf("%d МВТ", b.power));
        if (b.water > 0) infoLine(x, y, w, "ВОДА", strf("%d ЕД.", b.water));
        if (t.bt == B_SEWAGE) infoLine(x, y, w, "ОЧИСТКА", "400 ЕД.");
        infoLine(x, y, w, "СОДЕРЖАНИЕ", strf("%d ₽/МЕС", b.upkeep));
        if (b.cov >= 0) infoLine(x, y, w, "РАДИУС", strf("%d КЛ.", b.radius));
        if (t.bt == B_PUMP && !nearWater(x0 + t.ox, y0 + t.oy, 1)) infoLine(x, y, w, "!", "НЕТ ВОДОЁМА", RGB(255, 120, 100));
    } else if (t.road) {
        infoLine(x, y, w, "ЗАГРУЖЕННОСТЬ", pct(std::min(255, (int)traffic[i])), traffic[i] > 150 ? RGB(255, 120, 100) : RGB(255, 255, 255));
    } else {
        infoLine(x, y, w, "ВЫСОТА", strf("%d", t.h));
    }
    bool needP = pwrNeedOf(map_[tileOriginIdx(x0, y0)]) > 0 || t.zone;
    if (needP || t.bt > B_RUBBLE) infoLine(x, y, w, "ЭЛЕКТРИЧЕСТВО", (t.flags & F_POWER) ? "ЕСТЬ" : "НЕТ", (t.flags & F_POWER) ? RGB(150, 255, 150) : RGB(255, 120, 100));
    if (t.bt == B_GROW || t.zone) infoLine(x, y, w, "ВОДА", (t.flags & F_WATER) ? "ЕСТЬ" : "НЕТ", (t.flags & F_WATER) ? RGB(150, 255, 150) : RGB(255, 200, 120));
    if (t.zone || t.bt == B_GROW) infoLine(x, y, w, "ДОРОГА", access_[i] ? "ЕСТЬ" : "НЕТ", access_[i] ? RGB(150, 255, 150) : RGB(255, 120, 100));
    if (t.ter != T_WATER) {
        infoLine(x, y, w, "СТОИМОСТЬ ЗЕМЛИ", pct(landv[i]));
        infoLine(x, y, w, "ЗАГРЯЗНЕНИЕ", pct(pollution[i]), pollution[i] > 120 ? RGB(255, 150, 100) : RGB(255, 255, 255));
        infoLine(x, y, w, "ПРЕСТУПНОСТЬ", pct(crime[i]), crime[i] > 120 ? RGB(255, 150, 100) : RGB(255, 255, 255));
        infoLine(x, y, w, "ПОЖ. БЕЗОПАСНОСТЬ", pct(cov[C_FIRE][i]));
    }
    if (t.bt != B_NONE && y + 12 * U < oy + oh) textButton(x, oy + oh - 13 * U, w, 10 * U, BT_INFO_BULL, 0, "СНЕСТИ", false, RGB(120, 50, 45));
}
static void modalBg() { fillR(0, 0, SW, SH, ARGB(120, 0, 0, 0)); addBtn(0, 0, SW, SH, BT_BACKDROP, 0); }
static void drawMenu() {
    modalBg();
    int w = 120 * U, bh = 13 * U, n = 9, h = 22 * U + n * (bh + 2 * U);
    h = std::min(h, SH - 2 * padT);
    int x = (SW - w) / 2, y = (SH - h) / 2;
    panel(x, y, w, h);
    drawText(SW / 2, y + 5 * U, "«МОЙ ГОРОД»", U + 1, RGB(255, 230, 120), 1);
    const char* items[9] = {"ПРОДОЛЖИТЬ", "БЮДЖЕТ", "ДОСТИЖЕНИЯ", "СОХРАНИТЬ", "ЗАГРУЗИТЬ", "НОВАЯ КАРТА", "", "", "ВЫЙТИ"};
    int by = y + 9 * U + 7 * (U + 1);
    bh = std::min(bh, (y + h - by - 2 * U) / n - 2 * U);
    for (int k = 0; k < n; k++) {
        std::string s = items[k];
        if (k == 6) s = std::string("ВРЕМЕНА ГОДА: ") + (seasonsOn ? "ВКЛ" : "ВЫКЛ");
        if (k == 7) s = std::string("ЗВУК: ") + (soundOn ? "ВКЛ" : "ВЫКЛ");
        textButton(x + 6 * U, by + k * (bh + 2 * U), w - 12 * U, bh, BT_MITEM, k, s, false, k == 8 ? RGB(110, 50, 45) : RGB(52, 62, 86));
    }
    // статистика сбоку
    int sx = x + w + 4 * U;
    int pw2 = 132 * SU;
    if (sx + pw2 > SW - padR) sx = x - pw2 - 4 * U;
    if (sx > padL) {
        panel(sx, y, pw2, 9 * 9 * SU + 12 * U);
        int yy = y + 4 * U, xx = sx + 4 * U, ww = pw2 - 8 * U;
        drawText(xx, yy, "ГОРОД", SU, RGB(255, 230, 120)); yy += 10 * SU;
        infoLine(xx, yy, ww, "СТАТУС", TIER_NAME[city.tier]);
        infoLine(xx, yy, ww, "ЖИТЕЛИ", fmtInt(city.pop));
        infoLine(xx, yy, ww, "РАБОТА К/П", fmtInt(city.jobsC) + "/" + fmtInt(city.jobsI));
        infoLine(xx, yy, ww, "ЭНЕРГИЯ", strf("%d/%d", city.powerNeed, city.powerCap), city.powerNeed > city.powerCap ? RGB(255, 120, 100) : RGB(150, 255, 150));
        infoLine(xx, yy, ww, "ВОДА", strf("%d/%d", city.waterNeed, city.waterCap), city.waterNeed > city.waterCap ? RGB(255, 200, 120) : RGB(150, 255, 150));
        infoLine(xx, yy, ww, "ОЧИСТКА", strf("%d", city.sewageCap));
        infoLine(xx, yy, ww, "ЗЕРНО", strf("%u", city.seed));
        infoLine(xx, yy, ww, "СЛОТ", strf("%d%s", slot + 1, city.sandbox ? " ПЕСОЧН." : ""));
    }
}
static void drawBudget() {
    modalBg();
    int w = 170 * U, h = 140 * U; h = std::min(h, SH - 2 * padT); w = std::min(w, SW - padL - padR);
    int x = (SW - w) / 2, y = (SH - h) / 2;
    panel(x, y, w, h);
    button(x + w - 12 * U, y + 2 * U, 10 * U, 10 * U, BT_CLOSE, 0); iconAt(IC_CLOSE, x + w - 11 * U, y + 3 * U, 8 * U);
    drawText(x + 5 * U, y + 4 * U, "БЮДЖЕТ ГОРОДА", U, RGB(255, 230, 120));
    int yy = y + 16 * U;
    const char* nm[3] = {"НАЛОГ ЖИЛОЙ", "НАЛОГ ТОРГОВЫЙ", "НАЛОГ ПРОМЫШЛ."};
    const u32 col[3] = {RGB(80, 220, 90), RGB(80, 140, 240), RGB(240, 200, 50)};
    int lw = 64 * U, sw = w - lw - 26 * U;
    for (int k = 0; k < 3; k++) {
        drawText(x + 5 * U, yy + 2 * U, nm[k], SU, RGB(220, 225, 235));
        int tx = x + 5 * U + lw, tw = sw;
        fillR(tx, yy + 4 * U, tw, 2 * U, RGB(60, 66, 84));
        fillR(tx, yy + 4 * U, tw * city.tax[k] / 20, 2 * U, col[k]);
        int kx = tx + tw * city.tax[k] / 20 - 2 * U;
        fillR(kx, yy + U, 4 * U, 8 * U, RGB(240, 240, 245));
        addBtn(tx - 3 * U, yy - U, tw + 6 * U, 12 * U, BT_TAX, k, true);
        drawText(x + w - 5 * U, yy + 2 * U, strf("%d%%", city.tax[k]), SU, RGB(255, 255, 255), 2);
        yy += 13 * U;
    }
    yy += 2 * U;
    int cx = x + 5 * U, cw = (w - 15 * U) / 2;
    int y1 = yy;
    drawText(cx, yy, "ДОХОДЫ ЗА МЕСЯЦ", SU, RGB(150, 255, 150)); yy += 10 * SU;
    infoLine(cx, yy, cw, "ЖИЛЫЕ", fmtInt(city.incR));
    infoLine(cx, yy, cw, "ТОРГОВЛЯ", fmtInt(city.incC));
    infoLine(cx, yy, cw, "ПРОМЫШЛ.", fmtInt(city.incI));
    int y2 = y1, cx2 = cx + cw + 5 * U;
    drawText(cx2, y2, "РАСХОДЫ ЗА МЕСЯЦ", SU, RGB(255, 160, 140)); y2 += 10 * SU;
    infoLine(cx2, y2, cw, "СЛУЖБЫ", fmtInt(city.expServ));
    infoLine(cx2, y2, cw, "ДОРОГИ", fmtInt(city.expRoad));
    infoLine(cx2, y2, cw, "КОММУНАЛКА", fmtInt(city.expUtil));
    infoLine(cx2, y2, cw, "КРЕДИТ", fmtInt(city.expLoan));
    yy = std::max(yy, y2) + 2 * U;
    int net = city.incR + city.incC + city.incI - city.expServ - city.expRoad - city.expUtil - city.expLoan;
    infoLine(cx, yy, w - 10 * U, "ИТОГО ЗА МЕСЯЦ", (net >= 0 ? "+" : "") + fmtInt(net), net >= 0 ? RGB(150, 255, 150) : RGB(255, 120, 100));
    infoLine(cx, yy, w - 10 * U, "КАЗНА", moneyStr());
    infoLine(cx, yy, w - 10 * U, "ДОЛГ", fmtInt(city.debt) + " ₽", city.debt ? RGB(255, 200, 120) : RGB(255, 255, 255));
    int bw2 = (w - 15 * U) / 2, bh2 = 12 * U, by2 = std::min(yy + 3 * U, y + h - bh2 - 4 * U);
    textButton(cx, by2, bw2, bh2, BT_LOAN_TAKE, 0, "ВЗЯТЬ 10 000", false, RGB(50, 90, 60));
    textButton(cx2, by2, bw2, bh2, BT_LOAN_PAY, 0, "ВЕРНУТЬ 10 000", false, RGB(90, 70, 50));
}
static void drawAch() {
    modalBg();
    int rh = 9 * SU + 2 * U, w = std::min(SW - padL - padR, 2 * 26 * 6 * SU + 12 * U), h = 18 * U + (NACH + 1) / 2 * rh * 2; h = std::min(h, SH - 2 * padT);
    int x = (SW - w) / 2, y = (SH - h) / 2;
    panel(x, y, w, h);
    button(x + w - 12 * U, y + 2 * U, 10 * U, 10 * U, BT_CLOSE, 0); iconAt(IC_CLOSE, x + w - 11 * U, y + 3 * U, 8 * U);
    int got = 0; for (int k = 0; k < NACH; k++) got += (city.ach >> k) & 1;
    drawText(x + 5 * U, y + 4 * U, strf("ДОСТИЖЕНИЯ %d/%d", got, NACH), U, RGB(255, 230, 120));
    int cw = (w - 10 * U) / 2;
    for (int k = 0; k < NACH; k++) {
        int col = k % 2, yy = y + 15 * U + (k / 2) * rh * 2, xx = x + 5 * U + col * cw;
        if (yy + rh * 2 > y + h) break;
        bool on = (city.ach >> k) & 1;
        iconAt(IC_TROPHY, xx, yy, 7 * SU);
        if (!on) fillR(xx, yy, 7 * SU, 7 * SU, ARGB(170, 34, 40, 56));
        int sc = fitScale(ACH[k].name, SU, cw - 10 * SU);
        drawText(xx + 9 * SU, yy, ACH[k].name, sc, on ? RGB(255, 230, 120) : RGB(130, 135, 150));
        int sc2 = fitScale(ACH[k].desc, SU, cw - 10 * SU);
        drawText(xx + 9 * SU, yy + 9 * SU, ACH[k].desc, sc2, on ? RGB(220, 230, 240) : RGB(110, 115, 130));
    }
}
static void drawSlots(bool save) {
    modalBg();
    int w = 150 * U, bh = 18 * U, h = 22 * U + 3 * (bh + 3 * U); w = std::min(w, SW - padL - padR);
    int x = (SW - w) / 2, y = (SH - h) / 2;
    panel(x, y, w, h);
    button(x + w - 12 * U, y + 2 * U, 10 * U, 10 * U, BT_CLOSE, 0); iconAt(IC_CLOSE, x + w - 11 * U, y + 3 * U, 8 * U);
    drawText(x + 5 * U, y + 4 * U, save ? "СОХРАНИТЬ В СЛОТ" : "ЗАГРУЗИТЬ ИЗ СЛОТА", U, RGB(255, 230, 120));
    for (int s = 0; s < 3; s++) {
        int by = y + 16 * U + s * (bh + 3 * U);
        button(x + 5 * U, by, w - 10 * U, bh, BT_SLOT, s, s == slot);
        drawText(x + 9 * U, by + 3 * U, strf("СЛОТ %d", s + 1), U);
        std::string d = slotValid[s] ? strf("НАСЕЛЕНИЕ %s · ДЕНЬ %d%s", fmtInt(slotInfo[s].pop).c_str(), slotInfo[s].day + 1, slotInfo[s].sandbox ? " · ПЕСОЧНИЦА" : "") : std::string("ПУСТО");
        drawText(x + 9 * U, by + bh - 7 * SU - 3 * U, d, SU, slotValid[s] ? RGB(200, 215, 240) : RGB(130, 135, 150));
    }
}
static SDL_Texture* previewTex = nullptr; static u32 previewSeed = 0; static int previewType = -1;
static int newType = MT_RIVER, newSX = MAPN / 2, newSY = MAPN / 2;
// превью в изометрии: картинка 2N x N, ромб как в игре
static void buildPreview() {
    static u32 px[2 * MAPN * MAPN];
    static Tile save[NT]; memcpy(save, map_, sizeof map_);
    generateMap(newSeed, newType);
    findStartSpot(newSX, newSY);
    for (int py = 0; py < MAPN; py++) for (int pxx = 0; pxx < 2 * MAPN; pxx++) {
        float sx = pxx + 0.5f - MAPN, sy = py + 0.5f;
        int x = (int)floorf((sx + 2 * sy) * 0.5f), y = (int)floorf((2 * sy - sx) * 0.5f);
        u32 c = 0;
        if (inMap(x, y)) {
            const Tile& t = TL(x, y);
            if (t.ter == T_WATER) c = RGB(50, 110, 185); else if (t.ter == T_SAND) c = RGB(220, 202, 150);
            else { c = cshade(RGB(100, 165, 70), 0.80f + t.h * 0.09f); if (t.tree) c = cshade(RGB(50, 110, 50), 0.85f + t.h * 0.07f); }
        }
        px[py * 2 * MAPN + pxx] = c;
    }
    // восстанавливаем текущий город (карты симуляции не трогаем: generateMap их чистит)
    memcpy(map_, save, sizeof map_);
    fullRecalc();
    if (!previewTex) { previewTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 2 * MAPN, MAPN); if (previewTex) SDL_SetTextureBlendMode(previewTex, SDL_BLENDMODE_BLEND); }
    if (previewTex) SDL_UpdateTexture(previewTex, nullptr, px, 2 * MAPN * 4);
    previewSeed = newSeed; previewType = newType;
}
static SDL_Rect previewRect;
static void drawNew() {
    modalBg();
    int w = 280 * U, h = 150 * U; w = std::min(w, SW - padL - padR); h = std::min(h, SH - 2 * padT);
    int x = (SW - w) / 2, y = (SH - h) / 2;
    panel(x, y, w, h);
    button(x + w - 12 * U, y + 2 * U, 10 * U, 10 * U, BT_CLOSE, 0); iconAt(IC_CLOSE, x + w - 11 * U, y + 3 * U, 8 * U);
    drawText(x + 5 * U, y + 4 * U, "НОВЫЙ ГОРОД", U, RGB(255, 230, 120));
    if (previewSeed != newSeed || previewType != newType || !previewTex) buildPreview();
    int bh = 11 * U;
    // левая колонка: местность + карта
    int lw = w * 58 / 100, lx = x + 5 * U, yy = y + 15 * U;
    drawText(lx, yy, "МЕСТНОСТЬ:", SU, RGB(200, 210, 230)); yy += 9 * SU;
    int tb = (lw - (MT_COUNT - 1) * U) / MT_COUNT;
    for (int k = 0; k < MT_COUNT; k++) textButton(lx + k * (tb + U), yy, tb, bh, BT_MAPTYPE, k, MT_NAME[k], newType == k);
    yy += bh + 2 * U;
    int ph = std::min(lw / 2, y + h - yy - 12 * SU - 4 * U), pw = ph * 2;
    int pxl = lx + (lw - pw) / 2;
    previewRect = SDL_Rect{pxl, yy, pw, ph};
    if (previewTex) SDL_RenderCopy(ren, previewTex, nullptr, &previewRect);
    addBtn(pxl, yy, pw, ph, BT_PREVIEW, 0, true);
    // отметка места: ромб области 13x13 вокруг точки
    {
        float k = pw / (2.0f * MAPN);
        auto P = [&](float tx, float ty, int& ox, int& oy) { ox = pxl + (int)((tx - ty + MAPN) * k); oy = yy + (int)((tx + ty) * 0.5f * k); };
        int ax, ay, bx2, by2, cx2, cy2, dx2, dy2;
        P(newSX - 6.0f, newSY - 6.0f, ax, ay); P(newSX + 7.0f, newSY - 6.0f, bx2, by2); P(newSX + 7.0f, newSY + 7.0f, cx2, cy2); P(newSX - 6.0f, newSY + 7.0f, dx2, dy2);
        bool blink = fmod(gameClock, 1.0) < 0.6;
        SDL_SetRenderDrawColor(ren, 255, blink ? 60 : 230, 50, 255);
        for (int o = 0; o < std::max(1, U / 2); o++) {
            SDL_RenderDrawLine(ren, ax, ay - o, bx2 + o, by2); SDL_RenderDrawLine(ren, bx2 + o, by2, cx2, cy2 + o);
            SDL_RenderDrawLine(ren, cx2, cy2 + o, dx2 - o, dy2); SDL_RenderDrawLine(ren, dx2 - o, dy2, ax, ay - o);
        }
        int mx, my; P(newSX + 0.5f, newSY + 0.5f, mx, my);
        fillR(mx - U, my - 7 * U, U * 2 / 2 + 1, 7 * U, RGB(80, 60, 50));
        fillR(mx, my - 7 * U, 4 * U, 3 * U, RGB(230, 50, 45));
    }
    std::string tip = "НАЖМИТЕ НА КАРТУ, ЧТОБЫ ВЫБРАТЬ МЕСТО";
    drawText(lx + lw / 2, yy + ph + 2 * U, tip, fitScale(tip, SU, lw), RGB(200, 210, 230), 1);
    // правая колонка
    int cx = lx + lw + 6 * U, cw = x + w - 5 * U - cx; yy = y + 15 * U;
    drawText(cx, yy, strf("ЗЕРНО: %u", newSeed), SU); yy += 9 * SU;
    int b3 = (cw - 2 * U) / 3;
    textButton(cx, yy, b3, bh, BT_SEED, 0, "-"); textButton(cx + b3 + U, yy, b3, bh, BT_SEED, 1, "+"); textButton(cx + 2 * (b3 + U), yy, b3, bh, BT_SEED, 2, "?");
    yy += bh + 3 * U;
    textButton(cx, yy, cw, bh, BT_MODE, 0, newSandbox ? "ПЕСОЧНИЦА" : "ОБЫЧНЫЙ РЕЖИМ", newSandbox); yy += bh + 3 * U;
    drawText(cx, yy, "СЛОТ:", SU); yy += 9 * SU;
    for (int s = 0; s < 3; s++) textButton(cx + s * (b3 + U), yy, b3, bh, BT_NEWSLOT, s, strf("%d", s + 1), newSlot == s);
    yy += bh + 2 * U;
    if (slotValid[newSlot]) { std::string s = "СЛОТ БУДЕТ ПЕРЕЗАПИСАН"; drawText(cx, yy, s, fitScale(s, SU, cw), RGB(255, 180, 120)); }
    textButton(cx, y + h - bh - 5 * U, cw, bh, BT_CREATE, 0, "ОСНОВАТЬ ГОРОД", false, RGB(50, 100, 60));
}
static void drawMessages(float dt) {
    int y = padT + 18 * U;
    for (size_t k = 0; k < msgs.size(); k++) {
        Msg& m = msgs[k]; m.t -= dt;
        int a = clampi((int)(m.t * 255), 0, 255);
        int w = textW(m.text, SU) + 6 * U;
        int x = (SW - w) / 2;
        fillR(x, y, w, 7 * SU + 4 * U, ARGB(a * 3 / 4, 20, 24, 34));
        drawText(x + 3 * U, y + 2 * U, m.text, SU, calpha(m.col, a));
        y += 7 * SU + 6 * U;
    }
    for (size_t k = 0; k < msgs.size();) if (msgs[k].t <= 0) msgs.erase(msgs.begin() + k); else k++;
}
static void drawHint() {
    std::string h = hint;
    if (h.empty()) return;
    int w = textW(h, SU) + 6 * U, x = (SW - w) / 2;
    int y = SH - bottomH() - padB - 4 * U - 7 * SU - 4 * U;
    if (openCat >= 0) y -= 46 * U;
    fillR(x, y, w, 7 * SU + 4 * U, ARGB(190, 20, 24, 34));
    drawText(x + 3 * U, y + 2 * U, h, SU, RGB(230, 235, 245));
}
static void drawUI(float dt) {
    btns.clear();
    drawTopBar(); drawBottomBar(); drawInfo();
    if (layer) {
        std::string s = LAYER_NAME[layer];
        int w = textW(s, SU) + 6 * U; fillR(padL, padT + 17 * U, w, 7 * SU + 4 * U, ARGB(200, 20, 24, 34));
        drawText(padL + 3 * U, padT + 17 * U + 2 * U, s, SU, RGB(255, 235, 150));
    }
    drawHint(); drawMessages(dt);
    switch (panelOpen) {
        case P_MENU: drawMenu(); break; case P_BUDGET: drawBudget(); break; case P_ACH: drawAch(); break;
        case P_SAVE: drawSlots(true); break; case P_LOAD: drawSlots(false); break; case P_NEW: drawNew(); break;
    }
    // кнопки, добавленные позже, имеют приоритет (модальные окна поверх)
}
static int hitBtn(const std::vector<Btn>& v, int x, int y) {
    for (int k = (int)v.size() - 1; k >= 0; k--) { const SDL_Rect& r = v[k].r; if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h) return k; }
    return -1;
}
static void selectTool(const ToolDef& td) {
    curTool = td; infoTile = -1;
    if (td.kind == TK_PIPE) layer = 2;
    else if (td.kind == TK_PLINE || (td.kind == TK_BLD && isPowerProducer(td.arg))) layer = 1;
    else if (layer == 1 || layer == 2) layer = 0;
}
static void sliderSet(const Btn& b, int x, int y) {
    if (b.id == BT_PREVIEW) {
        float k = b.r.w / (2.0f * MAPN);
        float sx = (x - b.r.x) / k - MAPN, sy = (y - b.r.y) / k;
        int tx = (int)floorf((sx + 2 * sy) * 0.5f), ty = (int)floorf((2 * sy - sx) * 0.5f);
        newSX = clampi(tx, 8, MAPN - 9); newSY = clampi(ty, 8, MAPN - 9);
        return;
    }
    if (b.id == BT_TAX) {
        int tx = b.r.x + 3 * U, tw = b.r.w - 6 * U;
        city.tax[b.arg] = clampi((int)lroundf((x - tx) * 20.0f / tw), 0, 20);
    }
}
static void onButton(const Btn& b) {
    playSound(SND_CLICK);
    switch (b.id) {
        case BT_CAT: openCat = openCat == b.arg ? -1 : b.arg; layersOpen = false; break;
        case BT_TOOL: {
            const ToolDef& td = cats[openCat].tools[b.arg];
            if (toolLocked(td)) { notify(std::string("ОТКРОЕТСЯ НА СТАТУСЕ: ") + TIER_NAME[BD[td.arg].tier] + strf(" (%s ЖИТ.)", fmtInt(TIER_POP[BD[td.arg].tier]).c_str()), RGB(255, 180, 120)); break; }
            selectTool(td); openCat = -1; break;
        }
        case BT_SPEED: if (panelOpen == P_NONE) speed = b.arg; break;
        case BT_MENU: openPanel(P_MENU); break;
        case BT_LAYERS: layersOpen = !layersOpen; openCat = -1; break;
        case BT_LAYER: layer = b.arg; layersOpen = false; break;
        case BT_UNDO: undoLast(); break;
        case BT_INSPECT: selectTool(ToolDef{TK_INSPECT, 0, "ОСМОТР"}); openCat = -1; layersOpen = false; break;
        case BT_BULL: selectTool(ToolDef{TK_BULL, 0, "СНОС"}); openCat = -1; layersOpen = false; break;
        case BT_BUDGET: openPanel(panelOpen == P_BUDGET ? P_NONE : P_BUDGET); break;
        case BT_CLOSE: if (panelOpen != P_NONE) openPanel(panelOpen == P_MENU ? P_NONE : (panelOpen == P_BUDGET || panelOpen == P_ACH || panelOpen == P_SAVE || panelOpen == P_LOAD || panelOpen == P_NEW) ? P_MENU : P_NONE); else infoTile = -1;
            break;
        case BT_BACKDROP: break;
        case BT_MITEM:
            switch (b.arg) {
                case 0: openPanel(P_NONE); break; case 1: panelOpen = P_BUDGET; break; case 2: panelOpen = P_ACH; break;
                case 3: panelOpen = P_SAVE; refreshSlots(); break; case 4: panelOpen = P_LOAD; refreshSlots(); break;
                case 5: panelOpen = P_NEW; newSeed = city.seed + 1; newSandbox = city.sandbox; newSlot = slot; refreshSlots(); break;
                case 6: seasonsOn = !seasonsOn; saveConfig(); break; case 7: soundOn = !soundOn; saveConfig(); break;
                case 8: quitReq = true; break;
            }
            break;
        case BT_LOAN_TAKE:
            if (city.sandbox) break;
            if (city.debt >= 100000) { notify("ЛИМИТ КРЕДИТА 100 000"); break; }
            city.debt += 10000; city.money += 10000; playSound(SND_COIN); break;
        case BT_LOAN_PAY:
            if (city.debt <= 0) break;
            { i64 p = std::min<i64>(10000, city.debt); if (city.money < p) { notify("НЕДОСТАТОЧНО ДЕНЕГ"); break; } city.money -= p; city.debt -= p; playSound(SND_COIN); }
            break;
        case BT_SLOT:
            if (panelOpen == P_SAVE) { slot = b.arg; if (saveGame(slot)) notify(strf("СОХРАНЕНО В СЛОТ %d", slot + 1), RGB(150, 255, 150)); else notify("ОШИБКА СОХРАНЕНИЯ", RGB(255, 120, 100)); saveConfig(); refreshSlots(); }
            else if (slotValid[b.arg]) {
                if (loadGame(b.arg)) { slot = b.arg; saveConfig(); notify(strf("ЗАГРУЖЕН СЛОТ %d", slot + 1), RGB(150, 255, 150)); panelOpen = P_MENU; openPanel(P_NONE); speed = 1; }
                else notify("ОШИБКА ЗАГРУЗКИ", RGB(255, 120, 100));
            }
            break;
        case BT_SEED: if (b.arg == 0) newSeed--; else if (b.arg == 1) newSeed++; else newSeed = hash32(SDL_GetTicks() ^ newSeed) % 1000000; break;
        case BT_MODE: newSandbox = !newSandbox; break;
        case BT_MAPTYPE: newType = b.arg; break;
        case BT_NEWSLOT: newSlot = b.arg; break;
        case BT_CREATE:
            saveGame(slot);
            slot = newSlot; newGame(newSeed, newSandbox, newType, newSX, newSY); saveGame(slot); saveConfig();
            panelOpen = P_MENU; openPanel(P_NONE); speed = 1; prevSpeed = 1;
            break;
        case BT_INFO_BULL:
            if (infoTile >= 0) {
                ToolDef old = curTool; int lay = layer; curTool = ToolDef{TK_BULL, 0, "СНОС"}; layer = 0;
                dragSX = dragEX = infoTile % MAPN; dragSY = dragEY = infoTile / MAPN;
                applyTool(); curTool = old; layer = lay; infoTile = -1; pv.active = false;
            }
            break;
    }
}
static void updateHint() {
    hint.clear();
    if (panelOpen != P_NONE) return;
    switch (curTool.kind) {
        case TK_INSPECT: hint = ""; break;
        case TK_ROAD: hint = std::string(ROAD_NAME[curTool.arg]) + ": ПРОВЕДИТЕ ПАЛЬЦЕМ. ДВА ПАЛЬЦА — КАРТА"; break;
        case TK_ZONE: hint = std::string(ZT_NAME[(curTool.arg - 1) / 3]) + " ЗОНА: ВЫДЕЛИТЕ ПРЯМОУГОЛЬНИК"; break;
        case TK_BLD: hint = std::string(BD[curTool.arg].name) + ": КОСНИТЕСЬ МЕСТА И ОТПУСТИТЕ"; break;
        case TK_BULL: hint = layer == 2 ? "СНОС ТРУБ: ВЫДЕЛИТЕ ОБЛАСТЬ" : "СНОС: ВЫДЕЛИТЕ ОБЛАСТЬ"; break;
        case TK_PLINE: hint = "ЛЭП: ПРОВЕДИТЕ ОТ СТАНЦИИ К ЗОНАМ"; break;
        case TK_PIPE: hint = "ТРУБЫ: ПРОВЕДИТЕ ОТ ВОДОКАЧКИ. ВОДА ДОХОДИТ НА 5 КЛЕТОК"; break;
        default: hint = std::string(curTool.name) + ": ВЫДЕЛИТЕ ОБЛАСТЬ"; break;
    }
    if (pv.active && curTool.kind != TK_INSPECT) {
        int cost = 0;
        if (curTool.kind == TK_BLD) evalBuilding(dragEX, dragEY, false, cost); else cost = evalTiles(false);
        hint = strf("СТОИМОСТЬ: %s ₽", fmtInt(cost).c_str());
        if (!canAfford(cost)) hint += " — НЕ ХВАТАЕТ ДЕНЕГ";
    }
}

// ---------------------------------------------------------------------
//  Ввод: касания и мышь
// ---------------------------------------------------------------------
struct Finger { i64 id; float x, y, sx, sy; bool ui; };
static std::vector<Finger> fingers;
enum { G_NONE, G_PAN, G_BUILD, G_PINCH, G_UI, G_SLIDER };
static int gesture = G_NONE;
static bool moved = false;
static float pinchD0 = 1, pinchZ0 = 1, pinchWX = 0, pinchWY = 0;
static float panLX = 0, panLY = 0;
static Finger* findF(i64 id) { for (size_t k = 0; k < fingers.size(); k++) if (fingers[k].id == id) return &fingers[k]; return nullptr; }
static void clampCam() {
    camX = clampf(camX, -MAPN * 32.0f, MAPN * 32.0f); camY = clampf(camY, -40.0f, MAPN * 32.0f + 40);
    zoom = clampf(zoom, 0.4f, 3.0f);
}
static void beginPinch() {
    Finger& a = fingers[0]; Finger& b = fingers[1];
    pinchD0 = std::max(10.0f, hypotf(a.x - b.x, a.y - b.y)); pinchZ0 = zoom;
    s2w((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, pinchWX, pinchWY);
    gesture = G_PINCH; pv.active = false; moved = true;
}
static void pointerDown(i64 id, float x, float y) {
    Finger f; f.id = id; f.x = f.sx = x; f.y = f.sy = y; f.ui = false;
    if (gesture == G_UI || gesture == G_SLIDER) return;
    int hb = hitBtn(btns, (int)x, (int)y);
    if (fingers.empty() && hb >= 0) {
        f.ui = true; fingers.push_back(f);
        pressedOn = true; pressedB = btns[hb];
        if (pressedB.slider) { gesture = G_SLIDER; sliderSet(pressedB, (int)x, (int)y); } else gesture = G_UI;
        return;
    }
    if (panelOpen != P_NONE) return;
    fingers.push_back(f);
    if (fingers.size() == 1) {
        moved = false; panLX = x; panLY = y;
        if (curTool.kind == TK_INSPECT) gesture = G_PAN;
        else {
            gesture = G_BUILD;
            int tx, ty; pickTile(x, y, tx, ty);
            dragSX = dragEX = tx; dragSY = dragEY = ty; flatH = TL(tx, ty).ter == T_WATER ? 1 : TL(tx, ty).h;
            pv.active = true;
            if (curTool.kind == TK_BLD) { int c; evalBuilding(tx, ty, false, c); } else evalTiles(false);
        }
        layersOpen = false; openCat = -1;
    } else if (fingers.size() == 2) beginPinch();
}
static void pointerMove(i64 id, float x, float y) {
    Finger* f = findF(id); if (!f) return;
    f->x = x; f->y = y;
    float thr = 6.0f * U;
    if (gesture == G_SLIDER) { sliderSet(pressedB, (int)x, (int)y); return; }
    if (f->ui) return;
    if (hypotf(x - f->sx, y - f->sy) > thr) moved = true;
    if (gesture == G_PAN && moved) {
        camX -= (x - panLX) / zoom; camY -= (y - panLY) / zoom; panLX = x; panLY = y; clampCam();
    } else if (gesture == G_PAN) { /* ждём порога */ }
    else if (gesture == G_PINCH && fingers.size() >= 2) {
        Finger& a = fingers[0]; Finger& b = fingers[1];
        float d = std::max(10.0f, hypotf(a.x - b.x, a.y - b.y));
        zoom = clampf(pinchZ0 * d / pinchD0, 0.4f, 3.0f);
        float mx = (a.x + b.x) * 0.5f, my = (a.y + b.y) * 0.5f;
        camX = pinchWX - (mx - SW * 0.5f) / zoom; camY = pinchWY - (my - SH * 0.5f) / zoom; clampCam();
    } else if (gesture == G_BUILD) {
        int tx, ty; pickTile(x, y, tx, ty);
        if (tx != dragEX || ty != dragEY) {
            if (curTool.kind == TK_BLD) { dragSX = tx; dragSY = ty; }
            dragEX = tx; dragEY = ty;
            if (curTool.kind == TK_BLD) { int c; evalBuilding(tx, ty, false, c); } else evalTiles(false);
        }
    }
}
static void pointerUp(i64 id, float x, float y) {
    Finger* f = findF(id); if (!f) return;
    bool ui = f->ui;
    for (size_t k = 0; k < fingers.size(); k++) if (fingers[k].id == id) { fingers.erase(fingers.begin() + k); break; }
    if (ui) {
        if (pressedOn && gesture != G_SLIDER) {
            int hb = hitBtn(btns, (int)x, (int)y);
            if (hb >= 0 && btns[hb].id == pressedB.id && btns[hb].arg == pressedB.arg) { Btn b = pressedB; pressedOn = false; onButton(b); }
        }
        pressedOn = false;
    }
    if (ui) { if (fingers.empty()) gesture = G_NONE; return; }
    if (gesture == G_BUILD && fingers.empty()) {
        pv.active = false;
        applyTool();
        gesture = G_NONE;
    } else if (gesture == G_PAN && fingers.empty()) {
        if (!moved) {
            int tx, ty; pickTile(x, y, tx, ty);
            infoTile = ty * MAPN + tx; playSound(SND_CLICK);
        }
        gesture = G_NONE;
    } else if (gesture == G_PINCH) {
        if (fingers.size() == 1) { gesture = G_PAN; moved = true; panLX = fingers[0].x; panLY = fingers[0].y; fingers[0].sx = -9999; }
        else if (fingers.empty()) gesture = G_NONE;
    }
    if (fingers.empty()) gesture = G_NONE;
}
static void onBack() {
    playSound(SND_CLICK);
    if (panelOpen == P_BUDGET || panelOpen == P_ACH || panelOpen == P_SAVE || panelOpen == P_LOAD || panelOpen == P_NEW) { panelOpen = P_MENU; return; }
    if (panelOpen == P_MENU) { openPanel(P_NONE); return; }
    if (infoTile >= 0) { infoTile = -1; return; }
    if (openCat >= 0 || layersOpen) { openCat = -1; layersOpen = false; return; }
    if (curTool.kind != TK_INSPECT) { selectTool(ToolDef{TK_INSPECT, 0, "ОСМОТР"}); return; }
    openPanel(P_MENU);
}
static void computeUIScale() {
    SDL_GetRendererOutputSize(ren, &SW, &SH);
    U = std::max(2, (int)lroundf(SH / 260.0f));
    SU = std::max(1, (U * 2 + 1) / 3);
    if (SU < 2 && U >= 2) SU = 2;
    if (U >= 4) SU = U - 1;
    // вырез камеры: в горизонтальной ориентации он сбоку
#ifdef __ANDROID__
    padL = padR = std::max(8, (int)(SW * 0.045f)); padT = std::max(8, (int)(SH * 0.035f)); padB = std::max(4, SH / 90);
#else
    padL = padR = 8; padT = 8; padB = 6;
#endif
    // если интерфейс не помещается по ширине — уменьшаем
    while (U > 2 && 12 * (std::min(30 * U, (SW - padL - padR) / 12 - U) + U) < 12 * 20 * U && (SW - padL - padR) / 12 < 22 * U) U--;
}

// ---------------------------------------------------------------------
//  Главный цикл
// ---------------------------------------------------------------------
static SDL_mutex* gMtx = nullptr;
static volatile bool savedOnBg = false;
static int SDLCALL appEventWatch(void*, SDL_Event* e) {
    if (e->type == SDL_APP_WILLENTERBACKGROUND || e->type == SDL_APP_TERMINATING) {
        if (gMtx) SDL_LockMutex(gMtx);
        saveGame(slot); saveConfig(); savedOnBg = true;
        if (gMtx) SDL_UnlockMutex(gMtx);
    }
    return 0;
}
static float mouseK() { int w = 0, h = 0; SDL_GetWindowSize(win, &w, &h); return w > 0 ? (float)SW / w : 1.0f; }
static void initRenderer() {
    genAllSprites();
    uploadPages();
    if (previewTex) { SDL_DestroyTexture(previewTex); previewTex = nullptr; }
    previewSeed = 0;
}
static void frame(float dt) {
    gtime += dt;
    simUpdate(dt);
    spawnCars(); updateCars(dt); updatePeds(dt); updateParticles(speed ? dt : 0);
    ambVol = clampf(city.pop / 20000.0f, 0.0f, 1.0f) * 0.25f + (cars.empty() ? 0.0f : 0.03f);
    updateHint();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 22, 40, 62, 255);
    SDL_RenderClear(ren);
    renderWorld();
    drawUI(dt);
}
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }
    }
#ifdef __ANDROID__
    win = SDL_CreateWindow("Мой город", 0, 0, 0, 0, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    const char* ip = SDL_AndroidGetInternalStoragePath();
    basePath = ip ? std::string(ip) + "/" : std::string("");
#else
    win = SDL_CreateWindow("Мой город", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    { char* bp = SDL_GetBasePath(); basePath = bp ? bp : ""; if (bp) SDL_free(bp); }
#endif
    if (!win) { SDL_Log("SDL_CreateWindow: %s", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { SDL_Log("SDL_CreateRenderer: %s", SDL_GetError()); return 1; }
    SDL_RendererInfo ri; bool vsync = false;
    if (SDL_GetRendererInfo(ren, &ri) == 0) {
        vsync = (ri.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
        if (ri.max_texture_width > 0 && ri.max_texture_width < PGS) PGS = ri.max_texture_width;
    }
    initRenderer();
    initTools();
    computeUIScale();
    initAudio();
    for (int i = 0; i < NT; i++) carHead[i] = -1;
    loadConfig();
    if (!loadGame(slot)) {
        // первый запуск: сразу предлагаем выбрать местность и место для города
        newGame(hash32(SDL_GetTicks() ^ 0xA5A5u) % 1000000, false);
        newSeed = city.seed; newSlot = slot; newSandbox = false;
        openPanel(P_NEW); prevSpeed = 1;
    }
    else notify("ГОРОД ЗАГРУЖЕН", RGB(150, 255, 150));
    gMtx = SDL_CreateMutex();
    SDL_AddEventWatch(appEventWatch, nullptr);

    Uint64 freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
    Uint32 lastSave = SDL_GetTicks();
    bool rightDrag = false; int mlx = 0, mly = 0;
    bool running = true, background = false;
    while (running) {
        if (gMtx) SDL_LockMutex(gMtx);
        SDL_Event e;
        for (;;) {
            // мьютекс отпускаем на время опроса: событие сворачивания приходит из потока Java
            if (gMtx) SDL_UnlockMutex(gMtx);
            int got = SDL_PollEvent(&e);
            if (gMtx) SDL_LockMutex(gMtx);
            if (!got) break;
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_APP_DIDENTERBACKGROUND: background = true; if (audioDev) SDL_PauseAudioDevice(audioDev, 1); break;
                case SDL_APP_DIDENTERFOREGROUND: background = false; if (audioDev) SDL_PauseAudioDevice(audioDev, 0); last = SDL_GetPerformanceCounter(); fingers.clear(); gesture = G_NONE; pv.active = false; break;
                case SDL_RENDER_DEVICE_RESET: initRenderer(); break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED) computeUIScale();
                    break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_AC_BACK || e.key.keysym.sym == SDLK_ESCAPE) onBack();
                    else if (e.key.keysym.sym == SDLK_SPACE && panelOpen == P_NONE) speed = speed ? 0 : 1;
                    else if (e.key.keysym.sym == SDLK_LEFT) camX -= 64 / zoom; else if (e.key.keysym.sym == SDLK_RIGHT) camX += 64 / zoom;
                    else if (e.key.keysym.sym == SDLK_UP) camY -= 64 / zoom; else if (e.key.keysym.sym == SDLK_DOWN) camY += 64 / zoom;
                    break;
                case SDL_FINGERDOWN: pointerDown((i64)e.tfinger.fingerId, e.tfinger.x * SW, e.tfinger.y * SH); break;
                case SDL_FINGERMOTION: pointerMove((i64)e.tfinger.fingerId, e.tfinger.x * SW, e.tfinger.y * SH); break;
                case SDL_FINGERUP: pointerUp((i64)e.tfinger.fingerId, e.tfinger.x * SW, e.tfinger.y * SH); break;
                case SDL_MOUSEBUTTONDOWN: {
                    if (e.button.which == SDL_TOUCH_MOUSEID) break;
                    if (e.button.button == SDL_BUTTON_LEFT) pointerDown(-1000, e.button.x * mouseK(), e.button.y * mouseK());
                    else if (e.button.button == SDL_BUTTON_RIGHT) { rightDrag = true; mlx = e.button.x; mly = e.button.y; }
                    break;
                }
                case SDL_MOUSEMOTION:
                    if (e.motion.which == SDL_TOUCH_MOUSEID) break;
                    if (rightDrag) { camX -= (e.motion.x - mlx) * mouseK() / zoom; camY -= (e.motion.y - mly) * mouseK() / zoom; mlx = e.motion.x; mly = e.motion.y; clampCam(); }
                    if (findF(-1000)) pointerMove(-1000, e.motion.x * mouseK(), e.motion.y * mouseK());
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.which == SDL_TOUCH_MOUSEID) break;
                    if (e.button.button == SDL_BUTTON_LEFT) pointerUp(-1000, e.button.x * mouseK(), e.button.y * mouseK());
                    else if (e.button.button == SDL_BUTTON_RIGHT) rightDrag = false;
                    break;
                case SDL_MOUSEWHEEL: {
                    if (e.wheel.which == SDL_TOUCH_MOUSEID || panelOpen != P_NONE) break;
                    int mx = SW / 2, my = SH / 2;
                    float wx, wy; s2w((float)mx, (float)my, wx, wy);
                    zoom = clampf(zoom * (e.wheel.y > 0 ? 1.15f : 1 / 1.15f), 0.4f, 3.0f);
                    camX = wx - (mx - SW * 0.5f) / zoom; camY = wy - (my - SH * 0.5f) / zoom; clampCam();
                    break;
                }
            }
        }
        if (quitReq) running = false;
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / (double)freq); last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (!background) {
            frame(dt);
            SDL_RenderPresent(ren);
            if (!vsync) { float el = (float)((SDL_GetPerformanceCounter() - now) / (double)freq); if (el < 1 / 60.0f) SDL_Delay((Uint32)((1 / 60.0f - el) * 1000)); }
        } else SDL_Delay(50);
        if (SDL_GetTicks() - lastSave > 60000) { lastSave = SDL_GetTicks(); if (saveGame(slot)) {} }
        if (gMtx) SDL_UnlockMutex(gMtx);
    }
    saveGame(slot); saveConfig();
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
