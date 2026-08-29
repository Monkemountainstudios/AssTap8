#pragma once

#include <JuceHeader.h>

//==============================================================================
class AssTapTrack : public juce::Component
{
public:
    AssTapTrack(int trackNumber, juce::Colour waveformColour);
    ~AssTapTrack() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    void releaseResources();

    // Global TIME temperament supplied by MainComponent.
    // 0.0 = slow (about 30-50 s), 1.0 = really slow (about 45-200 s).
    void setTimeTemperament(double normalizedTime);

    float getReverbSend() const;
    float getChorusSend() const;
    float getTrimGain() const;

    // Renders this track into a buffer starting at sample 0.
    void processBlock(juce::AudioBuffer<float>& output, int numSamples);

    // Called by MainComponent's 30 Hz GUI timer.
    void timerUpdate();

private:
    void loadAudioFile();

    // Manual and automatic loop movement use the same destination generator.
    void chooseNewLoopDestination(bool automaticJourney = false);

    float getSourceSample(int channel, double position) const;
    float getSafetyGain(double position) const;

    void startTurn();
    void startWrap(double loopStart, double loopEnd);

    //--------------------------------------------------------------------------
    // Mutation brain

    enum class MutationJourney
    {
        none,
        turn,
        loop,
        volume,
        pan,
        filter,
        octave,
        loopVolume,
        loopPan,
        loopFilter
    };

    void makeAutomaticMutationDecision();
    void startVolumeJourney(bool asCompound = false);
    void startPanJourney(bool asCompound = false);
    void startFilterJourney(bool asCompound = false);

    double chooseJourneySeconds();

    void armOctaveToggle();

    void updateContinuousMutations();

    // Display-only activity telemetry.
    void drawActivityStrip(juce::Graphics& g,
        juce::Rectangle<float> area,
        juce::Colour colour,
        float currentA,
        float targetA,
        bool active,
        float currentB = -1.0f,
        float targetB = -1.0f) const;

    float getFilterDisplayPosition(bool target) const;
    float getVolumeDisplayPosition(bool target) const;
    void updateFilterCoefficients();
    float processFilters(int channel, float sample);

    static double randomLogFrequency(juce::Random& rng,
        double minHz,
        double maxHz);

    //--------------------------------------------------------------------------
    int trackNumber = 1;
    juce::Colour waveformColour;

    //--------------------------------------------------------------------------
    // Controls

    juce::TextButton loadButton{ "LOAD" };
    juce::TextButton unloadButton;
    juce::TextButton reverseButton{ "REVERSE" };
    juce::TextButton mutateButton{ "MUTATE" };
    juce::TextButton loopMutateButton{ "MOVE LOOP" };
    juce::TextButton muteButton{ "MUTE" };

    juce::Slider probabilityKnob;
    juce::Slider trimKnob;
    juce::Slider chorusSendKnob;
    juce::Slider reverbSendKnob;
    std::unique_ptr<juce::LookAndFeel> barLookAndFeel;
    //--------------------------------------------------------------------------
    // Audio file

    juce::AudioFormatManager formatManager;

    // Protects audioBuffer and playback/file state during a hot load.
    // File decoding happens outside this lock; only the final swap is locked.
    mutable juce::CriticalSection audioStateLock;

    juce::AudioBuffer<float> audioBuffer;
    juce::int64 loadedLengthInSamples = 0;
    int loadedNumChannels = 0;

    double deviceSampleRate = 44100.0;
    double sourceSampleRate = 44100.0;

    //--------------------------------------------------------------------------
    // On-load source normalization
    //
    // This is a hidden one-time gain correction derived from the whole file.
    // It only brings wildly mismatched files into the same general region.
    // TRIM and AssTap's autonomous volume movement remain musically separate.

    float sourceNormalizationGain = 1.0f;

    static constexpr double normalizationTargetDb = -20.0;
    static constexpr double normalizationMaxBoostDb = 12.0;
    static constexpr double normalizationMaxCutDb = -18.0;

    //--------------------------------------------------------------------------
    // Manual MUTE: 15-second fade while the tape keeps evolving.
    float currentMuteGain = 1.0f;
    float targetMuteGain = 1.0f;
    juce::int64 muteSamplesRemaining = 0;
    static constexpr double muteFadeSeconds = 15.0;
    // Short temporary fade used when physically ejecting a tape.
    bool unloadPending = false;
    float muteTargetBeforeUnload = 1.0f;
    static constexpr double unloadFadeSeconds = 0.75;

    //--------------------------------------------------------------------------
    // Loop state

    double loopStartNormalized = 0.20;
    double loopEndNormalized = 0.70;

    double targetLoopStartNormalized = 0.20;
    double targetLoopEndNormalized = 0.70;

    bool loopTargetActive = false;

    static constexpr double safetyFadeSeconds = 0.020;

    // Source/loop guard rails.
    static constexpr double minimumSourceSeconds = 0.200;
    static constexpr double minimumLoopSeconds = 0.300;
    static constexpr double maximumSourceSeconds = 600.0;

    // True elapsed-time LOOP journey state.
    double loopJourneyStartStartNormalized = 0.20;
    double loopJourneyStartEndNormalized = 0.70;
    double loopJourneyElapsedSeconds = 0.0;
    double loopJourneyDurationSeconds = 30.0;

