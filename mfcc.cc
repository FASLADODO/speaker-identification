#include "mfcc.h"

#include <assert.h>
#include <stdint.h>

TriFilterBank::TriFilterBank(double left, double middle, double right, uint32_t fs, uint32_t size) {
    filter_.resize(size);
    double unit = 1.0f * fs / 2 / (size - 1);
    for (uint32_t i = 0; i < size; i++) {
        double f = unit * i;
        if (f <= left) {
            filter_[i] = 0;
        } else if (left < f && f <= middle) {
            filter_[i] = 1.0f * (f - left) / (middle - left);
        } else if (middle < f && f <= right) {
            filter_[i] = 1.0f * (right - f) / (right - middle);
        } else if (right < f) {
            filter_[i] = 0;
        } else {
            assert(false && "TriFilterBank argument wrong or implementation bug");
        }
    }
}

double TriFilterBank::filter(vector<double> input) {
    uint32_t filter_size = filter_.size();
    assert(input.size() == filter_size
           && "Dimension mismatch in TriFilterBank filter");

    double sum = 0;
    for (uint32_t i = 0; i < filter_size; i++) {
        sum += input[i] * filter_[i];
    }
    return sum;
}

MFCC::MFCC(uint32_t sampleRate, uint32_t FFTSize,
           double startFreq, double endFreq,
           uint32_t numFilterbankChannel,
           uint32_t numCepstralCoeff,
           uint32_t lifterParam)
        : num_cc_(numCepstralCoeff),
          lifter_param_(lifterParam) {
    vector<double> freqs(numFilterbankChannel + 2);
    double mel_start = TriFilterBank::toMelScale(startFreq);
    double mel_end = TriFilterBank::toMelScale(endFreq);
    double mel_step = (mel_end - mel_start) / (numFilterbankChannel + 1);

    for (uint32_t i = 0; i < numFilterbankChannel + 2; i++) {
        freqs[i] = TriFilterBank::fromMelScale(mel_start + i * mel_step);
    }

    for (uint32_t i = 0; i < numFilterbankChannel; i++) {
        filters_.push_back(TriFilterBank(freqs[i],
                                         freqs[i + 1],
                                         freqs[i + 2],
                                         sampleRate,
                                         FFTSize));
    }
}

vector<double> MFCC::getLFBE(const vector<double>& fft) {
    uint32_t M = filters_.size();
    vector<double> lfbe(M);
    for (uint32_t i = 0; i < M; i++) {
        double energy = filters_[i].filter(fft);
        if (energy == 0) {
            // Prevent log_energy goes to -inf...
            lfbe[i] = 0;
        } else {
            lfbe[i] = log(energy);
        }
    }
    return lfbe;
}

vector<double> MFCC::getCC(const vector<double>& lfbe) {
    uint32_t M = filters_.size();
    vector<double> cc(num_cc_);
    for (uint32_t i = 0; i < num_cc_; i++) {
        for (uint32_t j = 0; j < M; j++) {
            // [1] j is 1:M not 0:(M-1), so we change (j - 0.5) to (j + 0.5)
            cc[i] += sqrt(2.0 / M) * lfbe[j] * cos(PI * i / M * (j + 0.5));
        }
    }
    return cc;
}

vector<double> MFCC::lifterCC(const vector<double>& cc) {
    vector<double> liftered(num_cc_);
    uint32_t L = lifter_param_;
    for (uint32_t i = 0; i < num_cc_; i++) {
        liftered[i] = (1 + 1.0f * L / 2 * sin(PI * i / L)) * cc[i];
    }
    return liftered;
}
