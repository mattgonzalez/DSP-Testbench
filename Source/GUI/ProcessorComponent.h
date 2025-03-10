/*
  ==============================================================================

    ProcessorComponent.h
    Created: 10 Jan 2018
    Author:  Andrew Jerrim

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "../Processing/ProcessorHarness.h"

class ProcessorComponent final : public Component, dsp::ProcessorBase
{
public:
    
    ProcessorComponent (const String& processorId, ProcessorHarness* processorToTest);
    ~ProcessorComponent() override;

    void paint (Graphics& g) override;
    void resized() override;
    float getPreferredHeight() const;

    void prepare (const dsp::ProcessSpec& spec) override;
    void process (const dsp::ProcessContextReplacing<float>& context) override;
    void reset() override;

    bool isSourceConnectedA() const noexcept;
    bool isSourceConnectedB() const noexcept;
    bool isProcessorEnabled() const noexcept;
    bool isInverted() const noexcept;
    bool isMuted() const noexcept;
    void muteProcessor (const bool shouldBeMuted = true);
    void disableProcessor (const bool shouldBeDisabled = true);

    std::shared_ptr<ProcessorHarness> processor {};

private:
    
    class ControlComponent : public Component
    {
    public:
        ControlComponent (const int index, ProcessorHarness* processorBeingControlled);
        void paint (Graphics& g) override;
        void resized() override;
        double getCurrentControlValue() const;

    private:
        int const controlIndex;
        ProcessorHarness* processor;
        Label controlLabel {};
        Slider controlSlider {};
        Atomic<double> currentControlValue;
    };

    class ControlArrayComponent : public Component
    {
    public:
        explicit ControlArrayComponent (OwnedArray<ControlComponent>* controlComponentsToReferTo);

        void paint (Graphics& g) override;
        void resized() override;
        float getPreferredHeight() const;

        /** This must be called to add the controls and make them visible (once they are set up in the parent). */
        void initialiseControls();

    private:
        OwnedArray<ControlComponent>* controlComponents {};
    };

    String keyName;
    std::unique_ptr<XmlElement> config;

    Label lblTitle;
    TextButton btnSourceA;
    TextButton btnSourceB;
    TextButton btnDisable;
    TextButton btnInvert;
    TextButton btnMute;

    // TODO - add variable delay so that signals can be time aligned?

    Atomic<bool> statusSourceA = true;
    Atomic<bool> statusSourceB = false;
    Atomic<bool> statusDisable = false;
    Atomic<bool> statusInvert = false;
    Atomic<bool> statusMute = false;

    Viewport viewport;
    OwnedArray<ControlComponent> controlArray {};
    ControlArrayComponent controlArrayComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorComponent)
};