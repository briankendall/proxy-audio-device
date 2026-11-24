#ifndef Equalizer_h
#define Equalizer_h

// 10-band graphic equalizer applied to the proxied audio. Header only, because
// the settings app shares the band layout and the settings string format.

#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>
#include <cmath>

#define kEqualizerBandCount 10
#define kEqualizerMaxChannels 8
#define kEqualizerMinGainDB -12.0
#define kEqualizerMaxGainDB 12.0
// One octave bandwidth, matching the octave spacing of the bands.
#define kEqualizerBandQ 1.414

static const double kEqualizerBandFrequencies[kEqualizerBandCount] = {
    31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

struct EqualizerSettings {
    bool enabled = false;
    double preampDB = 0.0;
    double gainsDB[kEqualizerBandCount] = {};

    bool operator==(const EqualizerSettings &other) const {
        if (enabled != other.enabled || preampDB != other.preampDB) {
            return false;
        }

        for (int i = 0; i < kEqualizerBandCount; i++) {
            if (gainsDB[i] != other.gainsDB[i]) {
                return false;
            }
        }

        return true;
    }
};

inline double EqualizerClampGain(double gainDB) {
    if (!std::isfinite(gainDB)) {
        return 0.0;
    }

    return std::min(std::max(gainDB, kEqualizerMinGainDB), kEqualizerMaxGainDB);
}

// Serialized as "enabled,preamp,gain0,...,gain9", e.g. "1,-3.0,4.5,0.0,...".
// Used both for the configuration channel and for the driver's storage.
inline CFStringRef EqualizerSettingsCopyString(const EqualizerSettings &settings) {
    CFMutableStringRef result = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(result, NULL, CFSTR("%d,%.1f"), settings.enabled ? 1 : 0, settings.preampDB);

    for (int i = 0; i < kEqualizerBandCount; i++) {
        CFStringAppendFormat(result, NULL, CFSTR(",%.1f"), settings.gainsDB[i]);
    }

    return result;
}

inline bool EqualizerSettingsFromString(CFStringRef string, EqualizerSettings &outSettings) {
    if (!string) {
        return false;
    }

    CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(NULL, string, CFSTR(","));

    if (!parts) {
        return false;
    }

    bool ok = CFArrayGetCount(parts) == 2 + kEqualizerBandCount;

    if (ok) {
        EqualizerSettings settings;
        settings.enabled = CFStringGetIntValue((CFStringRef)CFArrayGetValueAtIndex(parts, 0)) != 0;
        settings.preampDB = EqualizerClampGain(CFStringGetDoubleValue((CFStringRef)CFArrayGetValueAtIndex(parts, 1)));

        for (int i = 0; i < kEqualizerBandCount; i++) {
            settings.gainsDB[i] =
                EqualizerClampGain(CFStringGetDoubleValue((CFStringRef)CFArrayGetValueAtIndex(parts, 2 + i)));
        }

        outSettings = settings;
    }

    CFRelease(parts);
    return ok;
}

// Peaking biquads (RBJ audio EQ cookbook) in transposed direct form II, with
// double precision state. Only touched from the target device's IO thread.
class Equalizer {
  public:
    // Cheap enough to call from the IO thread. Every band always has a filter
    // (0 dB is an exact pass-through), so moving a slider only changes
    // coefficients and doesn't reset filter state, which would click.
    void configure(const EqualizerSettings &settings, double sampleRate) {
        bool wasActive = active;
        bool sampleRateChanged = sampleRate != configuredSampleRate;
        configuredSampleRate = sampleRate;
        preampFactor = (float)pow(10.0, settings.preampDB / 20.0);
        active = settings.enabled && sampleRate > 0 && settings.preampDB != 0.0;

        for (int i = 0; i < kEqualizerBandCount; i++) {
            double frequency = kEqualizerBandFrequencies[i];
            Coefficients &c = coefficients[i];

            // Bands at or near Nyquist (16 kHz at 22.05 kHz) can't be realized.
            if (sampleRate <= 0 || frequency >= sampleRate * 0.45) {
                c = Coefficients();
                continue;
            }

            if (settings.gainsDB[i] != 0.0) {
                active = active || settings.enabled;
            }

            double A = pow(10.0, settings.gainsDB[i] / 40.0);
            double w0 = 2.0 * M_PI * frequency / sampleRate;
            double alpha = sin(w0) / (2.0 * kEqualizerBandQ);
            double cosw0 = cos(w0);
            double a0 = 1.0 + alpha / A;

            c.b0 = (1.0 + alpha * A) / a0;
            c.b1 = (-2.0 * cosw0) / a0;
            c.b2 = (1.0 - alpha * A) / a0;
            c.a1 = (-2.0 * cosw0) / a0;
            c.a2 = (1.0 - alpha / A) / a0;
        }

        if (sampleRateChanged || (active && !wasActive)) {
            reset();
        }
    }

    void reset() {
        for (int band = 0; band < kEqualizerBandCount; band++) {
            for (int channel = 0; channel < kEqualizerMaxChannels; channel++) {
                state[band][channel] = State();
            }
        }
    }

    // In place on interleaved Float32 samples.
    void process(Float32 *samples, UInt32 frameCount, UInt32 channelCount) {
        if (!active) {
            return;
        }

        UInt32 channelsToProcess = std::min(channelCount, (UInt32)kEqualizerMaxChannels);

        for (UInt32 channel = 0; channel < channelsToProcess; channel++) {
            for (int band = 0; band < kEqualizerBandCount; band++) {
                const Coefficients &c = coefficients[band];
                State &s = state[band][channel];
                Float32 *sample = samples + channel;

                for (UInt32 frame = 0; frame < frameCount; frame++, sample += channelCount) {
                    double x = *sample;
                    double y = c.b0 * x + s.z1;
                    s.z1 = c.b1 * x - c.a1 * y + s.z2;
                    s.z2 = c.b2 * x - c.a2 * y;
                    *sample = (Float32)y;
                }

                // Flush decaying state to zero so silence doesn't leave the
                // filters crunching denormals.
                if (fabs(s.z1) < 1e-15) {
                    s.z1 = 0.0;
                }

                if (fabs(s.z2) < 1e-15) {
                    s.z2 = 0.0;
                }
            }

            if (preampFactor != 1.0f) {
                Float32 *sample = samples + channel;

                for (UInt32 frame = 0; frame < frameCount; frame++, sample += channelCount) {
                    *sample *= preampFactor;
                }
            }
        }
    }

  private:
    struct Coefficients {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    struct State {
        double z1 = 0.0, z2 = 0.0;
    };

    bool active = false;
    double configuredSampleRate = 0.0;
    float preampFactor = 1.0f;
    Coefficients coefficients[kEqualizerBandCount];
    State state[kEqualizerBandCount][kEqualizerMaxChannels];
};

#endif /* Equalizer_h */
