/*
  ==============================================================================

    MainComponent.cpp
    Created: 10 Jan 2018 11:52:07am
    Author:  Andrew Jerrim

  ==============================================================================
*/

#include "MainComponent.h"
#include "../Main.h"
#include "../Processing/ProcessorExamples.h"
#include "../Processing/ParametricEQ.h"

MainContentComponent::MainContentComponent (AudioDeviceManager& deviceManager)
    : AudioAppComponent (deviceManager)
{
    holdAudio.set (false);

    srcComponentA = std::make_unique<SourceComponent> ("A", &deviceManager);
    srcComponentB = std::make_unique<SourceComponent> ("B", &deviceManager);

// =================================================================================================================================
// +++      Here is where to instantiate the processors being tested (it's OK to leave one as nullptr if you don't need it)      +++
// =================================================================================================================================
    procComponentA = std::make_unique<ProcessorComponent> ("A", new ParametricEQ());
    procComponentB = std::make_unique<ProcessorComponent> ("B", new ThruExample());
// =================================================================================================================================

    analyserComponent = std::make_unique<AnalyserComponent>();
    monitoringComponent = std::make_unique<MonitoringComponent> (&deviceManager, procComponentA.get(), procComponentB.get());

    addAndMakeVisible (srcComponentA.get());
    addAndMakeVisible (srcComponentB.get());
    addAndMakeVisible (procComponentA.get());
    addAndMakeVisible (procComponentB.get());
    addAndMakeVisible (analyserComponent.get());
    addAndMakeVisible (monitoringComponent.get());

    srcComponentA->setOtherSource (srcComponentB.get());
    srcComponentB->setOtherSource (srcComponentA.get());

    // Set small to force resize to minimum resize limit
    setSize (1, 1);

    oglContext.attachTo (*this);

    // Listen for changes to audio device so we can save the state
    deviceManager.addChangeListener(this);

    // Read saved audio device state from user settings
    std::unique_ptr<XmlElement> savedAudioDeviceState (DSPTestbenchApplication::getApp().appProperties.getUserSettings()->getXmlValue("AudioDeviceState"));

    // Specify the number of input and output channels that we want to open
    setAudioChannels (2, 2, savedAudioDeviceState.get());
}
void MainContentComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    sampleCounter.set(0);
    // Oscilloscope and FftScope use 4096 sample frames
    // If we want to have a bigger scope buffer then we would need to devise a fancier hold mechanism inside the AnalyserComponent
    holdSize.set (4096);

    const auto currentDevice = deviceManager.getCurrentAudioDevice();
	const auto numInputChannels = static_cast<uint32> (currentDevice->getActiveInputChannels().countNumberOfSetBits());
    const auto numOutputChannels = static_cast<uint32> (currentDevice->getActiveOutputChannels().countNumberOfSetBits());

    const dsp::ProcessSpec spec {
        sampleRate,
        static_cast<uint32> (samplesPerBlockExpected),
        jmax (numInputChannels, numOutputChannels)
    };

    srcBufferA = dsp::AudioBlock<float> (srcBufferMemoryA, spec.numChannels, samplesPerBlockExpected);
    srcBufferB = dsp::AudioBlock<float> (srcBufferMemoryB, spec.numChannels, samplesPerBlockExpected);
    tempBuffer = dsp::AudioBlock<float> (tempBufferMemory, spec.numChannels, samplesPerBlockExpected);
    
    srcComponentA->prepare (spec);
    srcComponentB->prepare (spec);
    procComponentA->prepare (spec);
    procComponentB->prepare (spec);
    analyserComponent->prepare (spec);
    monitoringComponent->prepare (spec);
}
void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // If these asserts fail then we need to handle larger than expected buffer sizes
    jassert (bufferToFill.numSamples <= srcBufferA.getNumSamples());
    jassert (bufferToFill.numSamples <= srcBufferB.getNumSamples());
    jassert (bufferToFill.numSamples <= tempBuffer.getNumSamples());

    dsp::AudioBlock<float> outputBlock (*bufferToFill.buffer, static_cast<size_t>(bufferToFill.startSample));
    
    // Copy current block into source buffers if needed
    if (srcComponentA->getMode() == SourceComponent::Mode::AudioIn)
        srcBufferA.copyFrom (outputBlock);
    if (srcComponentB->getMode() == SourceComponent::Mode::AudioIn)
        srcBufferB.copyFrom (outputBlock);

    // Generate audio from sources
    srcComponentA->process(dsp::ProcessContextReplacing<float> (srcBufferA));
    srcComponentB->process(dsp::ProcessContextReplacing<float> (srcBufferB));

    // Run audio through processors
    if (procComponentA->isProcessorEnabled())
    {
        routeSourcesAndProcess (procComponentA.get(), tempBuffer);
        outputBlock.copyFrom (tempBuffer);
        if (procComponentB->isProcessorEnabled()) // both active
        {
            routeSourcesAndProcess (procComponentB.get(), tempBuffer);
            outputBlock.add (tempBuffer);
        }
    }
    else if (procComponentB->isProcessorEnabled()) // processor A inactive
    {
        routeSourcesAndProcess (procComponentB.get(), tempBuffer);
        outputBlock.copyFrom (tempBuffer);
    }
    else // neither is active
        outputBlock.clear();

    // Run audio through analyser (note that the analyser isn't expected to alter the outputBlock)
    if (analyserComponent->isProcessing())
        analyserComponent->process (dsp::ProcessContextReplacing<float> (outputBlock));

    // Run audio through monitoring section
    if (monitoringComponent->isMuted())
        outputBlock.clear();
    else
        monitoringComponent->process (dsp::ProcessContextReplacing<float> (outputBlock));

    if (holdAudio.get())
    {
        sampleCounter.set (sampleCounter.get() + bufferToFill.numSamples);
        if (sampleCounter.get() > holdSize.get())
        {
            analyserComponent->suspendProcessing();
            // Close audio device from another thread (note that calling addJob isn't usually safe on the audio thread - but we're closing it anyway!)
            threadPool.addJob ([this] { deviceManager.closeAudioDevice(); });
        }
    }
}
void MainContentComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.
    srcBufferA.clear();
    srcBufferB.clear();
    tempBuffer.clear();
}
void MainContentComponent::paint (Graphics& g)
{
    g.fillAll (DspTestBenchLnF::ApplicationColours::componentBackground());
}
void MainContentComponent::resized()
{
    using Track = Grid::TrackInfo;
    const auto monitoringComponentHeight = Grid::Px (monitoringComponent->getMinimumHeight());
    const auto margin = GUI_GAP_I(2);
    Grid grid;
    grid.rowGap = GUI_GAP_PX(2);
    grid.columnGap = GUI_GAP_PX(2);
    if (analyserIsExpanded)
    {
        analyserComponent->setBounds (getBounds());
        srcComponentA->setVisible (false);
        srcComponentB->setVisible (false);
        procComponentA->setVisible (false);
        procComponentB->setVisible (false);
        grid.templateRows = { Track (1_fr), Track (monitoringComponentHeight) };
        grid.templateColumns = { Track (1_fr) };
        grid.items.addArray ({ GridItem (analyserComponent.get()), GridItem (monitoringComponent.get()) });
    }
    else
    {
        srcComponentA->setVisible (true);
        srcComponentB->setVisible (true);
        procComponentA->setVisible (true);
        procComponentB->setVisible (true);
        
        const auto srcComponentHeight = Grid::Px (jmax (srcComponentA->getMinimumHeight(), srcComponentB->getMinimumHeight()));
        const auto procComponentHeight = Grid::Px (jmax (procComponentA->getPreferredHeight(), procComponentB->getPreferredHeight()));

        // Assume both source components have the same width and that this is also sufficient for the processor components
        const auto srcWidth = srcComponentA->getMinimumWidth();

        auto requiredWidthForSourcesAndProcessors = srcWidth * 4;
        requiredWidthForSourcesAndProcessors += GUI_GAP_I(2) * 3;   // Gaps between components
        requiredWidthForSourcesAndProcessors += margin * 2;         // Gaps at edges

        if (requiredWidthForSourcesAndProcessors < getWidth())
        {
            // Put sources and processors on first row
            grid.templateRows = { Track (srcComponentHeight), Track (1_fr), Track (monitoringComponentHeight) };
            grid.templateColumns = { Track (1_fr), Track (1_fr), Track (1_fr), Track (1_fr) };
            grid.items.addArray({   GridItem (srcComponentA.get()), GridItem (srcComponentB.get()), GridItem (procComponentA.get()), GridItem (procComponentB.get()),
                                    GridItem (analyserComponent.get()).withArea ({ }, GridItem::Span (4)),
                                    GridItem (monitoringComponent.get()).withArea ({ }, GridItem::Span (4))
                                });
        }
        else
        {
            // First row is sources, second row is processors
            grid.templateRows = { Track (srcComponentHeight), Track (procComponentHeight), Track (1_fr), Track (monitoringComponentHeight) };
            grid.templateColumns = { Track (1_fr), Track (1_fr) };
            grid.items.addArray({   GridItem (srcComponentA.get()), GridItem (srcComponentB.get()),
                                    GridItem (procComponentA.get()), GridItem (procComponentB.get()),
                                    GridItem (analyserComponent.get()).withArea ({ }, GridItem::Span (2)),
                                    GridItem (monitoringComponent.get()).withArea ({ }, GridItem::Span (2))
                                });
        }
    }
    grid.autoFlow = Grid::AutoFlow::row;
    grid.performLayout (getLocalBounds().reduced (margin, margin));
}
void MainContentComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == &deviceManager)
    {
        std::unique_ptr<XmlElement> xml (deviceManager.createStateXml());
        DSPTestbenchApplication::getApp().appProperties.getUserSettings()->setValue ("AudioDeviceState", xml.get());
    }
}
void MainContentComponent::triggerSnapshot ()
{
    srcComponentA->storeWavePlayerState();
    srcComponentB->storeWavePlayerState();

    deviceManager.closeAudioDevice();
    
    // Reset components to ensure consistent behaviour for hold function
    srcComponentA->prepForSnapShot();
    srcComponentB->prepForSnapShot();
    procComponentA->reset();
    procComponentB->reset();
    analyserComponent->reset();
    monitoringComponent->reset();

    // Ensure the analyser isn't paused
    analyserComponent->activateProcessing();

    // Set a flag & sample counter so we can stop the device again once a certain number of samples have been processed        
    holdAudio.set (true);
    sampleCounter.set (0);

    // Note that restarting the audio device will cause prepare to be called
    deviceManager.restartLastAudioDevice();
}
void MainContentComponent::resumeStreaming()
{
    analyserComponent->activateProcessing();
    holdAudio.set (false);
    deviceManager.restartLastAudioDevice();
}
void MainContentComponent::setAnalyserExpanded (const bool shouldBeExpanded)
{
    analyserIsExpanded = shouldBeExpanded;
    resized();
}
ProcessorHarness* MainContentComponent::getProcessorHarness (const int index)
{
    jassert (index >=0 && index < 2);
    if (index == 0)
        return procComponentA->processor.get();
    else
        return procComponentB->processor.get();
}
SourceComponent* MainContentComponent::getSourceComponentA()
{
    return srcComponentA.get();
}
void MainContentComponent::routeSourcesAndProcess (ProcessorComponent* processor, dsp::AudioBlock<float>& temporaryBuffer)
{
    // Route signal sources
    if (processor->isSourceConnectedA())
    {
        temporaryBuffer.copyFrom (srcBufferA);
        if (processor->isSourceConnectedB()) // both sources connected
            temporaryBuffer.add (srcBufferB);
    }
    else if (processor->isSourceConnectedB()) // source A not connected
        temporaryBuffer.copyFrom (srcBufferB);
    else // Neither source is connected
        temporaryBuffer.clear(); 
    
    // Perform processing
    processor->process (dsp::ProcessContextReplacing<float> (temporaryBuffer));
    
    // Invert processor output as appropriate
    if (processor->isInverted())
        temporaryBuffer.multiplyBy (-1.0f);
}
