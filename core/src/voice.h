// Sabak Voice: formant (Klatt-style) speech synthesiser with lip-sync output.
// Text (English / Arabic, or raw phonemes in {braces}) -> PCM + mouth-shape track.
#pragma once
#include <string>
#include <vector>

namespace sk {

struct VoiceParams {
  float f0 = 112.f;        // base pitch Hz
  float tract = 1.0f;      // vocal-tract scale (formants multiplied by it): male 1.0, female ~1.17, child ~1.35
  float rate = 1.0f;       // speaking speed (1 = normal)
  float breath = 0.10f;    // breathiness 0..1
  float jitter = 0.006f;   // pitch micro-variation
  float shimmer = 0.04f;   // amplitude micro-variation
  float tilt = 0.55f;      // spectral tilt (higher = darker / softer voice)
  float range = 1.0f;      // intonation range multiplier
  float tremor = 0.0f;     // old-age tremor 0..1
  float roughness = 0.0f;  // creak/roughness 0..1
  float room = 0.05f;      // classroom reverb amount 0..1
  unsigned seed = 1;
  // presets
  static VoiceParams maleTeacher();
  static VoiceParams femaleTeacher();
  static VoiceParams child();
  static VoiceParams oldMan();
};

// One mouth pose sample (every 10 ms). All 0..1.
struct MouthKey {
  float t;        // seconds
  float jaw;      // jaw opening
  float wide;     // lip corners spread (ee)
  float round;    // lips rounded / protruded (oo)
  float close;    // lips pressed together (p b m)
  float fv;       // lower lip against upper teeth (f v)
  float tongue;   // tongue tip raised (t d n l)
  float teeth;    // teeth visible (s z sh)
  float energy;   // loudness envelope
};

struct WordMark { float t0, t1; std::string text; };

struct Speech {
  int rate = 24000;
  std::vector<float> pcm;          // mono, -1..1
  std::vector<MouthKey> mouth;     // 100 Hz lip-sync track
  std::vector<WordMark> words;     // word timings (for subtitles / gestures)
  std::string phonemes;            // debug: phoneme string used
  float duration() const { return rate ? (float)pcm.size() / rate : 0.f; }
};

// Speak text. Arabic characters are read with Arabic rules (type diacritics for best result),
// Latin text with English rules. "{AH0 B K}" injects raw ARPAbet-like phonemes. Punctuation drives pauses/intonation.
bool speak(const std::string& utf8, const VoiceParams& vp, Speech& out);

// Write a 16-bit mono WAV file.
bool writeWav(const std::string& path, const Speech& s);

}  // namespace sk
