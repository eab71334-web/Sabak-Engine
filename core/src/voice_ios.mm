// speakSystem for iOS/macOS: captures the system speech engine's audio (AVSpeechSynthesizer) as PCM.
#import <AVFoundation/AVFoundation.h>
#include "voice.h"
#include <memory>
#include <mutex>

namespace sk {
void speakSystem(const std::string& utf8, const std::string& lang, float rate, std::function<void(bool, Speech&)> cb) {
  @autoreleasepool {
    static AVSpeechSynthesizer* synth = [[AVSpeechSynthesizer alloc] init];
    NSString* text = [NSString stringWithUTF8String:utf8.c_str()];
    AVSpeechUtterance* u = [AVSpeechUtterance speechUtteranceWithString:text];
    AVSpeechSynthesisVoice* v = [AVSpeechSynthesisVoice voiceWithLanguage:[NSString stringWithUTF8String:lang.c_str()]];
    if (!v) { Speech e; if (cb) cb(false, e); return; }
    u.voice = v;
    u.rate = std::max(0.0f, std::min(1.0f, AVSpeechUtteranceDefaultSpeechRate * rate));
    auto acc = std::make_shared<Speech>();
    auto done = std::make_shared<bool>(false);
    std::string src = utf8;
    [synth writeUtterance:u toBufferCallback:^(AVAudioBuffer* buf) {
      if (*done) return;
      AVAudioPCMBuffer* pb = (AVAudioPCMBuffer*)buf;
      if (![pb isKindOfClass:[AVAudioPCMBuffer class]] || pb.frameLength == 0) {
        *done = true;
        bool ok = !acc->pcm.empty();
        if (ok) fitMouthToAudio(src, *acc);
        if (cb) cb(ok, *acc);
        return;
      }
      acc->rate = (int)pb.format.sampleRate;
      size_t n = pb.frameLength;
      if (pb.floatChannelData) { const float* d = pb.floatChannelData[0]; acc->pcm.insert(acc->pcm.end(), d, d + n); }
      else if (pb.int16ChannelData) { const int16_t* d = pb.int16ChannelData[0]; for (size_t i = 0; i < n; i++) acc->pcm.push_back(d[i] / 32768.f); }
    }];
  }
}
}  // namespace sk
