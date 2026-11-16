// Sabak Voice - formant speech synthesiser (Klatt-style cascade/parallel) with lip-sync track.
#include "voice.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <algorithm>
#include <functional>

namespace sk {
namespace {
const float PI = 3.14159265358979f;
float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float sstep(float t) { t = clampf(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }

// ------------------------------------------------------------------ parameters
enum { AV, AH, AF, NAS, F1, F2, F3, B1, B2, B3, FF, FB, JAW, WIDE, RND, CLS, FVL, TNG, TTH, NP };
struct Params { float v[NP]; Params() { for (float& x : v) x = 0.f; } };
const float NAN_F = -1.f;  // marker: formant to be inherited

enum Type { T_V, T_D, T_A, T_N, T_S, T_F, T_C, T_H, T_G, T_T };
struct Ph {
  Type type; bool voiced; float f1, f2, f3, g1, g2, g3; float dur; float nf, nbw, namp;
  char mouth; bool emph; bool arab;
};
std::map<std::string, Ph> g_ph;
void V(const char* s, float f1, float f2, float f3, float dur, bool arab = false) { g_ph[s] = {T_V, true, f1, f2, f3, 0, 0, 0, dur, 0, 0, 0, 'v', false, arab}; }
void D(const char* s, float f1, float f2, float f3, float g1, float g2, float g3, float dur) { g_ph[s] = {T_D, true, f1, f2, f3, g1, g2, g3, dur, 0, 0, 0, 'v', false, false}; }
void A(const char* s, float f1, float f2, float f3, float dur, char m) { g_ph[s] = {T_A, true, f1, f2, f3, 0, 0, 0, dur, 0, 0, 0, m, false, false}; }
void N(const char* s, float f1, float f2, float f3, float dur) { g_ph[s] = {T_N, true, f1, f2, f3, 0, 0, 0, dur, 0, 0, 0, s[0] == 'M' ? 'b' : 'a', false, false}; }
void S(const char* s, bool voiced, float nf, float nbw, float namp, char m, bool emph = false) { g_ph[s] = {T_S, voiced, NAN_F, NAN_F, NAN_F, 0, 0, 0, 60, nf, nbw, namp, m, emph, false}; }
void F(const char* s, bool voiced, float nf, float nbw, float namp, char m, float dur = 95, bool emph = false) { g_ph[s] = {T_F, voiced, NAN_F, NAN_F, NAN_F, 0, 0, 0, dur, nf, nbw, namp, m, emph, false}; }
void C(const char* s, bool voiced, float nf, float nbw, float namp, char m) { g_ph[s] = {T_C, voiced, NAN_F, NAN_F, NAN_F, 0, 0, 0, 85, nf, nbw, namp, m, false, false}; }

void initTable() {
  if (!g_ph.empty()) return;
  // English vowels (male reference formants)
  V("IY", 270, 2290, 3010, 135); V("IH", 390, 1990, 2550, 95); V("EH", 530, 1840, 2480, 110); V("AE", 660, 1720, 2410, 140);
  V("AA", 730, 1090, 2440, 140); V("AO", 570, 840, 2410, 140); V("UH", 440, 1020, 2240, 95); V("UW", 300, 870, 2240, 140);
  V("AH", 640, 1190, 2390, 100); V("ER", 490, 1350, 1690, 140); V("AX", 500, 1500, 2500, 65);
  D("EY", 500, 1900, 2500, 340, 2250, 2900, 175); D("AY", 720, 1250, 2450, 400, 1950, 2600, 190); D("OY", 560, 880, 2400, 380, 1950, 2550, 200);
  D("AW", 720, 1230, 2450, 440, 1050, 2250, 190); D("OW", 520, 950, 2350, 400, 830, 2250, 170);
  // Arabic vowels
  V("A", 690, 1450, 2500, 85, true); V("AAA", 740, 1250, 2500, 175, true); V("I", 400, 2000, 2600, 85, true); V("II", 300, 2250, 2950, 170, true);
  V("U", 420, 1050, 2300, 85, true); V("UU", 310, 900, 2250, 170, true);
  // approximants / nasals
  A("W", 290, 610, 2150, 65, 'w'); A("Y", 260, 2070, 3020, 60, 'y'); A("R", 330, 1060, 1380, 75, 'r'); A("L", 370, 1150, 2800, 70, 'l');
  A("RT", 400, 1500, 2400, 38, 'a'); A("AYN", 720, 1150, 2300, 85, 'p');
  N("M", 250, 1100, 2200, 75); N("N", 250, 1650, 2600, 70); N("NG", 250, 2000, 2700, 75);
  // stops (burst noise centre / bandwidth / amp)
  S("P", false, 1100, 1800, 0.22f, 'b'); S("B", true, 900, 1600, 0.18f, 'b');
  S("T", false, 4300, 2200, 0.34f, 'a'); S("D", true, 4000, 2200, 0.26f, 'a');
  S("K", false, 2300, 1500, 0.30f, 'k'); S("G", true, 2100, 1400, 0.24f, 'k');
  S("TT", false, 3600, 2000, 0.34f, 'a', true); S("DD", true, 3400, 2000, 0.26f, 'a', true);
  S("Q", false, 1500, 1200, 0.26f, 'k', true);
  // fricatives
  F("F", false, 4500, 6000, 0.12f, 'f'); F("V", true, 4500, 6000, 0.09f, 'f', 75);
  F("TH", false, 4500, 6000, 0.10f, 'd'); F("DH", true, 4500, 6000, 0.08f, 'd', 60);
  F("S", false, 7000, 2200, 0.55f, 's'); F("Z", true, 7000, 2200, 0.40f, 's', 85);
  F("SH", false, 3300, 1500, 0.55f, 'h'); F("ZH", true, 3300, 1500, 0.40f, 'h', 85);
  F("SS", false, 5200, 2200, 0.55f, 's', 105, true); F("ZZ", true, 5200, 2200, 0.40f, 's', 90, true);
  F("KH", false, 2300, 1400, 0.40f, 'k'); F("GH", true, 2000, 1300, 0.30f, 'k', 80);
  F("HX", false, 2800, 1500, 0.30f, 'p', 100, true);
  C("CH", false, 3300, 1500, 0.50f, 'h'); C("JH", true, 3300, 1500, 0.38f, 'h');
  g_ph["HH"] = {T_H, false, NAN_F, NAN_F, NAN_F, 0, 0, 0, 70, 0, 0, 0, 'n', false, false};
  g_ph["GS"] = {T_G, false, NAN_F, NAN_F, NAN_F, 0, 0, 0, 45, 0, 0, 0, 'n', false, false};
}

// ------------------------------------------------------------------ DSP
struct Res {
  float a = 1, b = 0, c = 0, y1 = 0, y2 = 0;
  void set(float f, float bw, float sr) {
    f = clampf(f, 30.f, sr * 0.45f);
    float cc = -std::exp(-2.f * PI * bw / sr), bb = 2.f * std::exp(-PI * bw / sr) * std::cos(2.f * PI * f / sr);
    c = cc; b = bb; a = 1.f - bb - cc;
  }
  float run(float x) { float y = a * x + b * y1 + c * y2; y2 = y1; y1 = y; return y; }
};
struct Anti {  // anti-resonator (zero pair)
  float a = 1, b = 0, c = 0, x1 = 0, x2 = 0;
  void set(float f, float bw, float sr) {
    float cc = -std::exp(-2.f * PI * bw / sr), bb = 2.f * std::exp(-PI * bw / sr) * std::cos(2.f * PI * f / sr), aa = 1.f - bb - cc;
    a = 1.f / aa; b = -bb / aa; c = -cc / aa;
  }
  float run(float x) { float y = a * x + b * x1 + c * x2; x2 = x1; x1 = x; return y; }
};
struct Rng {
  unsigned s;
  explicit Rng(unsigned seed) : s(seed * 2654435761u + 12345u) {}
  unsigned next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  float uni() { return (next() & 0xFFFFFF) / 16777216.f; }
  float noise() { return uni() + uni() + uni() + uni() - 2.f; }  // ~gaussian, std ~0.58
};

// ------------------------------------------------------------------ segments
struct Seg {
  float t0 = 0, t1 = 0;
  Params a, b;
  float tf = 0.05f;      // formant transition time
  int inh = 0;           // 0 none, 1 inherit prev formants, 2 inherit next
  bool stressed = false, vowel = false, finalVowel = false;
  int word = -1;
  float pitchBump = 0;   // for accents
};

struct Tok { std::vector<std::string> ph; std::string text; bool arabic = false; int punct = 0; bool raw = false; };
// punct: 0 none (word), '.' ',' '?' '!' ':' pause

bool isVowelSym(const std::string& s) { auto it = g_ph.find(s); return it != g_ph.end() && (it->second.type == T_V || it->second.type == T_D); }
std::string baseSym(const std::string& s, int* stress) {
  std::string r = s; *stress = -1;
  if (!r.empty() && isdigit((unsigned char)r.back())) { *stress = r.back() - '0'; r.pop_back(); }
  return r;
}

// ------------------------------------------------------------------ English G2P
const std::map<std::string, std::string>& dict() {
  static std::map<std::string, std::string> d = {
    {"the", "DH AX"}, {"a", "AX"}, {"an", "AX N"}, {"to", "T UW"}, {"of", "AH V"}, {"and", "AE N D"}, {"in", "IH N"}, {"is", "IH Z"},
    {"are", "AA R"}, {"was", "W AH Z"}, {"you", "Y UW"}, {"your", "Y AO R"}, {"we", "W IY"}, {"i", "AY"}, {"he", "HH IY"}, {"she", "SH IY"},
    {"it", "IH T"}, {"they", "DH EY"}, {"this", "DH IH S"}, {"that", "DH AE T"}, {"with", "W IH DH"}, {"for", "F AO R"}, {"from", "F R AH M"},
    {"will", "W IH L"}, {"be", "B IY"}, {"have", "HH AE V"}, {"has", "HH AE Z"}, {"do", "D UW"}, {"does", "D AH Z"}, {"what", "W AH T"},
    {"who", "HH UW"}, {"how", "HH AW"}, {"why", "W AY"}, {"when", "W EH N"}, {"where", "W EH R"}, {"there", "DH EH R"}, {"here", "HH IY R"},
    {"hello", "HH AH L OW1"}, {"good", "G UH1 D"}, {"morning", "M AO1 R N IH NG"}, {"class", "K L AE1 S"}, {"students", "S T UW1 D AH N T S"},
    {"student", "S T UW1 D AH N T"}, {"teacher", "T IY1 CH ER"}, {"today", "T AH D EY1"}, {"learn", "L ER1 N"}, {"about", "AX B AW1 T"},
    {"book", "B UH1 K"}, {"books", "B UH1 K S"}, {"read", "R IY1 D"}, {"lesson", "L EH1 S AH N"}, {"listen", "L IH1 S AH N"},
    {"please", "P L IY1 Z"}, {"open", "OW1 P AH N"}, {"page", "P EY1 JH"}, {"turn", "T ER1 N"}, {"welcome", "W EH1 L K AH M"},
    {"science", "S AY1 AH N S"}, {"carefully", "K EH1 R F AH L IY"}, {"question", "K W EH1 S CH AH N"}, {"questions", "K W EH1 S CH AH N Z"},
    {"answer", "AE1 N S ER"}, {"write", "R AY1 T"}, {"look", "L UH1 K"}, {"now", "N AW1"}, {"thank", "TH AE1 NG K"}, {"thanks", "TH AE1 NG K S"},
    {"very", "V EH1 R IY"}, {"well", "W EH1 L"}, {"yes", "Y EH1 S"}, {"no", "N OW1"}, {"one", "W AH1 N"}, {"two", "T UW1"}, {"three", "TH R IY1"},
    {"new", "N UW1"}, {"world", "W ER1 L D"}, {"my", "M AY"}, {"me", "M IY"}, {"our", "AW ER"}, {"all", "AO1 L"}, {"see", "S IY1"},
    {"know", "N OW1"}, {"understand", "AH N D ER S T AE1 N D"}, {"together", "T AH G EH1 DH ER"}, {"let", "L EH1 T"}, {"us", "AH S"},
    {"start", "S T AA1 R T"}, {"sit", "S IH1 T"}, {"down", "D AW1 N"}, {"quiet", "K W AY1 AH T"}, {"history", "HH IH1 S T ER IY"},
    {"math", "M AE1 TH"}, {"great", "G R EY1 T"}, {"work", "W ER1 K"}, {"homework", "HH OW1 M W ER K"}, {"school", "S K UW1 L"},
    {"story", "S T AO1 R IY"}, {"tomorrow", "T AH M AA1 R OW"}, {"again", "AH G EH1 N"}, {"next", "N EH1 K S T"}, {"chapter", "CH AE1 P T ER"},
    {"detective", "D IH T EH1 K T IH V"}, {"violet", "V AY1 AH L AH T"}, {"files", "F AY1 L Z"}, {"file", "F AY1 L"}};
  return d;
}
bool isFuncWord(const std::string& w) {
  static const char* f[] = {"the", "a", "an", "to", "of", "and", "in", "is", "are", "was", "you", "your", "we", "i", "he", "she", "it", "they", "this", "that", "with", "for", "from", "will", "be", "have", "has", "do", "does", "my", "me", "our", "us", "at", "on", "as", "or", "but", "so", "by", "can", "let"};
  for (const char* s : f) if (w == s) return true;
  return false;
}
bool isV(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

std::vector<std::string> g2pEnglish(const std::string& w) {
  std::vector<std::string> out;
  auto it = dict().find(w);
  if (it != dict().end()) {
    std::string cur;
    for (char ch : it->second + " ") { if (ch == ' ') { if (!cur.empty()) out.push_back(cur); cur.clear(); } else cur += ch; }
    return out;
  }
  int n = (int)w.size();
  auto at = [&](int i) -> char { return i >= 0 && i < n ? w[i] : 0; };
  auto starts = [&](int i, const char* s) { int l = (int)strlen(s); return i + l <= n && w.compare(i, l, s) == 0; };
  auto P = [&](const char* s) { out.push_back(s); };
  bool shortWord = n <= 3;
  for (int i = 0; i < n;) {
    char c = w[i];
    if (starts(i, "tion")) { P("SH"); P("AH"); P("N"); i += 4; continue; }
    if (starts(i, "sion")) { P("ZH"); P("AH"); P("N"); i += 4; continue; }
    if (starts(i, "ture") && i + 4 == n) { P("CH"); P("ER"); i += 4; continue; }
    if (starts(i, "ch")) { P("CH"); i += 2; continue; }
    if (starts(i, "sh")) { P("SH"); i += 2; continue; }
    if (starts(i, "th")) { P("TH"); i += 2; continue; }
    if (starts(i, "ph")) { P("F"); i += 2; continue; }
    if (starts(i, "ck")) { P("K"); i += 2; continue; }
    if (starts(i, "ng")) { P("NG"); i += 2; continue; }
    if (starts(i, "qu")) { P("K"); P("W"); i += 2; continue; }
    if (starts(i, "wh")) { P("W"); i += 2; continue; }
    if (i == 0 && starts(i, "kn")) { P("N"); i += 2; continue; }
    if (i == 0 && starts(i, "wr")) { P("R"); i += 2; continue; }
    if (starts(i, "gh")) { i += 2; continue; }
    if (c == 'e' && i == n - 1 && n > 2 && out.size() > 0) { i++; continue; }  // silent final e
    if (starts(i, "oo")) { P((starts(i, "ook") || starts(i, "ood") || starts(i, "oot")) ? "UH" : "UW"); i += 2; continue; }
    if (starts(i, "ee") || starts(i, "ea")) { P("IY"); i += 2; continue; }
    if (starts(i, "ai") || starts(i, "ay")) { P("EY"); i += 2; continue; }
    if (starts(i, "oa")) { P("OW"); i += 2; continue; }
    if (starts(i, "ow")) { P(i + 2 == n ? "OW" : "AW"); i += 2; continue; }
    if (starts(i, "ou")) { P("AW"); i += 2; continue; }
    if (starts(i, "oi") || starts(i, "oy")) { P("OY"); i += 2; continue; }
    if (starts(i, "au") || starts(i, "aw")) { P("AO"); i += 2; continue; }
    if (starts(i, "ew")) { P("UW"); i += 2; continue; }
    if (starts(i, "ie")) { P("IY"); i += 2; continue; }
    if (starts(i, "er") || starts(i, "ir") || starts(i, "ur")) { P("ER"); i += 2; continue; }
    if (starts(i, "ar")) { P("AA"); P("R"); i += 2; continue; }
    if (starts(i, "or")) { P("AO"); P("R"); i += 2; continue; }
    if (starts(i, "ed") && i + 2 == n && n > 3) {
      char p = at(i - 1);
      if (p == 't' || p == 'd') { P("IH"); P("D"); } else if (strchr("pkfsxhc", p)) P("T"); else P("D");
      i += 2; continue;
    }
    if (c == 's' && i == n - 1 && i > 0) {
      char p = at(i - 1);
      if (strchr("ptkfh", p) || p == 'e') P(p == 'e' ? "Z" : "S"); else P("Z");
      i++; continue;
    }
    if (isV(c)) {
      bool magic = (at(i + 1) && !isV(at(i + 1)) && at(i + 2) == 'e' && i + 3 == n) || (!isV(at(i + 1)) && at(i + 1) && isV(at(i + 2)) && at(i + 2) != 'e' && false);
      bool openEnd = (i == n - 1);
      switch (c) {
        case 'a': P(magic ? "EY" : (openEnd && !shortWord ? "AX" : "AE")); break;
        case 'e': P(magic ? "IY" : (openEnd ? "IY" : "EH")); break;
        case 'i': P(magic ? "AY" : (openEnd ? "AY" : "IH")); break;
        case 'o': P(magic ? "OW" : (openEnd ? "OW" : "AA")); break;
        case 'u': P(magic ? "UW" : "AH"); break;
      }
      i++; continue;
    }
    switch (c) {
      case 'b': P("B"); break; case 'd': P("D"); break; case 'f': P("F"); break; case 'h': P("HH"); break;
      case 'j': P("JH"); break; case 'k': P("K"); break; case 'l': P("L"); break; case 'm': P("M"); break;
      case 'n': P("N"); break; case 'p': P("P"); break; case 'r': P("R"); break; case 't': P("T"); break;
      case 'v': P("V"); break; case 'w': P("W"); break; case 'z': P("Z"); break; case 'x': P("K"); P("S"); break;
      case 'c': P(strchr("eiy", at(i + 1)) && at(i + 1) ? "S" : "K"); break;
      case 'g': P((strchr("eiy", at(i + 1)) && at(i + 1)) ? "JH" : "G"); break;
      case 's': P("S"); break;
      case 'y': if (i == 0) P("Y"); else P(i == n - 1 ? (n <= 3 ? "AY" : "IY") : "IH"); break;
      case 'q': P("K"); break;
      default: break;
    }
    i++;
  }
  // stress: first vowel of content words
  if (!isFuncWord(w)) {
    for (auto& s : out) if (isVowelSym(s)) { s += "1"; break; }
  }
  return out;
}

// ------------------------------------------------------------------ Arabic G2P
unsigned decodeUtf8(const std::string& s, size_t& i) {
  unsigned char c = s[i];
  if (c < 0x80) { i++; return c; }
  if ((c >> 5) == 6 && i + 1 < s.size()) { unsigned r = ((c & 31) << 6) | (s[i + 1] & 63); i += 2; return r; }
  if ((c >> 4) == 14 && i + 2 < s.size()) { unsigned r = ((c & 15) << 12) | ((s[i + 1] & 63) << 6) | (s[i + 2] & 63); i += 3; return r; }
  i++; return '?';
}
bool isArabicLetter(unsigned c) { return (c >= 0x0621 && c <= 0x064A) || c == 0x0670 || c == 0x0640; }
bool isArabicMark(unsigned c) { return (c >= 0x064B && c <= 0x0652) || c == 0x0670; }

const char* arabicCons(unsigned c) {
  switch (c) {
    case 0x0628: return "B"; case 0x062A: return "T"; case 0x062B: return "TH"; case 0x062C: return "JH"; case 0x062D: return "HX";
    case 0x062E: return "KH"; case 0x062F: return "D"; case 0x0630: return "DH"; case 0x0631: return "RT"; case 0x0632: return "Z";
    case 0x0633: return "S"; case 0x0634: return "SH"; case 0x0635: return "SS"; case 0x0636: return "DD"; case 0x0637: return "TT";
    case 0x0638: return "ZZ"; case 0x0639: return "AYN"; case 0x063A: return "GH"; case 0x0641: return "F"; case 0x0642: return "G";  // Libyan qaf = [g]
    case 0x0643: return "K"; case 0x0644: return "L"; case 0x0645: return "M"; case 0x0646: return "N"; case 0x0647: return "HH";
    case 0x0621: case 0x0623: case 0x0625: case 0x0624: case 0x0626: return "GS";
    default: return nullptr;
  }
}

std::vector<std::string> g2pArabic(const std::vector<unsigned>& w) {
  std::vector<std::string> out;
  // strip tatweel, keep order
  std::vector<unsigned> c;
  for (unsigned u : w) if (u != 0x0640) c.push_back(u);
  // article assimilation: ال + sun letter
  auto isSun = [&](unsigned u) { const char* s = arabicCons(u); if (!s) return false; static const char* sun[] = {"T", "TH", "D", "DH", "RT", "Z", "S", "SH", "SS", "DD", "TT", "ZZ", "L", "N"}; for (const char* x : sun) if (!strcmp(s, x)) return true; return false; };
  int n = (int)c.size();
  bool anyMark = false;
  for (unsigned u : c) if (isArabicMark(u)) anyMark = true;
  size_t start = 0;
  bool articleSun = false;
  if (n >= 3 && c[0] == 0x0627 && c[1] == 0x0644 && !isArabicMark(c[2]) && isSun(c[2])) { out.push_back("A"); out.push_back("S0"); start = 2; articleSun = true; out.pop_back(); }
  else if (n >= 3 && c[0] == 0x0627 && c[1] == 0x0644) { out.push_back("A"); out.push_back("L"); start = 2; }
  (void)articleSun;
  for (int i = (int)start; i < n; i++) {
    unsigned u = c[i];
    if (isArabicMark(u)) continue;
    bool nextMarkV = false;
    // collect marks following this letter
    int j = i + 1; bool fatha = false, damma = false, kasra = false, sukun = false, shadda = false, tanF = false, tanD = false, tanK = false, dagger = false;
    for (; j < n && isArabicMark(c[j]); j++) {
      switch (c[j]) {
        case 0x064E: fatha = true; break; case 0x064F: damma = true; break; case 0x0650: kasra = true; break; case 0x0652: sukun = true; break;
        case 0x0651: shadda = true; break; case 0x064B: tanF = true; break; case 0x064C: tanD = true; break; case 0x064D: tanK = true; break;
        case 0x0670: dagger = true; break;
      }
    }
    (void)nextMarkV; (void)sukun;
    bool last = (j >= n);
    bool hasVowelMark = fatha || damma || kasra || tanF || tanD || tanK || dagger;
    // long vowel letters
    if (u == 0x0627 || u == 0x0649 || u == 0x0622) {  // alef, alef maqsura, alef madda
      if (u == 0x0622) { out.push_back("GS"); }
      // long aa unless it is first letter (hamzat wasl) -> short a at word start
      if (i == 0) { out.push_back("A"); } else out.push_back("AAA");
      i = j - 1; continue;
    }
    if (u == 0x0648 || u == 0x064A) {  // waw / yaa
      bool isVowelCtx = i > 0 && !hasVowelMark && !shadda;
      // consonant if followed by vowel mark or is word-initial
      bool prevHasShort = false;
      if (i > 0) { for (int k = i - 1; k >= 0 && isArabicMark(c[k]); k--) { if (c[k] == 0x064F || c[k] == 0x0650 || c[k] == 0x064E) prevHasShort = true; } }
      if (i > 0 && isVowelCtx && !prevHasShort && !last) {  // medial long vowel
        out.push_back(u == 0x0648 ? "UU" : "II"); i = j - 1; continue;
      }
      if (i > 0 && isVowelCtx && !prevHasShort && last) { out.push_back(u == 0x0648 ? "UU" : "II"); i = j - 1; continue; }
      // consonantal
      out.push_back(u == 0x0648 ? "W" : "Y");
      if (shadda) out.push_back(u == 0x0648 ? "W" : "Y");
      if (fatha) out.push_back("A"); else if (damma) out.push_back("U"); else if (kasra) out.push_back("I");
      else if (!sukun && !last && i + 1 < n && arabicCons(c[j]) && j < n) out.push_back("A");
      i = j - 1; continue;
    }
    if (u == 0x0629) {  // ta marbuta
      out.push_back("A"); i = j - 1; continue;
    }
    const char* ph = arabicCons(u);
    if (!ph) { i = j - 1; continue; }
    // hamza carriers followed by nothing special
    out.push_back(ph);
    if (shadda) out.push_back(ph);
    if (fatha) out.push_back("A"); else if (damma) out.push_back("U"); else if (kasra) out.push_back("I");
    else if (tanF) { out.push_back("A"); out.push_back("N"); }
    else if (tanD) { out.push_back("U"); out.push_back("N"); }
    else if (tanK) { out.push_back("I"); out.push_back("N"); }
    else if (dagger) out.push_back("AAA");
    else if (!anyMark && !last) {
      // undiacritised text: insert a short vowel unless a long vowel letter / another vowel carrier follows
      unsigned nx = c[j];
      bool nextIsLong = (nx == 0x0627 || nx == 0x0648 || nx == 0x064A || nx == 0x0649 || nx == 0x0622);
      if (!nextIsLong) out.push_back("A");
    }
    i = j - 1;
  }
  // fix assimilated article: "A" + sun letter doubled handled by caller marker "S0" removed -> double next consonant
  if (start == 2 && out.size() >= 1 && out[0] == "A" && (int)out.size() >= 2 && out[1] != "L") { /* sun letter: geminate first consonant */
    for (size_t k = 1; k < out.size(); k++) { if (!isVowelSym(out[k])) { out.insert(out.begin() + k, out[k]); break; } }
  }
  // stress: penultimate-ish -> mark the last long vowel, else the vowel before the last one
  std::vector<int> vi;
  for (int k = 0; k < (int)out.size(); k++) if (isVowelSym(out[k])) vi.push_back(k);
  if (!vi.empty()) {
    int pick = -1;
    for (int k = (int)vi.size() - 1; k >= 0; k--) { const std::string& s = out[vi[k]]; if (s == "AAA" || s == "II" || s == "UU") { pick = vi[k]; break; } }
    if (pick < 0) pick = vi.size() >= 3 ? vi[vi.size() - 3] : (vi.size() == 2 ? vi[0] : vi[0]);
    out[pick] += "1";
  }
  return out;
}

// ------------------------------------------------------------------ tokenizer
std::vector<Tok> tokenize(const std::string& text) {
  std::vector<Tok> toks;
  size_t i = 0;
  std::string latin; std::vector<unsigned> arab;
  auto flush = [&]() {
    if (!latin.empty()) { Tok t; t.text = latin; t.ph = g2pEnglish(latin); if (!t.ph.empty()) toks.push_back(t); latin.clear(); }
    if (!arab.empty()) {
      Tok t; t.arabic = true; t.ph = g2pArabic(arab);
      for (unsigned u : arab) { char b[4]; if (u < 0x800) { b[0] = (char)(0xC0 | (u >> 6)); b[1] = (char)(0x80 | (u & 63)); t.text.append(b, 2); } else { b[0] = (char)(0xE0 | (u >> 12)); b[1] = (char)(0x80 | ((u >> 6) & 63)); b[2] = (char)(0x80 | (u & 63)); t.text.append(b, 3); } }
      if (!t.ph.empty()) toks.push_back(t);
      arab.clear();
    }
  };
  while (i < text.size()) {
    size_t i0 = i;
    unsigned u = decodeUtf8(text, i);
    if (u == '{') {
      flush();
      size_t e = text.find('}', i0);
      if (e == std::string::npos) e = text.size();
      std::string body = text.substr(i0 + 1, e - i0 - 1), cur; Tok t; t.raw = true; t.text = "{" + body + "}";
      for (char ch : body + " ") { if (ch == ' ') { if (!cur.empty()) { std::string up; for (char q : cur) up += (char)toupper(q); t.ph.push_back(up); } cur.clear(); } else cur += ch; }
      toks.push_back(t); i = std::min(e + 1, text.size()); continue;
    }
    if (isArabicLetter(u) || isArabicMark(u)) { if (!latin.empty()) flush(); arab.push_back(u); continue; }
    if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || u == '\'') {
      if (!arab.empty()) flush();
      if (u != '\'') latin += (char)tolower(u);
      continue;
    }
    flush();
    int p = 0;
    if (u == '.' ) p = '.'; else if (u == ',' || u == 0x060C) p = ','; else if (u == '?' || u == 0x061F) p = '?'; else if (u == '!') p = '!';
    else if (u == ':' || u == ';' || u == 0x061B) p = ':'; else if (u == '\n') p = '.'; else if (u == '-' ) p = 0;
    if (p) { Tok t; t.punct = p; toks.push_back(t); }
  }
  flush();
  return toks;
}

// ------------------------------------------------------------------ voice presets
}  // namespace

VoiceParams VoiceParams::maleTeacher() { VoiceParams v; return v; }
VoiceParams VoiceParams::femaleTeacher() { VoiceParams v; v.f0 = 205; v.tract = 1.17f; v.breath = 0.14f; v.tilt = 0.5f; v.range = 1.15f; v.seed = 7; return v; }
VoiceParams VoiceParams::child() { VoiceParams v; v.f0 = 290; v.tract = 1.34f; v.breath = 0.06f; v.tilt = 0.4f; v.range = 1.3f; v.rate = 1.05f; v.seed = 11; return v; }
VoiceParams VoiceParams::oldMan() { VoiceParams v; v.f0 = 98; v.tract = 0.98f; v.breath = 0.22f; v.jitter = 0.014f; v.shimmer = 0.09f; v.tremor = 0.6f; v.roughness = 0.35f; v.rate = 0.88f; v.range = 0.8f; v.seed = 5; return v; }

namespace {

void setMouthVowel(Params& p, float f1, float f2) {
  p.v[JAW] = clampf((f1 - 250.f) / 520.f, 0.f, 1.f) * 0.9f + 0.05f;
  p.v[WIDE] = clampf((f2 - 1350.f) / 1000.f, 0.f, 1.f);
  p.v[RND] = clampf((1250.f - f2) / 600.f, 0.f, 1.f) * (f1 < 650.f ? 1.f : 0.5f);
}
void setMouthCons(Params& p, char m) {
  switch (m) {
    case 'b': p.v[CLS] = 1; p.v[JAW] = 0.04f; break;
    case 'f': p.v[FVL] = 1; p.v[JAW] = 0.12f; break;
    case 'd': p.v[TTH] = 0.9f; p.v[TNG] = 0.6f; p.v[JAW] = 0.18f; break;
    case 'a': p.v[TNG] = 1; p.v[JAW] = 0.17f; p.v[TTH] = 0.3f; break;
    case 's': p.v[TTH] = 1; p.v[WIDE] = 0.45f; p.v[JAW] = 0.10f; p.v[TNG] = 0.4f; break;
    case 'h': p.v[RND] = 0.55f; p.v[TTH] = 0.6f; p.v[JAW] = 0.16f; break;
    case 'k': p.v[JAW] = 0.26f; break;
    case 'w': p.v[RND] = 1; p.v[JAW] = 0.14f; break;
    case 'y': p.v[WIDE] = 0.65f; p.v[JAW] = 0.12f; break;
    case 'r': p.v[RND] = 0.5f; p.v[JAW] = 0.22f; break;
    case 'l': p.v[TNG] = 1; p.v[JAW] = 0.28f; break;
    case 'p': p.v[JAW] = 0.5f; p.v[WIDE] = 0.2f; break;
    default: break;
  }
}

}  // namespace

bool speak(const std::string& utf8, const VoiceParams& vp, Speech& out) {
  initTable();
  const int SR = 24000;
  out = Speech(); out.rate = SR;
  std::vector<Tok> toks = tokenize(utf8);
  if (toks.empty()) return false;

  // ---- build segments
  std::vector<Seg> segs;
  float t = 0.f;
  const float rateK = 1.f / std::max(0.4f, vp.rate);
  struct PhraseInfo { float t0, t1; int end; };
  std::vector<PhraseInfo> phrases;
  float phraseStart = 0.f;
  std::string phStr;
  auto pushPause = [&](float sec, int word = -1) {
    Seg s; s.t0 = t; s.t1 = t + sec; s.inh = 1; s.tf = 0.02f; s.word = word; segs.push_back(s); t += sec;
  };
  pushPause(0.12f);
  int wordIdx = 0;
  std::vector<std::pair<int, int>> wordSegRange;  // [first,last] segment index per word
  bool pendingStart = true;
  for (size_t ti = 0; ti < toks.size(); ti++) {
    const Tok& tk = toks[ti];
    if (tk.punct) {
      float pz = tk.punct == ',' ? 0.20f : (tk.punct == ':' ? 0.28f : 0.45f);
      pushPause(pz * rateK);
      phrases.push_back({phraseStart, t, tk.punct});
      phraseStart = t; pendingStart = true;
      phStr += (char)tk.punct; phStr += ' ';
      continue;
    }
    int firstSeg = (int)segs.size();
    bool arab = tk.arabic;
    // expand symbols
    struct Sym { std::string base; int stress; };
    std::vector<Sym> syms;
    for (const auto& s : tk.ph) { int st; std::string b = baseSym(s, &st); syms.push_back({b, st}); phStr += s + " "; }
    phStr += "| ";
    for (size_t si = 0; si < syms.size(); si++) {
      auto it = g_ph.find(syms[si].base);
      if (it == g_ph.end()) continue;
      const Ph& p = it->second;
      bool stressed = syms[si].stress == 1;
      // context for emphatic colouring
      auto isBack = [&](int k) {
        if (k < 0 || k >= (int)syms.size()) return false;
        const std::string& b = syms[k].base;
        return b == "SS" || b == "DD" || b == "TT" || b == "ZZ" || b == "Q" || b == "KH" || b == "GH" || b == "HX" || b == "AYN";
      };
      bool back = arab && p.type == T_V && (isBack((int)si - 1) || isBack((int)si + 1));
      bool lastInWord = si + 1 == syms.size();
      float dm = rateK;
      auto addSeg = [&](float durMs, const Params& a, const Params& b, int inh, float tf, bool vowel = false) {
        Seg s; s.t0 = t; s.t1 = t + durMs * 0.001f; s.a = a; s.b = b; s.inh = inh; s.tf = tf; s.vowel = vowel; s.stressed = stressed && vowel; s.word = wordIdx;
        segs.push_back(s); t = s.t1;
      };
      auto setF = [&](Params& q, float f1, float f2, float f3) { q.v[F1] = f1; q.v[F2] = f2; q.v[F3] = f3; q.v[B1] = 70; q.v[B2] = 100; q.v[B3] = 160; };
      auto stressDur = [&](float d) { return d * (stressed ? 1.18f : (p.type == T_V && p.f1 > 0 && !syms[si].base.empty() && syms[si].base == "AX" ? 0.9f : 1.f)); };
      if (p.type == T_V || p.type == T_D) {
        Params a, b;
        float f1 = p.f1, f2 = p.f2, f3 = p.f3;
        if (back) { f1 *= 1.08f; f2 *= 0.80f; f3 *= 0.96f; }
        setF(a, f1, f2, f3);
        a.v[AV] = 0.75f + 0.25f * clampf(f1 / 700.f, 0.f, 1.f);
        if (p.type == T_D) {
          setF(b, p.g1, p.g2, p.g3); b.v[AV] = a.v[AV] * 0.9f;
          setMouthVowel(a, f1, f2);
          Params m2; setMouthVowel(m2, p.g1, p.g2);
          for (int k : {JAW, WIDE, RND}) b.v[k] = m2.v[k];
        } else { b = a; setMouthVowel(a, f1, f2); setMouthVowel(b, f1, f2); }
        if (!stressed && syms[si].base != "AX") { a.v[AV] *= 0.88f; b.v[AV] *= 0.88f; }
        float d = stressDur(p.dur) * dm * (lastInWord && ti + 1 < toks.size() && toks[ti + 1].punct ? 1.25f : 1.f);
        if (!stressed && !arab && p.type == T_V && syms[si].stress < 0 && syms[si].base != "AX" && (isFuncWord(tk.text))) d *= 0.8f;
        addSeg(d, a, b, 0, 0.055f, true);
        segs.back().pitchBump = stressed ? 1.f : 0.f;
        if (lastInWord && ti + 1 < toks.size() && toks[ti + 1].punct) segs.back().finalVowel = true;
      } else if (p.type == T_A || p.type == T_N) {
        Params a; setF(a, p.f1, p.f2, p.f3);
        a.v[AV] = p.type == T_N ? 0.55f : (syms[si].base == "RT" ? 0.55f : 0.7f); a.v[NAS] = p.type == T_N ? 1.f : 0.f;
        if (syms[si].base == "RT") a.v[AV] = 0.45f;
        if (p.type == T_N) { a.v[B1] = 90; }
        setMouthVowel(a, p.f1, p.f2); setMouthCons(a, p.mouth);
        addSeg(p.dur * dm, a, a, 0, 0.040f);
      } else if (p.type == T_S) {
        Params clo; clo.v[AV] = p.voiced ? 0.20f : 0.f; setMouthCons(clo, p.mouth); clo.v[NAS] = 0;
        addSeg((p.voiced ? 55.f : 70.f) * dm, clo, clo, 1, 0.012f);
        Params br; br.v[AF] = p.namp; br.v[FF] = p.nf; br.v[FB] = p.nbw; br.v[AV] = p.voiced ? 0.3f : 0.f; setMouthCons(br, p.mouth);
        {Params br2=br; br2.v[AF]*=0.25f; addSeg(14.f * dm, br, br2, 2, 0.003f);}
        if (!p.voiced) { Params as; as.v[AH] = 0.55f; as.v[AF] = p.namp * 0.15f; as.v[FF] = p.nf; as.v[FB] = p.nbw; setMouthCons(as, p.mouth); addSeg(36.f * dm, as, as, 2, 0.008f); }
      } else if (p.type == T_F) {
        Params a; a.v[AF] = p.namp; a.v[FF] = p.nf; a.v[FB] = p.nbw; a.v[AV] = p.voiced ? 0.42f : 0.f; setMouthCons(a, p.mouth);
        if (!p.voiced) a.v[AH] = 0.05f;
        addSeg(p.dur * dm, a, a, 2, 0.030f);
      } else if (p.type == T_C) {
        Params clo; setMouthCons(clo, p.mouth); clo.v[AV] = p.voiced ? 0.2f : 0.f; addSeg(50.f * dm, clo, clo, 1, 0.012f);
        Params a; a.v[AF] = p.namp; a.v[FF] = p.nf; a.v[FB] = p.nbw; a.v[AV] = p.voiced ? 0.4f : 0.f; setMouthCons(a, p.mouth); addSeg(85.f * dm, a, a, 2, 0.008f);
      } else if (p.type == T_H) {
        Params a; a.v[AH] = 0.65f; a.v[AV] = 0.f; addSeg(p.dur * dm, a, a, 2, 0.02f);
      } else if (p.type == T_G) {
        Params a; addSeg(p.dur * dm, a, a, 1, 0.01f);
      }
    }
    int lastSeg = (int)segs.size() - 1;
    if (lastSeg >= firstSeg) {
      wordSegRange.push_back({firstSeg, lastSeg});
      out.words.push_back({segs[firstSeg].t0, segs[lastSeg].t1, tk.text});
      wordIdx++;
    }
    pendingStart = false;
    // tiny gap between words (coarticulation continues)
    // no pause
  }
  (void)pendingStart;
  phrases.push_back({phraseStart, t, 0});
  pushPause(0.25f);
  float totalT = t;
  out.phonemes = phStr;

  // ---- resolve inherited formants
  const int FORM[] = {F1, F2, F3, B1, B2, B3};
  // forward: inherit previous end
  for (size_t i = 0; i < segs.size(); i++) {
    if (segs[i].inh == 1) {
      for (int k : FORM) { float v = i ? segs[i - 1].b.v[k] : (k >= B1 ? 80.f : (k == F1 ? 500.f : (k == F2 ? 1500.f : 2500.f))); segs[i].a.v[k] = v; segs[i].b.v[k] = v; }
      if (i) { segs[i].a.v[NAS] = 0; }
    }
  }
  // backward: inherit next start (look for next seg with real formants, i.e. inh==0)
  for (int i = (int)segs.size() - 1; i >= 0; i--) {
    if (segs[i].inh == 2) {
      int j = i + 1; while (j < (int)segs.size() && segs[j].inh == 2) j++;
      for (int k : FORM) {
        float v;
        if (j < (int)segs.size()) v = segs[j].inh == 0 ? segs[j].a.v[k] : segs[j].a.v[k];
        else v = i ? segs[i - 1].b.v[k] : 1000.f;
        if (j >= (int)segs.size() || v == 0) v = i ? segs[i - 1].b.v[k] : 1000.f;
        segs[i].a.v[k] = v; segs[i].b.v[k] = v;
      }
    }
  }
  // inh==1 segments that follow inh==2 chain were resolved before; recompute anything still zero
  for (size_t i = 0; i < segs.size(); i++) if (segs[i].a.v[F1] <= 0) { for (int k : FORM) { float v = i ? segs[i - 1].b.v[k] : 500.f; segs[i].a.v[k] = v; segs[i].b.v[k] = v; } }
  // consonant mouth: h/stops that inherit take next vowel mouth shape
  for (int i = (int)segs.size() - 1; i >= 0; i--) {
    if (segs[i].inh == 2 && segs[i].a.v[JAW] < 0.06f && segs[i].a.v[CLS] < 0.5f) {
      for (int j = i + 1; j < (int)segs.size(); j++) if (segs[j].vowel) { for (int k : {JAW, WIDE, RND}) { segs[i].a.v[k] = std::max(segs[i].a.v[k], segs[j].a.v[k] * 0.8f); segs[i].b.v[k] = segs[i].a.v[k]; } break; }
    }
  }

  // ---- pitch contour control
  Rng rng(vp.seed);
  float ph0 = rng.uni();
  auto f0At = [&](float tt) {
    // find phrase
    const PhraseInfo* pi = &phrases.back();
    for (const auto& p : phrases) if (tt < p.t1) { pi = &p; break; }
    float pt = clampf((tt - pi->t0) / std::max(0.2f, pi->t1 - pi->t0), 0.f, 1.f);
    float decl = lerpf(1.10f, 0.90f, pt);
    float rg = vp.range;
    float f = vp.f0 * (1.f + (decl - 1.f) * rg);
    // accents
    float bump = 0;
    for (const Seg& s : segs) {
      if (s.pitchBump > 0 && std::fabs(tt - (s.t0 + (s.t1 - s.t0) * 0.35f)) < 0.3f) {
        float d = (tt - (s.t0 + (s.t1 - s.t0) * 0.35f)) / 0.075f; bump += 0.12f * rg * std::exp(-d * d);
      }
    }
    f *= 1.f + bump;
    float endT = pi->t1 - 0.45f * ((pi->end == ',') ? 0.4f : 0.6f);  // before the trailing pause
    float rem = pi->t1 - tt;
    float lastVoiceEnd = pi->t1 - (pi->end == ',' ? 0.20f : (pi->end == ':' ? 0.28f : (pi->end ? 0.45f : 0.25f)));
    float k = clampf(1.f - (lastVoiceEnd - tt) / 0.35f, 0.f, 1.f);
    (void)endT; (void)rem;
    if (pi->end == '.' || pi->end == '!' || pi->end == 0) f *= 1.f - 0.20f * rg * sstep(k);
    else if (pi->end == '?') f *= 1.f + 0.32f * rg * sstep(k);
    else if (pi->end == ',' || pi->end == ':') f *= 1.f + 0.05f * rg * sstep(k);
    return f;
  };

  // ---- evaluate params at time
  size_t cur = 0;
  auto evalParams = [&](float tt, Params& o) {
    while (cur + 1 < segs.size() && tt >= segs[cur].t1) cur++;
    const Seg& s = segs[cur];
    float u = tt - s.t0, dur = std::max(1e-4f, s.t1 - s.t0);
    float frac = clampf(u / dur, 0.f, 1.f);
    Params base;
    for (int k = 0; k < NP; k++) base.v[k] = lerpf(s.a.v[k], s.b.v[k], frac);
    if (cur > 0) {
      const Seg& pv = segs[cur - 1];
      float tfm = std::min(s.tf, dur * 0.7f);
      float wf = sstep(u / tfm);
      float ws = sstep(u / std::min(0.008f, dur * 0.5f));
      for (int k = 0; k < NP; k++) {
        float w = (k == AV || k == AH || k == AF || k == NAS) ? ws : (k >= JAW ? sstep(u / std::min(0.05f, dur * 0.8f)) : wf);
        if (k == FF || k == FB) w = ws;
        float pe = pv.b.v[k];
        if ((k == FF || k == FB) && pe == 0.f) pe = base.v[k];
        o.v[k] = lerpf(pe, base.v[k], w);
      }
    } else for (int k = 0; k < NP; k++) o.v[k] = base.v[k];
  };

  // ---- synthesis
  const float T = 1.f / SR;
  size_t nS = (size_t)((totalT + 0.35f) * SR);
  out.pcm.assign(nS, 0.f);
  Res r1, r2, r3, r4, r5, nr, fr, fr2; Anti na;
  float gPrev = 0.f, phase = ph0, period = SR / vp.f0, ampJ = 1.f, lp = 0.f, hp = 0.f, hpPrev = 0.f;
  float curF0 = vp.f0; int periodCount = 0; float tremPh = 0.f, noiseLP = 0.f;
  float shimmerAmp = 1.f; float oq = 0.50f + 0.28f * vp.breath;
  Params P;
  float lastF0 = vp.f0;
  float tiltA = 0.12f + 0.75f * (1.f - clampf(vp.tilt, 0.f, 1.f));
  const int UP = 12;
  float envSmooth = 0;
  for (size_t n = 0; n < nS; n++) {
    float tt = n * T;
    if (n % UP == 0) {
      if (tt < totalT) evalParams(tt, P); else for (int k = 0; k < NP; k++) P.v[k] *= 0.9f;
      float tr = vp.tract;
      r1.set(P.v[F1] * tr, std::max(40.f, P.v[B1]) * (0.9f + 0.1f * tr), SR);
      r2.set(P.v[F2] * tr, std::max(60.f, P.v[B2]), SR);
      r3.set(P.v[F3] * tr, std::max(90.f, P.v[B3]), SR);
      r4.set(3350.f * tr, 250.f, SR); r5.set(3850.f * tr, 300.f, SR);
      if (P.v[NAS] > 0.01f) { nr.set(270.f, 90.f, SR); na.set(950.f, 220.f, SR); }
      float ff = P.v[FF] > 100.f ? P.v[FF] : 4000.f;
      fr.set(ff * (0.85f + 0.15f * tr), std::max(300.f, P.v[FB]), SR);
      curF0 = f0At(tt);
      if (vp.tremor > 0) { tremPh += 5.2f * UP * T; curF0 *= 1.f + 0.035f * vp.tremor * std::sin(2 * PI * tremPh); }
      lastF0 = curF0;
    }
    // glottal source
    phase += lastF0 / SR * 1.f;
    if (phase >= 1.f) {
      phase -= 1.f; periodCount++;
      float jit = 1.f + vp.jitter * rng.noise() * 1.7f;
      period = SR / (lastF0 * jit);
      shimmerAmp = 1.f + vp.shimmer * rng.noise() * 1.7f;
      if (vp.roughness > 0 && (periodCount & 1)) shimmerAmp *= 1.f - 0.4f * vp.roughness;
    }
    float tp = 0.40f * oq / 0.5f * 0.62f + 0.12f, tn = 0.16f * oq / 0.5f * 0.7f + 0.04f;
    tp = clampf(tp, 0.3f, 0.6f); tn = clampf(tn, 0.08f, 0.25f);
    float g = 0.f;
    if (phase < tp) g = 0.5f * (1.f - std::cos(PI * phase / tp));
    else if (phase < tp + tn) g = std::cos(PI * 0.5f * (phase - tp) / tn);
    float dg = (g - gPrev) * period * 0.12f; gPrev = g;
    float voiced = dg * P.v[AV] * shimmerAmp;
    lp += tiltA * (voiced - lp);  // spectral tilt
    float src = lp;
    float nz = rng.noise();
    float asp = nz * (P.v[AH] * 0.45f + vp.breath * 0.30f * P.v[AV] * (0.20f + g));
    float x = src * 5.5f + asp * 1.6f;
    // cascade
    float y = r1.run(x); y = r2.run(y); y = r3.run(y); y = r4.run(y); y = r5.run(y);
    if (P.v[NAS] > 0.01f) { float yn = na.run(y) * 0.8f + 0.7f * nr.run(y); y = lerpf(y, yn, clampf(P.v[NAS], 0.f, 1.f)); }
    // frication (parallel)
    float fric = fr.run(rng.noise() * P.v[AF] * 3.2f);
    float s = y * 0.26f + fric * 0.10f;
    // pre-emphasis / radiation tilt tweak and DC block
    float hpOut = s - hpPrev + 0.995f * hp; hpPrev = s; hp = hpOut;
    out.pcm[n] = hpOut;
    (void)noiseLP;
  }

  // ---- normalise, soft clip
  std::vector<float> ab(out.pcm.size());
  for (size_t i = 0; i < ab.size(); i++) ab[i] = std::fabs(out.pcm[i]);
  std::vector<float> sorted = ab; size_t kth = (size_t)(sorted.size() * 0.995);
  std::nth_element(sorted.begin(), sorted.begin() + kth, sorted.end());
  float pk = std::max(1e-6f, sorted[kth]);
  float gain = 0.55f / pk;
  for (size_t i = 0; i < out.pcm.size(); i++) { float q = out.pcm[i] * gain; out.pcm[i] = std::tanh(q * 1.1f) / std::tanh(1.1f) * 0.9f; }

  // ---- classroom reverb (Schroeder)
  if (vp.room > 0.001f) {
    struct Comb { std::vector<float> b; size_t i = 0; float fb; float lpv = 0; };
    const float cd[4] = {0.0297f, 0.0371f, 0.0411f, 0.0437f};
    Comb cb[4]; for (int k = 0; k < 4; k++) { cb[k].b.assign((size_t)(cd[k] * SR), 0.f); cb[k].fb = 0.55f; }
    struct AP { std::vector<float> b; size_t i = 0; };
    AP ap[2]; ap[0].b.assign((size_t)(0.005f * SR), 0.f); ap[1].b.assign((size_t)(0.0017f * SR), 0.f);
    std::vector<float> dry = out.pcm;
    for (size_t n = 0; n < nS; n++) {
      float x = dry[n], acc = 0;
      for (int k = 0; k < 4; k++) { float o = cb[k].b[cb[k].i]; cb[k].lpv += 0.45f * (o - cb[k].lpv); cb[k].b[cb[k].i] = x + cb[k].lpv * cb[k].fb; cb[k].i = (cb[k].i + 1) % cb[k].b.size(); acc += o; }
      acc *= 0.25f;
      for (int k = 0; k < 2; k++) { float bo = ap[k].b[ap[k].i]; float yy = -acc + bo; ap[k].b[ap[k].i] = acc + bo * 0.5f; ap[k].i = (ap[k].i + 1) % ap[k].b.size(); acc = yy; }
      out.pcm[n] = dry[n] * (1.f - 0.15f * vp.room) + acc * vp.room * 0.9f;
    }
  }

  // ---- mouth track (100 Hz)
  cur = 0;
  float prevJaw = 0;
  for (float tt = 0; tt < totalT + 0.2f; tt += 0.01f) {
    Params q;
    if (tt < totalT) evalParams(tt, q);
    MouthKey m;
    m.t = tt; m.jaw = clampf(q.v[JAW], 0.f, 1.f); m.wide = clampf(q.v[WIDE], 0.f, 1.f); m.round = clampf(q.v[RND], 0.f, 1.f);
    m.close = clampf(q.v[CLS], 0.f, 1.f); m.fv = clampf(q.v[FVL], 0.f, 1.f); m.tongue = clampf(q.v[TNG], 0.f, 1.f); m.teeth = clampf(q.v[TTH], 0.f, 1.f);
    m.energy = clampf(std::max(q.v[AV], q.v[AF]), 0.f, 1.f);
    // silence -> mouth relaxes slightly closed
    if (m.energy < 0.05f && m.close < 0.5f) { m.jaw *= 0.3f; }
    prevJaw = m.jaw; out.mouth.push_back(m);
  }
  (void)prevJaw;
  return true;
}

bool writeWav(const std::string& path, const Speech& s) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  unsigned n = (unsigned)s.pcm.size(), dataBytes = n * 2, rate = (unsigned)s.rate;
  auto w32 = [&](unsigned v) { fwrite(&v, 4, 1, f); };
  auto w16 = [&](unsigned short v) { fwrite(&v, 2, 1, f); };
  fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(1); w32(rate); w32(rate * 2); w16(2); w16(16);
  fwrite("data", 1, 4, f); w32(dataBytes);
  for (unsigned i = 0; i < n; i++) { float v = std::max(-1.f, std::min(1.f, s.pcm[i])); short q = (short)std::lrintf(v * 32767.f); fwrite(&q, 2, 1, f); }
  fclose(f);
  return true;
}

}  // namespace sk
