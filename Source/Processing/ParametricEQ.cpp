/*
  ==============================================================================

    ParametricEQ.cpp
    Created: 3 Apr 2023 5:17:18pm
    Author:  Matt

  ==============================================================================
*/

#include "ParametricEQ.h"

ParametricEQ::ParametricEQ() :
    ProcessorHarness(numControls)
{
}

void ParametricEQ::prepare(const dsp::ProcessSpec& spec)
{
    numChannels = static_cast<int> (spec.numChannels);
}

void ParametricEQ::process(const dsp::ProcessContextReplacing<float>& context)
{
    jassert(context.getInputBlock().getNumChannels() == context.getOutputBlock().getNumChannels());
    context.getOutputBlock().copyFrom(context.getInputBlock());
}

void ParametricEQ::reset()
{

}

juce::String ParametricEQ::getProcessorName()
{
    return "PEQ";
}

juce::String ParametricEQ::getControlName(const int index)
{
    switch (index)
    {
        case frequencyControl: return "Frequency";
        case gainControl: return "Gain";
    }

    return {};
}

double ParametricEQ::getDefaultControlValue(const int index)
{
    switch (index)
    {
    case frequencyControl: return 440.0;
    case gainControl: return 0.0;
    }

    return 0.0;
}

juce::Range<double> ParametricEQ::getControlRange(const int index)
{
    switch (index)
    {
    case frequencyControl: return { 20.0, 20000.0 };
    case gainControl: return { -36.0, 36.0 };
    }

    return { 0.0, 1.0 };
}

void ParametricEQ::init()
{

}

void ParametricEQ::calculateCoefficients()
{

}
