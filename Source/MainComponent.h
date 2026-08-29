#pragma once

#include <JuceHeader.h>
#include "AssTapTrack.h"

//==============================================================================
class MainComponent : public juce::AudioAppComponent,
    public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void prepareToPlay(int samplesPerBlockExpected,
        double sampleRate) override;

    void getNextAudioBlock(
        const juce::AudioSourceChannelInfo& bufferToFill) override;

    void releaseResources() override;

    void timerCallback() override;

private:
    // Global journey-time temperament shared by all tracks.
    juce::Slider timeKnob;
    std::unique_ptr<juce::LookAndFeel> timeKnobLookAndFeel;

    juce::Image metalBackground;
    juce::Image sonicOnionLogo;
    AssTapTrack track1{ 1, juce::Colour(140, 190, 190) };
    AssTapTrack track2{ 2, juce::Colour(165, 185, 205) };
    AssTapTrack track3{ 3, juce::Colour(185, 175, 205) };
    AssTapTrack track4{ 4, juce::Colour(190, 180, 155) };
    AssTapTrack track5{ 5, juce::Colour(160, 195, 165) };
    AssTapTrack track6{ 6, juce::Colour(200, 155, 175) };
    AssTapTrack track7{ 7, juce::Colour(155, 170, 210) };
    AssTapTrack track8{ 8, juce::Colour(205, 170, 145) };

    juce::AudioBuffer<float> trackBuffer1;
    juce::AudioBuffer<float> trackBuffer2;
    juce::AudioBuffer<float> trackBuffer3;
    juce::AudioBuffer<float> trackBuffer4;
    juce::AudioBuffer<float> trackBuffer5;
    juce::AudioBuffer<float> trackBuffer6;
    juce::AudioBuffer<float> trackBuffer7;
    juce::AudioBuffer<float> trackBuffer8;
    juce::AudioBuffer<float> chorusBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    // Shared warm stereo chorus, intentionally closer to a restrained
    // JUNO Chorus I than Chorus II or I+II.
    juce::dsp::Chorus<float> sharedChorus;

    juce::Reverb sharedReverb;

    //--------------------------------------------------------------------------
    // WHAT-YOU-HEAR master recorder.
    juce::TextButton recordButton{ "REC" };
    std::unique_ptr<juce::LookAndFeel> recordButtonLookAndFeel;
    juce::TimeSliceThread recordingThread{ "AssTap Recording Thread" };

    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter{ nullptr };

    double masterSampleRate = 44100.0;
    juce::File currentRecordingFile;
    juce::Time recordingStartTime;

    std::atomic<float> masterRmsL{ 0.0f };
    std::atomic<float> masterRmsR{ 0.0f };
    float vuDisplayL = 0.0f;
    float vuDisplayR = 0.0f;

    void toggleRecording();
    void startRecording();
    void stopRecording();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};