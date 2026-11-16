// sabakvoice: command-line voice maker.  sabakvoice [--voice male|female|child|old] [--rate 1.0] [--f0 Hz] [--out x.wav] [--mouth x.csv] "text"
#include "voice.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
int main(int argc, char** argv) {
  sk::VoiceParams vp = sk::VoiceParams::maleTeacher(); std::string out = "voice.wav", mouth, text;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--voice" && i + 1 < argc) { std::string v = argv[++i]; vp = v == "female" ? sk::VoiceParams::femaleTeacher() : v == "child" ? sk::VoiceParams::child() : v == "old" ? sk::VoiceParams::oldMan() : sk::VoiceParams::maleTeacher(); }
    else if (a == "--rate" && i + 1 < argc) vp.rate = (float)atof(argv[++i]);
    else if (a == "--f0" && i + 1 < argc) vp.f0 = (float)atof(argv[++i]);
    else if (a == "--out" && i + 1 < argc) out = argv[++i];
    else if (a == "--mouth" && i + 1 < argc) mouth = argv[++i];
    else text += (text.empty() ? "" : " ") + a;
  }
  sk::Speech s;
  if (!sk::speak(text, vp, s)) { fprintf(stderr, "nothing to say\n"); return 1; }
  sk::writeWav(out, s);
  if (!mouth.empty()) { FILE* f = fopen(mouth.c_str(), "w"); fprintf(f, "t,jaw,wide,round,close,fv,tongue,teeth,energy\n"); for (auto& m : s.mouth) fprintf(f, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", m.t, m.jaw, m.wide, m.round, m.close, m.fv, m.tongue, m.teeth, m.energy); fclose(f); }
  printf("%.2fs  phonemes: %s\n", s.duration(), s.phonemes.c_str());
  return 0;
}