    // Same-direction wrap:
    static constexpr double loopCrossfadeSeconds = 0.100;

    // Direction change:
    // fade fully out -> reverse at silence -> fade in.
    static constexpr double turnFadeSeconds = 0.120;

    //--------------------------------------------------------------------------
    // Main tapehead

    double playPosition = 0.0;
    bool reversePlayback = false;

    //--------------------------------------------------------------------------
    // TAPE TOUCH
    //
    // The waveform itself behaves like physical tape.
    // Hold = gradually apply friction.
    // Release = motor snaps back with a tiny overshoot/flutter.

    std::atomic<bool> tapeTouchHeld{ false };

    double tapeTouchSpeed = 1.0;
    double tapeTouchReleaseStartSpeed = 1.0;

    bool tapeTouchWasHeld = false;
    double tapeTouchRecoverySeconds = 0.0;

    static constexpr double tapeTouchMinimumSpeed = 0.72;
    static constexpr double tapeTouchSlowdownSeconds = 2.5;
    static constexpr double tapeTouchRecoveryDuration = 0.75;
    //--------------------------------------------------------------------------
    // Two-head WRAP state

    bool crossfadeActive = false;
    double incomingPosition = 0.0;

    int crossfadeSamplesDone = 0;
    int crossfadeTotalSamples = 1;

    //--------------------------------------------------------------------------
    // TURN state

    enum class TurnState
    {
        idle,
        fadingOut,
        fadingIn
    };

    TurnState turnState = TurnState::idle;

    int turnSamplesDone = 0;
    int turnSamplesTotal = 1;

    bool manualReverseRequested = false;

    // Stops repeated automatic rolls at the same loop boundary.
    bool boundaryDecisionMade = false;

    //--------------------------------------------------------------------------
    // OCTAVE state
    //
    // Rare binary structural mutation:
    // 1x <-> 1/2x. The request is armed, then executed at the next loop
    // boundary using a fade-out / toggle / fade-in transition.

    bool halfSpeedActive = false;
    bool octaveTogglePending = false;
    bool octaveTransitionActive = false;

    enum class OctaveTransitionState
    {
        idle,
        fadingOut,
        fadingIn
    };

    OctaveTransitionState octaveTransitionState =
        OctaveTransitionState::idle;

    int octaveTransitionSamplesDone = 0;
    int octaveTransitionSamplesTotal = 1;

    static constexpr double octaveTransitionFadeSeconds = 0.120;

    //--------------------------------------------------------------------------
    // Mutation state

    bool mutateEnabled = false;
    MutationJourney currentJourney = MutationJourney::none;

    juce::Random random;

    //--------------------------------------------------------------------------
    // Global TIME temperament

    std::atomic<double> timeTemperament{ 0.0 };

    //--------------------------------------------------------------------------
    // VOLUME mutation
    //
    // TRIM remains the user's baseline. This is a small autonomous lean
    // forward/back around it.

    double currentMutationVolumeDb = 0.0;
    double targetMutationVolumeDb = 0.0;
    juce::int64 volumeSamplesRemaining = 0;

    static constexpr double minMutationVolumeDb = -6.0;
    static constexpr double maxMutationVolumeDb = 3.0;

    //--------------------------------------------------------------------------
    // PAN mutation

    double currentPan = 0.0;
    double targetPan = 0.0;
    juce::int64 panSamplesRemaining = 0;

    // Stereo tracks keep their image; pan becomes a gentler balance move.
    static constexpr double stereoBalanceMaxCutDb = 9.0;

    //--------------------------------------------------------------------------
    // FILTER mutation
    //
    // One FILTER family, three hidden workers:
    // HP + LP + a broad tasteful peaking bump/cut.

    double currentHpHz = 20.0;
    double targetHpHz = 20.0;

    double currentLpHz = 18000.0;
    double targetLpHz = 18000.0;

    double currentPeakHz = 1000.0;
    double targetPeakHz = 1000.0;

    double currentPeakGainDb = 0.0;
    double targetPeakGainDb = 0.0;

    double currentPeakQ = 0.90;
    double targetPeakQ = 0.90;

    juce::int64 filterSamplesRemaining = 0;
    int filterCoefficientCountdown = 0;

    juce::IIRFilter hpFilterL;
    juce::IIRFilter hpFilterR;
    juce::IIRFilter lpFilterL;
    juce::IIRFilter lpFilterR;
    juce::IIRFilter peakFilterL;
    juce::IIRFilter peakFilterR;

    //--------------------------------------------------------------------------
    // Waveform

    juce::AudioThumbnailCache thumbnailCache{ 5 };
    juce::AudioThumbnail thumbnail{ 512, formatManager, thumbnailCache };

    // Precomputed display-only waveform, auto-scaled independently of audio.
    static constexpr int waveformDisplayBins = 1024;
    std::array<std::vector<float>, 2> waveformDisplayMin;
    std::array<std::vector<float>, 2> waveformDisplayMax;
    int waveformDisplayChannels = 0;

    juce::String currentFileName{ "No Tape Loaded" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AssTapTrack)
};