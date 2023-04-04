#pragma once

#include "ProcessorHarness.h"

class ParametricEQ : public ProcessorHarness
{
public:
    ParametricEQ();
    ~ParametricEQ() override = default;

    void prepare(const dsp::ProcessSpec& spec) override;
    void process(const dsp::ProcessContextReplacing<float>& context) override;
    void reset() override;

    String getProcessorName() override;
    String getControlName(const int index) override;
    double getDefaultControlValue(const int index) override;

    enum 
    {
        frequencyControl,
        gainControl,
        numControls
    };


    juce::Range<double> getControlRange(const int index) override;

private:
    void init();
    void calculateCoefficients();

    int numChannels = 0;
};
