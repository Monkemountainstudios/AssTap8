#include "AssTapTrack.h"
namespace
{
    class AssTapBarLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawLinearSlider(juce::Graphics& g,
            int x, int y, int width, int height,
            float sliderPos,
            float minSliderPos,
            float maxSliderPos,
            const juce::Slider::SliderStyle style,
            juce::Slider& slider) override
        {
            juce::ignoreUnused(minSliderPos, maxSliderPos);

            if (style != juce::Slider::LinearBarVertical)
            {
                juce::LookAndFeel_V4::drawLinearSlider(
                    g, x, y, width, height,
                    sliderPos, minSliderPos, maxSliderPos,
                    style, slider);
                return;
            }

            auto area = juce::Rectangle<float>(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(width),
                static_cast<float>(height));

            const auto background =
                slider.findColour(juce::Slider::backgroundColourId);

            const auto colour =
                slider.findColour(juce::Slider::trackColourId);

            // Dark recessed slot.
            g.setColour(background);
            g.fillRect(area);

            // Filled part of a vertical LinearBar runs from sliderPos downward.
            auto fill =
                juce::Rectangle<float>(
                    area.getX(),
                    sliderPos,
                    area.getWidth(),
                    area.getBottom() - sliderPos);

            if (fill.getHeight() > 0.0f)
            {
                // Heavy edge shading with a broad flat bright centre:
                // 25% ramp | 50% full colour | 25% ramp.
                juce::ColourGradient gradient(
                    colour.darker(0.65f),
                    fill.getX(),
                    fill.getCentreY(),
                    colour.darker(0.65f),
                    fill.getRight(),
                    fill.getCentreY(),
                    false);

                gradient.addColour(0.25, colour);
                gradient.addColour(0.75, colour);

                g.setGradientFill(gradient);
                g.fillRect(fill);
            }

            // Fine hardware edge.
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawRect(area, 1.0f);
        }
    };
    class AssTapMuteLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawButtonText(juce::Graphics& g,
            juce::TextButton& button,
            bool,
            bool) override
        {
            auto r = button.getLocalBounds().toFloat().reduced(7.0f, 5.0f);

            const bool muted =
                button.getProperties()["muteTarget"];

            const bool transitioning =
                button.getProperties()["muteTransition"];

            const bool blinkOn =
                button.getProperties()["muteBlink"];

            // During a transition, blink the pictogram only.
            if (transitioning && !blinkOn)
                return;

            auto colour = muted
                ? juce::Colour(220, 95, 85)
                : juce::Colour(205, 210, 212);

            g.setColour(colour);

            // Speaker body.
            const float cx = r.getCentreX() - 3.0f;
            const float cy = r.getCentreY();

            juce::Path speaker;
            speaker.startNewSubPath(cx - 7.0f, cy - 3.0f);
            speaker.lineTo(cx - 3.0f, cy - 3.0f);
            speaker.lineTo(cx + 1.0f, cy - 6.0f);
            speaker.lineTo(cx + 1.0f, cy + 6.0f);
            speaker.lineTo(cx - 3.0f, cy + 3.0f);
            speaker.lineTo(cx - 7.0f, cy + 3.0f);
            speaker.closeSubPath();

            g.fillPath(speaker);

            if (muted)
            {
                // Crossed-out speaker.
                g.drawLine(cx + 5.0f, cy - 5.0f,
                    cx + 12.0f, cy + 5.0f, 1.8f);

                g.drawLine(cx + 12.0f, cy - 5.0f,
                    cx + 5.0f, cy + 5.0f, 1.8f);
            }
            else
            {
                // Three simple sound rays.
                // Short - long - short, fanning outward.

                const float x = cx + 5.0f;
                const float y = cy;

                g.drawLine(
                    x, y - 1.0f,
                    x + 4.0f, y - 5.0f,
                    1.5f);

                g.drawLine(
                    x + 1.0f, y,
                    x + 7.0f, y,
                    1.7f);

                g.drawLine(
                    x, y + 1.0f,
                    x + 4.0f, y + 5.0f,
                    1.5f);
            }
        }
    };
}
//==============================================================================
AssTapTrack::AssTapTrack(int number, juce::Colour colour)
    : trackNumber(number),
    waveformColour(colour)
{
    formatManager.registerBasicFormats();
    barLookAndFeel =
        std::make_unique<AssTapBarLookAndFeel>();
    //--------------------------------------------------------------------------
    // LOAD

    addAndMakeVisible(loadButton);

    loadButton.onClick = [this]
        {
            loadAudioFile();
        };
    //--------------------------------------------------------------------------
    // EJECT / UNLOAD

    addAndMakeVisible(unloadButton);

    unloadButton.setButtonText(
        juce::String::fromUTF8("\xC3\x97"));

    unloadButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(38, 42, 45));

    unloadButton.setColour(
        juce::TextButton::textColourOffId,
        juce::Colour(205, 125, 120));

    unloadButton.onClick = [this]
        {
            const juce::ScopedLock stateLock(audioStateLock);

            if (loadedLengthInSamples <= 0 || unloadPending)
                return;

            // Remember whether the user had this tape muted or unmuted.
            muteTargetBeforeUnload = targetMuteGain;

            // Temporary fast fade to silence before removing the source.
            targetMuteGain = 0.0f;

            muteSamplesRemaining =
                juce::jmax<juce::int64>(
                    1,
                    static_cast<juce::int64>(
                        unloadFadeSeconds * deviceSampleRate));

            unloadPending = true;
        };
    //--------------------------------------------------------------------------
    // REVERSE

    addAndMakeVisible(reverseButton);

    reverseButton.onClick = [this]
        {
            if (turnState == TurnState::idle &&
                octaveTransitionState == OctaveTransitionState::idle &&
                !crossfadeActive)
            {
                manualReverseRequested = true;
            }
        };

    //--------------------------------------------------------------------------
    // MUTATE

    addAndMakeVisible(mutateButton);

    mutateButton.onClick = [this]
        {
            mutateEnabled = !mutateEnabled;

            mutateButton.setButtonText(
                mutateEnabled ? "MUTATE ON" : "MUTATE");
        };

    //--------------------------------------------------------------------------
    // PROBABILITY

    addAndMakeVisible(probabilityKnob);

    probabilityKnob.setSliderStyle(juce::Slider::LinearBarVertical);
    probabilityKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    probabilityKnob.setScrollWheelEnabled(true);
    probabilityKnob.setColour(juce::Slider::backgroundColourId,
        juce::Colour(13, 18, 21));
    probabilityKnob.setColour(juce::Slider::trackColourId,
        juce::Colour(155, 110, 220));
    probabilityKnob.setColour(juce::Slider::thumbColourId,
        juce::Colour(155, 110, 220).brighter(0.35f));

    probabilityKnob.setRange(0.0, 100.0, 1.0);
    probabilityKnob.setValue(35.0);
    probabilityKnob.setTextValueSuffix(" %");

    //--------------------------------------------------------------------------
    // TRIM

    addAndMakeVisible(trimKnob);

    trimKnob.setSliderStyle(juce::Slider::LinearBarVertical);
    trimKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    trimKnob.setScrollWheelEnabled(true);
    trimKnob.setColour(juce::Slider::backgroundColourId,
        juce::Colour(13, 18, 21));
    trimKnob.setColour(juce::Slider::trackColourId,
        juce::Colour(105, 195, 105));
    trimKnob.setColour(juce::Slider::thumbColourId,
        juce::Colour(105, 195, 105).brighter(0.35f));

    trimKnob.setRange(-18.0, 18.0, 0.1);
    trimKnob.setValue(0.0);
    trimKnob.setTextValueSuffix(" dB");

    //--------------------------------------------------------------------------
    // CHORUS SEND
    //
    // Shared warm chorus in MainComponent. This knob controls how much
    // of this track enters that common chorus bus.

    addAndMakeVisible(chorusSendKnob);

    chorusSendKnob.setSliderStyle(juce::Slider::LinearBarVertical);
    chorusSendKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    chorusSendKnob.setScrollWheelEnabled(true);
    chorusSendKnob.setColour(juce::Slider::backgroundColourId,
        juce::Colour(13, 18, 21));
    chorusSendKnob.setColour(juce::Slider::trackColourId,
        juce::Colour(135, 195, 235));
    chorusSendKnob.setColour(juce::Slider::thumbColourId,
        juce::Colour(135, 195, 235).brighter(0.35f));

    chorusSendKnob.setRange(0.0, 100.0, 1.0);
    chorusSendKnob.setValue(0.0);
    chorusSendKnob.setTextValueSuffix(" %");

    //--------------------------------------------------------------------------
    // REVERB SEND

    addAndMakeVisible(reverbSendKnob);

    reverbSendKnob.setSliderStyle(juce::Slider::LinearBarVertical);
    reverbSendKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    reverbSendKnob.setScrollWheelEnabled(true);
    reverbSendKnob.setColour(juce::Slider::backgroundColourId,
        juce::Colour(13, 18, 21));
    reverbSendKnob.setColour(juce::Slider::trackColourId,
        juce::Colour(225, 105, 145));
    reverbSendKnob.setColour(juce::Slider::thumbColourId,
        juce::Colour(225, 105, 145).brighter(0.35f));

    reverbSendKnob.setRange(0.0, 100.0, 1.0);
    reverbSendKnob.setValue(20.0);
    reverbSendKnob.setTextValueSuffix(" %");

    probabilityKnob.setLookAndFeel(barLookAndFeel.get());
    trimKnob.setLookAndFeel(barLookAndFeel.get());
    chorusSendKnob.setLookAndFeel(barLookAndFeel.get());
    reverbSendKnob.setLookAndFeel(barLookAndFeel.get());
    //--------------------------------------------------------------------------
    // MUTE - manual 15-second fade, transport/mutations keep running.

    addAndMakeVisible(muteButton);
    muteLookAndFeel =
        std::make_unique<AssTapMuteLookAndFeel>();

    muteButton.setLookAndFeel(
        muteLookAndFeel.get());

    muteButton.setButtonText("");
    muteButton.onClick = [this]
        {
            const juce::ScopedLock stateLock(audioStateLock);

            const float newTarget =
                targetMuteGain > 0.5f ? 0.0f : 1.0f;

            targetMuteGain = newTarget;

            // An empty tape has nothing to fade.
            // Its MUTE state changes instantly so a newly loaded tape
            // can begin completely silent.
            if (loadedLengthInSamples <= 0)
            {
                currentMuteGain = targetMuteGain;
                muteSamplesRemaining = 0;
            }
            else
            {
                muteSamplesRemaining =
                    juce::jmax<juce::int64>(
                        1,
                        static_cast<juce::int64>(
                            muteFadeSeconds * deviceSampleRate));
            }

        };

    //--------------------------------------------------------------------------
    // Manual loop wandering stays available even when MUTATE is off.

    addAndMakeVisible(loopMutateButton);

    loopMutateButton.onClick = [this]
        {
            chooseNewLoopDestination(false);
        };
}
AssTapTrack::~AssTapTrack()
{
    probabilityKnob.setLookAndFeel(nullptr);
    trimKnob.setLookAndFeel(nullptr);
    chorusSendKnob.setLookAndFeel(nullptr);
    reverbSendKnob.setLookAndFeel(nullptr);
    muteButton.setLookAndFeel(nullptr);
}
//==============================================================================
void AssTapTrack::prepareToPlay(int samplesPerBlockExpected,
    double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);

    deviceSampleRate = sampleRate;

    crossfadeTotalSamples =
        juce::jmax(
            1,
            static_cast<int>(
                loopCrossfadeSeconds * deviceSampleRate));

    turnSamplesTotal =
        juce::jmax(
            1,
            static_cast<int>(
                turnFadeSeconds * deviceSampleRate));

    octaveTransitionSamplesTotal =
        juce::jmax(
            1,
            static_cast<int>(
                octaveTransitionFadeSeconds * deviceSampleRate));

    // Start FILTER wide open and harmless.
    currentHpHz = targetHpHz = 20.0;

    const double safeTop =
        juce::jmax(2000.0, deviceSampleRate * 0.45);

    currentLpHz = targetLpHz =
        juce::jmin(20000.0, safeTop);

    currentPeakHz = targetPeakHz = 1000.0;
    currentPeakGainDb = targetPeakGainDb = 0.0;
    currentPeakQ = targetPeakQ = 0.90;

    hpFilterL.reset();
    hpFilterR.reset();
    lpFilterL.reset();
    lpFilterR.reset();
    peakFilterL.reset();
    peakFilterR.reset();

    updateFilterCoefficients();
}

void AssTapTrack::releaseResources()
{
    hpFilterL.reset();
    hpFilterR.reset();
    lpFilterL.reset();
    lpFilterR.reset();
    peakFilterL.reset();
    peakFilterR.reset();
}

//==============================================================================
void AssTapTrack::setTimeTemperament(double normalizedTime)
{
    timeTemperament.store(
        juce::jlimit(0.0, 1.0, normalizedTime),
        std::memory_order_relaxed);
}

//==============================================================================
double AssTapTrack::chooseJourneySeconds()
{
    const double t =
        juce::jlimit(
            0.0, 1.0,
            timeTemperament.load(std::memory_order_relaxed));

    // CCW: 20-45 s. CW: 45-200 s.
    const double minimumSeconds =
        20.0 + (25.0 * t);

    const double curved = t * t;

    const double maximumSeconds =
        45.0 + (155.0 * curved);

    return minimumSeconds
        + random.nextDouble()
        * (maximumSeconds - minimumSeconds);
}

//==============================================================================
float AssTapTrack::getSourceSample(int channel,
    double position) const
{
    if (loadedLengthInSamples <= 0 ||
        loadedNumChannels <= 0)
    {
        return 0.0f;
    }

    position = juce::jlimit(
        0.0,
        static_cast<double>(loadedLengthInSamples - 1),
        position);

    const int sampleIndex =
        static_cast<int>(position);

    const int sourceChannel =
        juce::jmin(channel, loadedNumChannels - 1);

    return audioBuffer.getSample(
        sourceChannel,
        sampleIndex);
}

//==============================================================================
float AssTapTrack::getTrimGain() const
{
    return juce::Decibels::decibelsToGain(
        static_cast<float>(trimKnob.getValue()));
}

float AssTapTrack::getReverbSend() const
{
    return static_cast<float>(
        reverbSendKnob.getValue() / 100.0);
}

float AssTapTrack::getChorusSend() const
{
    return static_cast<float>(
        chorusSendKnob.getValue() / 100.0);
}

//==============================================================================
float AssTapTrack::getSafetyGain(double position) const
{
    if (loadedLengthInSamples <= 0)
        return 0.0f;

    const double safetySamples =
        safetyFadeSeconds * sourceSampleRate;

    if (safetySamples <= 0.0)
        return 1.0f;

    float gain = 1.0f;

    if (position < safetySamples)
    {
        gain = static_cast<float>(
            position / safetySamples);
    }

    const double samplesToEnd =
        static_cast<double>(loadedLengthInSamples) - position;

    if (samplesToEnd < safetySamples)
    {
        gain = juce::jmin(
            gain,
            static_cast<float>(
                samplesToEnd / safetySamples));
    }

    return juce::jlimit(0.0f, 1.0f, gain);
}

//==============================================================================
void AssTapTrack::startTurn()
{
    if (turnState != TurnState::idle ||
        crossfadeActive)
    {
        return;
    }

    turnState = TurnState::fadingOut;
    turnSamplesDone = 0;

    boundaryDecisionMade = true;
    manualReverseRequested = false;
}

//==============================================================================
void AssTapTrack::startWrap(double loopStart,
    double loopEnd)
{
    if (crossfadeActive ||
        turnState != TurnState::idle)
    {
        return;
    }

    crossfadeActive = true;
    crossfadeSamplesDone = 0;

    if (!reversePlayback)
    {
        incomingPosition = loopStart;
    }
    else
    {
        const double speedMultiplier =
            halfSpeedActive ? 0.5 : 1.0;

        const double sourceStep =
            (sourceSampleRate / deviceSampleRate)
            * speedMultiplier;

        incomingPosition =
            loopEnd - sourceStep;
    }
}

//==============================================================================
double AssTapTrack::randomLogFrequency(juce::Random& rng,
    double minHz,
    double maxHz)
{
    minHz = juce::jmax(1.0, minHz);
    maxHz = juce::jmax(minHz + 1.0, maxHz);

    const double logMin = std::log(minHz);
    const double logMax = std::log(maxHz);

    return std::exp(
        logMin + rng.nextDouble() * (logMax - logMin));
}

//==============================================================================
void AssTapTrack::makeAutomaticMutationDecision()
{
    if (!mutateEnabled ||
        currentJourney != MutationJourney::none)
        return;

    const double probability =
        probabilityKnob.getValue() / 100.0;

    if (random.nextDouble() >= probability)
        return;

    // OCT is rarer going down and deliberately more likely to return.
    //
    // Normal speed:
    //   38% LOOP
    //   15% LOOP + VOL/PAN/FILTER
    //   14% FILTER
    //   10% VOLUME
    //   10% PAN
    //   10% TURN
    //    3% OCT down
    //
    // Half speed:
    //   OCT return rises to 10%. The other categories are compressed
    //   proportionally so the track is less likely to remain underground
    //   forever by statistical accident.

    const double octaveWeight =
        halfSpeedActive ? 0.10 : 0.03;

    const double remaining =
        1.0 - octaveWeight;

    const double roll =
        random.nextDouble();

    if (roll < octaveWeight)
    {
        currentJourney =
            MutationJourney::octave;

        armOctaveToggle();
        return;
    }

    const double r =
        (roll - octaveWeight) / remaining;

    if (r < 0.38)
    {
        currentJourney = MutationJourney::loop;
        chooseNewLoopDestination(true);
    }
    else if (r < 0.53)
    {
        const int companion =
            random.nextInt(3);

        if (companion == 0)
        {
            currentJourney =
                MutationJourney::loopVolume;

            chooseNewLoopDestination(false);
            startVolumeJourney(true);
        }
        else if (companion == 1)
        {
            currentJourney =
                MutationJourney::loopPan;

            chooseNewLoopDestination(false);
            startPanJourney(true);
        }
        else
        {
            currentJourney =
                MutationJourney::loopFilter;

            chooseNewLoopDestination(false);
            startFilterJourney(true);
        }
    }
    else if (r < 0.67)
    {
        startFilterJourney(false);
    }
    else if (r < 0.77)
    {
        startVolumeJourney(false);
    }
    else if (r < 0.87)
    {
        startPanJourney(false);
    }
    else
    {
        currentJourney =
            MutationJourney::turn;

        startTurn();
    }
}

//==============================================================================
void AssTapTrack::startVolumeJourney(bool asCompound)
{
    if (!asCompound)
        currentJourney = MutationJourney::volume;

    // Take one modest step from wherever the track currently is.
    // Mostly 0.75-2.5 dB, occasionally close to 3 dB.
    const double stepDb =
        0.75 + random.nextDouble() * 2.25;

    const double direction =
        random.nextBool() ? 1.0 : -1.0;

    targetMutationVolumeDb =
        juce::jlimit(
            minMutationVolumeDb,
            maxMutationVolumeDb,
            currentMutationVolumeDb + direction * stepDb);

    // If the clamp gave us effectively no move, gently head the other way.
    if (std::abs(targetMutationVolumeDb - currentMutationVolumeDb) < 0.05)
    {
        targetMutationVolumeDb =
            juce::jlimit(
                minMutationVolumeDb,
                maxMutationVolumeDb,
                currentMutationVolumeDb - direction * stepDb);
    }

    const double travelSeconds =
        chooseJourneySeconds();

    volumeSamplesRemaining =
        juce::jmax<juce::int64>(
            1,
            static_cast<juce::int64>(
                travelSeconds * deviceSampleRate));
}

//==============================================================================
void AssTapTrack::startPanJourney(bool asCompound)
{
    if (!asCompound)
        currentJourney = MutationJourney::pan;

    targetPan =
        -1.0 + random.nextDouble() * 2.0;

    if (std::abs(targetPan - currentPan) < 0.12)
    {
        targetPan =
            juce::jlimit(
                -1.0, 1.0,
                currentPan + (random.nextBool() ? 0.35 : -0.35));
    }

    const double travelSeconds =
        chooseJourneySeconds();

    panSamplesRemaining =
        juce::jmax<juce::int64>(
            1,
            static_cast<juce::int64>(
                travelSeconds * deviceSampleRate));
}

//==============================================================================
void AssTapTrack::armOctaveToggle()
{
    // Do not change speed immediately. Wait for the next loop boundary.
    octaveTogglePending = true;
    boundaryDecisionMade = true;
}

//==============================================================================
void AssTapTrack::startFilterJourney(bool asCompound)
{
    if (!asCompound)
        currentJourney = MutationJourney::filter;

    // FILTER itself is one mutation family. Inside it, choose one or two
    // workers most of the time; all three is deliberately rare.
    int howMany = 1;

    const double countRoll = random.nextDouble();

    if (countRoll > 0.95)
        howMany = 3;
    else if (countRoll > 0.70)
        howMany = 2;

    bool moveHp = false;
    bool moveLp = false;
    bool movePeak = false;

    for (int pick = 0; pick < howMany; ++pick)
    {
        int choice = random.nextInt(3);

        for (int retry = 0; retry < 6; ++retry)
        {
            const bool alreadyChosen =
                (choice == 0 && moveHp) ||
                (choice == 1 && moveLp) ||
                (choice == 2 && movePeak);

            if (!alreadyChosen)
                break;

            choice = random.nextInt(3);
        }

        if (choice == 0) moveHp = true;
        if (choice == 1) moveLp = true;
        if (choice == 2) movePeak = true;
    }

    const double safeTop =
        juce::jmax(2500.0, deviceSampleRate * 0.45);

    if (moveHp)
    {
        // Step logarithmically rather than teleport across the spectrum.
        const double factor =
            std::exp(
                (random.nextDouble() * 2.0 - 1.0)
                * std::log(2.6));

        targetHpHz =
            juce::jlimit(
                20.0,
                juce::jmin(5000.0, safeTop - 100.0),
                currentHpHz * factor);

        if (currentHpHz < 30.0 && targetHpHz < 30.0)
            targetHpHz = randomLogFrequency(random, 35.0, 800.0);
    }

    if (moveLp)
    {
        const double factor =
            std::exp(
                (random.nextDouble() * 2.0 - 1.0)
                * std::log(2.4));

        targetLpHz =
            juce::jlimit(
                500.0,
                safeTop,
                currentLpHz * factor);

        if (currentLpHz > safeTop * 0.90 &&
            targetLpHz > safeTop * 0.90)
        {
            targetLpHz =
                randomLogFrequency(
                    random,
                    2500.0,
                    safeTop * 0.90);
        }
    }

    if (movePeak)
    {
        const double factor =
            std::exp(
                (random.nextDouble() * 2.0 - 1.0)
                * std::log(3.0));

        targetPeakHz =
            juce::jlimit(
                80.0,
                juce::jmin(12000.0, safeTop),
                currentPeakHz * factor);

        // Tasteful broad bump OR cut.
        targetPeakGainDb =
            -3.0 + random.nextDouble() * 6.0;

        targetPeakQ =
            0.70 + random.nextDouble() * 0.50;
    }

    // For the first experiment we intentionally do NOT prevent HP and LP
    // crossing. If it produces wonderful Traveller-style glass shards,
    // the crime stays.

    const double travelSeconds =
        chooseJourneySeconds();

    filterSamplesRemaining =
        juce::jmax<juce::int64>(
            1,
            static_cast<juce::int64>(
                travelSeconds * deviceSampleRate));

    filterCoefficientCountdown = 0;
}

//==============================================================================
void AssTapTrack::updateContinuousMutations()
{
    const bool volumePart =
        currentJourney == MutationJourney::volume ||
        currentJourney == MutationJourney::loopVolume;

    if (volumePart && volumeSamplesRemaining > 0)
    {
        currentMutationVolumeDb +=
            (targetMutationVolumeDb - currentMutationVolumeDb)
            / static_cast<double>(volumeSamplesRemaining);

        --volumeSamplesRemaining;

        if (volumeSamplesRemaining <= 0)
        {
            currentMutationVolumeDb = targetMutationVolumeDb;

            if (currentJourney == MutationJourney::volume ||
                (currentJourney == MutationJourney::loopVolume &&
                    !loopTargetActive))
                currentJourney = MutationJourney::none;
        }
    }

    const bool panPart =
        currentJourney == MutationJourney::pan ||
        currentJourney == MutationJourney::loopPan;

    if (panPart && panSamplesRemaining > 0)
    {
        currentPan +=
            (targetPan - currentPan)
            / static_cast<double>(panSamplesRemaining);

        --panSamplesRemaining;

        if (panSamplesRemaining <= 0)
        {
            currentPan = targetPan;

            if (currentJourney == MutationJourney::pan ||
                (currentJourney == MutationJourney::loopPan &&
                    !loopTargetActive))
                currentJourney = MutationJourney::none;
        }
    }

    const bool filterPart =
        currentJourney == MutationJourney::filter ||
        currentJourney == MutationJourney::loopFilter;

    if (filterPart && filterSamplesRemaining > 0)
    {
        const double remaining =
            static_cast<double>(filterSamplesRemaining);

        currentHpHz +=
            (targetHpHz - currentHpHz) / remaining;
        currentLpHz +=
            (targetLpHz - currentLpHz) / remaining;
        currentPeakHz +=
            (targetPeakHz - currentPeakHz) / remaining;
        currentPeakGainDb +=
            (targetPeakGainDb - currentPeakGainDb) / remaining;
        currentPeakQ +=
            (targetPeakQ - currentPeakQ) / remaining;

        --filterSamplesRemaining;

        if (--filterCoefficientCountdown <= 0)
        {
            updateFilterCoefficients();
            filterCoefficientCountdown = 64;
        }

        if (filterSamplesRemaining <= 0)
        {
            currentHpHz = targetHpHz;
            currentLpHz = targetLpHz;
            currentPeakHz = targetPeakHz;
            currentPeakGainDb = targetPeakGainDb;
            currentPeakQ = targetPeakQ;

            updateFilterCoefficients();

            if (currentJourney == MutationJourney::filter ||
                (currentJourney == MutationJourney::loopFilter &&
                    !loopTargetActive))
                currentJourney = MutationJourney::none;
        }
    }
}

//==============================================================================
void AssTapTrack::updateFilterCoefficients()
{
    if (deviceSampleRate <= 0.0)
        return;

    const double nyquistSafe =
        deviceSampleRate * 0.45;

    const double hp =
        juce::jlimit(
            10.0,
            juce::jmax(20.0, nyquistSafe - 100.0),
            currentHpHz);

    const double lp =
        juce::jlimit(
            100.0,
            nyquistSafe,
            currentLpHz);

    const double peakHz =
        juce::jlimit(
            40.0,
            nyquistSafe,
            currentPeakHz);

    const double peakQ =
        juce::jlimit(
            0.50,
            2.0,
            currentPeakQ);

    const double peakGainLinear =
        juce::Decibels::decibelsToGain(
            static_cast<float>(currentPeakGainDb));

    const auto hpCoeffs =
        juce::IIRCoefficients::makeHighPass(
            deviceSampleRate,
            hp);

    const auto lpCoeffs =
        juce::IIRCoefficients::makeLowPass(
            deviceSampleRate,
            lp);

    const auto peakCoeffs =
        juce::IIRCoefficients::makePeakFilter(
            deviceSampleRate,
            peakHz,
            peakQ,
            peakGainLinear);

    hpFilterL.setCoefficients(hpCoeffs);
    hpFilterR.setCoefficients(hpCoeffs);

    lpFilterL.setCoefficients(lpCoeffs);
    lpFilterR.setCoefficients(lpCoeffs);

    peakFilterL.setCoefficients(peakCoeffs);
    peakFilterR.setCoefficients(peakCoeffs);
}

//==============================================================================
float AssTapTrack::processFilters(int channel, float sample)
{
    if (channel == 0)
    {
        sample = hpFilterL.processSingleSampleRaw(sample);
        sample = lpFilterL.processSingleSampleRaw(sample);
        sample = peakFilterL.processSingleSampleRaw(sample);
    }
    else
    {
        sample = hpFilterR.processSingleSampleRaw(sample);
        sample = lpFilterR.processSingleSampleRaw(sample);
        sample = peakFilterR.processSingleSampleRaw(sample);
    }

    return sample;
}

//==============================================================================
void AssTapTrack::processBlock(juce::AudioBuffer<float>& output,
    int numSamples)
{
    // The message thread only holds this lock for the final buffer/state swap.
    // This prevents the audio thread reading a buffer while it is being replaced.
    const juce::ScopedLock stateLock(audioStateLock);

    output.clear();

    if (loadedLengthInSamples <= 0)
        return;

    const float trimGain =
        getTrimGain();

    const double totalSourceSamples =
        static_cast<double>(loadedLengthInSamples);

    const double loopStart =
        loopStartNormalized * totalSourceSamples;

    const double loopEnd =
        loopEndNormalized * totalSourceSamples;

    const double speedMultiplier =
        halfSpeedActive ? 0.5 : 1.0;

    const double sourceStep =
        (sourceSampleRate / deviceSampleRate)
        * speedMultiplier;

    const double currentLoopLengthSourceSamples =
        juce::jmax(1.0, loopEnd - loopStart);

    // Never let the wrap crossfade consume more than 25% of the loop.
    const double loopCrossfadeSourceSamples =
        juce::jmin(
            loopCrossfadeSeconds * sourceSampleRate,
            currentLoopLengthSourceSamples * 0.25);

    const double turnFadeSourceSamples =
        turnFadeSeconds * sourceSampleRate;

    //==========================================================================
    for (int i = 0; i < numSamples; ++i)
    {
        //----------------------------------------------------------------------
        // Manual TURN can happen anywhere. Manual interference does not create
        // an automatic mutation journey.

        if (manualReverseRequested &&
            turnState == TurnState::idle &&
            !crossfadeActive)
        {
            startTurn();
        }

        //----------------------------------------------------------------------
        // Armed OCTAVE toggle.
        //
        // Wait until this tape reaches its next loop boundary, then perform
        // a soft fade-out -> speed toggle -> fade-in. No pitch shifter:
        // this is true tape-style 1/2 playback speed.

        if (octaveTogglePending &&
            octaveTransitionState == OctaveTransitionState::idle &&
            turnState == TurnState::idle &&
            !crossfadeActive)
        {
            bool reachedOctaveBoundary = false;

            if (!reversePlayback)
            {
                reachedOctaveBoundary =
                    playPosition >=
                    loopEnd - turnFadeSourceSamples;
            }
            else
            {
                reachedOctaveBoundary =
                    playPosition <=
                    loopStart + turnFadeSourceSamples;
            }

            if (reachedOctaveBoundary)
            {
                octaveTransitionActive = true;
                octaveTransitionState =
                    OctaveTransitionState::fadingOut;

                octaveTransitionSamplesDone = 0;
            }
        }

        //----------------------------------------------------------------------
        // Boundary mutation opportunity.
        //
        // Per-track law:
        // if this track is already travelling somewhere, finish that journey
        // first. Only an idle track may roll for another automatic action.

        if (turnState == TurnState::idle &&
            !crossfadeActive &&
            !boundaryDecisionMade &&
            currentJourney == MutationJourney::none)
        {
            bool insideDecisionZone = false;

            if (!reversePlayback)
            {
                insideDecisionZone =
                    playPosition >=
                    loopEnd - turnFadeSourceSamples;
            }
            else
            {
                insideDecisionZone =
                    playPosition <=
                    loopStart + turnFadeSourceSamples;
            }

            if (insideDecisionZone)
            {
                boundaryDecisionMade = true;
                makeAutomaticMutationDecision();
            }
        }

        //----------------------------------------------------------------------
        // If there is no TURN, the tape still performs its normal wrap.
        // LOOP/VOLUME/FILTER journeys do not interrupt playback.

        if (turnState == TurnState::idle &&
            !crossfadeActive)
        {
            bool shouldWrap = false;

            if (!reversePlayback)
            {
                shouldWrap =
                    playPosition >=
                    loopEnd - loopCrossfadeSourceSamples;
            }
            else
            {
                shouldWrap =
                    playPosition <=
                    loopStart + loopCrossfadeSourceSamples;
            }

            if (shouldWrap)
                startWrap(loopStart, loopEnd);
        }

        //----------------------------------------------------------------------
        // Advance any slow autonomous journey once per output sample.

        updateContinuousMutations();

        //----------------------------------------------------------------------
        // Current tape direction.

        const double direction =
            reversePlayback ? -1.0 : 1.0;

        //----------------------------------------------------------------------
        // TAPE TOUCH
        //
        // Hold: slowly load the motor down toward 0.72x.
        // Release: faster recovery with a tiny mechanical overshoot.

        const bool touchingTape =
            tapeTouchHeld.load(
                std::memory_order_relaxed);

        if (touchingTape)
        {
            tapeTouchWasHeld = true;
            tapeTouchRecoverySeconds = 0.0;

            const double slowdownPerSample =
                (1.0 - tapeTouchMinimumSpeed)
                /
                juce::jmax(
                    1.0,
                    tapeTouchSlowdownSeconds
                    * deviceSampleRate);

            tapeTouchSpeed =
                juce::jmax(
                    tapeTouchMinimumSpeed,
                    tapeTouchSpeed
                    - slowdownPerSample);
        }
        else
        {
            if (tapeTouchWasHeld)
            {
                tapeTouchWasHeld = false;

                tapeTouchReleaseStartSpeed =
                    tapeTouchSpeed;

                tapeTouchRecoverySeconds = 0.0;
            }

            if (tapeTouchSpeed != 1.0 ||
                tapeTouchRecoverySeconds > 0.0)
            {
                tapeTouchRecoverySeconds +=
                    1.0 / deviceSampleRate;

                const double t =
                    tapeTouchRecoverySeconds;

                if (t < 0.35)
                {
                    // Fast spring upward to slight overshoot.
                    const double p =
                        juce::jlimit(
                            0.0,
                            1.0,
                            t / 0.35);

                    const double smooth =
                        p * p * (3.0 - 2.0 * p);

                    tapeTouchSpeed =
                        tapeTouchReleaseStartSpeed
                        + (1.02
                            - tapeTouchReleaseStartSpeed)
                        * smooth;
                }
                else if (t < 0.55)
                {
                    // Tiny motor wobble below normal.
                    const double p =
                        (t - 0.35) / 0.20;

                    const double smooth =
                        p * p * (3.0 - 2.0 * p);

                    tapeTouchSpeed =
                        1.02
                        + (0.98 - 1.02)
                        * smooth;
                }
                else if (t <
                    tapeTouchRecoveryDuration)
                {
                    // Settle home.
                    const double p =
                        (t - 0.55)
                        /
                        (tapeTouchRecoveryDuration
                            - 0.55);

                    const double smooth =
                        p * p * (3.0 - 2.0 * p);

                    tapeTouchSpeed =
                        0.98
                        + (1.0 - 0.98)
                        * smooth;
                }
                else
                {
                    tapeTouchSpeed = 1.0;
                    tapeTouchRecoverySeconds = 0.0;
                }
            }
        }

        const double signedStep =
            sourceStep
            * tapeTouchSpeed
            * direction;

        //----------------------------------------------------------------------
        // Mutation volume is a small offset around the user's TRIM.

        const float mutationVolumeGain =
            juce::Decibels::decibelsToGain(
                static_cast<float>(
                    currentMutationVolumeDb));

        //----------------------------------------------------------------------
        // Manual 15-second MUTE fade. The transport and mutation brain never
        // stop, so an unmuted tape can return in a completely new state.

        if (muteSamplesRemaining > 0)
        {
            currentMuteGain +=
                (targetMuteGain - currentMuteGain)
                / static_cast<float>(muteSamplesRemaining);

            --muteSamplesRemaining;

            if (muteSamplesRemaining <= 0)
                currentMuteGain = targetMuteGain;
        }

        //----------------------------------------------------------------------
        // Generate stereo output.

        for (int channel = 0;
            channel < output.getNumChannels();
            ++channel)
        {
            float mainSample =
                getSourceSample(
                    channel,
                    playPosition);

            mainSample *=
                getSafetyGain(playPosition);

            mainSample *=
                sourceNormalizationGain;

            float result = mainSample;

            //------------------------------------------------------------------
            // Same-direction WRAP:
            // A/B equal-power crossfade.

            if (crossfadeActive)
            {
                const float progress =
                    juce::jlimit(
                        0.0f,
                        1.0f,
                        static_cast<float>(
                            crossfadeSamplesDone) /
                        static_cast<float>(
                            crossfadeTotalSamples));

                const float outgoingGain =
                    std::cos(
                        progress *
                        juce::MathConstants<float>::halfPi);

                const float incomingGain =
                    std::sin(
                        progress *
                        juce::MathConstants<float>::halfPi);

                float incomingSample =
                    getSourceSample(
                        channel,
                        incomingPosition);

                incomingSample *=
                    getSafetyGain(
                        incomingPosition);

                incomingSample *=
                    sourceNormalizationGain;

                result =
                    mainSample * outgoingGain +
                    incomingSample * incomingGain;
            }

            //------------------------------------------------------------------
            // TURN:
            // fade completely to silence before flipping direction.

            if (turnState == TurnState::fadingOut)
            {
                const float progress =
                    juce::jlimit(
                        0.0f,
                        1.0f,
                        static_cast<float>(
                            turnSamplesDone) /
                        static_cast<float>(
                            turnSamplesTotal));

                const float turnGain =
                    std::cos(
                        progress *
                        juce::MathConstants<float>::halfPi);

                result *= turnGain;
            }
            else if (turnState == TurnState::fadingIn)
            {
                const float progress =
                    juce::jlimit(
                        0.0f,
                        1.0f,
                        static_cast<float>(
                            turnSamplesDone) /
                        static_cast<float>(
                            turnSamplesTotal));

                const float turnGain =
                    std::sin(
                        progress *
                        juce::MathConstants<float>::halfPi);

                result *= turnGain;
            }

            //------------------------------------------------------------------
            // OCTAVE structural transition:
            // fade to silence, toggle speed, fade back in.

            if (octaveTransitionState ==
                OctaveTransitionState::fadingOut)
            {
                const float progress =
                    juce::jlimit(
                        0.0f,
                        1.0f,
                        static_cast<float>(
                            octaveTransitionSamplesDone) /
                        static_cast<float>(
                            octaveTransitionSamplesTotal));

                const float gain =
                    std::cos(
                        progress *
                        juce::MathConstants<float>::halfPi);

                result *= gain;
            }
            else if (octaveTransitionState ==
                OctaveTransitionState::fadingIn)
            {
                const float progress =
                    juce::jlimit(
                        0.0f,
                        1.0f,
                        static_cast<float>(
                            octaveTransitionSamplesDone) /
                        static_cast<float>(
                            octaveTransitionSamplesTotal));

                const float gain =
                    std::sin(
                        progress *
                        juce::MathConstants<float>::halfPi);

                result *= gain;
            }

            //------------------------------------------------------------------
            // TRIM = user baseline.
            // mutationVolumeGain = AssTap leaning forward/back.
            // FILTER = HP -> LP -> broad bump/cut.

            result *=
                trimGain * mutationVolumeGain;

            result =
                processFilters(
                    channel,
                    result);

            // PAN:
            // mono = full equal-power field;
            // stereo = gentler balance by attenuating only the far side.
            float panGain = 1.0f;

            if (loadedNumChannels <= 1)
            {
                const double angle =
                    (currentPan + 1.0)
                    * juce::MathConstants<double>::halfPi
                    * 0.5;

                panGain =
                    static_cast<float>(
                        channel == 0 ? std::cos(angle) : std::sin(angle));
            }
            else
            {
                if (currentPan > 0.0 && channel == 0)
                {
                    panGain =
                        juce::Decibels::decibelsToGain(
                            static_cast<float>(
                                -stereoBalanceMaxCutDb * currentPan));
                }
                else if (currentPan < 0.0 && channel == 1)
                {
                    panGain =
                        juce::Decibels::decibelsToGain(
                            static_cast<float>(
                                stereoBalanceMaxCutDb * currentPan));
                }
            }

            result *= panGain;
            result *= currentMuteGain;

            output.setSample(
                channel,
                i,
                result);
        }

        //----------------------------------------------------------------------
        // Main head movement.

        playPosition += signedStep;

        //----------------------------------------------------------------------
        // Incoming WRAP head follows same direction.

        if (crossfadeActive)
        {
            incomingPosition += signedStep;
            ++crossfadeSamplesDone;

            if (crossfadeSamplesDone >=
                crossfadeTotalSamples)
            {
                playPosition = incomingPosition;

                crossfadeActive = false;
                crossfadeSamplesDone = 0;

                // A new pass may become a future mutation opportunity.
                boundaryDecisionMade = false;
            }
        }

        //----------------------------------------------------------------------
        // OCTAVE state machine.

        if (octaveTransitionState ==
            OctaveTransitionState::fadingOut)
        {
            ++octaveTransitionSamplesDone;

            if (octaveTransitionSamplesDone >=
                octaveTransitionSamplesTotal)
            {
                halfSpeedActive =
                    !halfSpeedActive;

                octaveTogglePending = false;

                // Keep the head safely inside the current loop before
                // continuing at the new speed.
                playPosition =
                    juce::jlimit(
                        loopStart,
                        loopEnd - sourceStep,
                        playPosition);

                octaveTransitionState =
                    OctaveTransitionState::fadingIn;

                octaveTransitionSamplesDone = 0;
            }
        }
        else if (octaveTransitionState ==
            OctaveTransitionState::fadingIn)
        {
            ++octaveTransitionSamplesDone;

            if (octaveTransitionSamplesDone >=
                octaveTransitionSamplesTotal)
            {
                octaveTransitionState =
                    OctaveTransitionState::idle;

                octaveTransitionActive = false;
                octaveTransitionSamplesDone = 0;

                if (currentJourney ==
                    MutationJourney::octave)
                {
                    currentJourney =
                        MutationJourney::none;
                }

                boundaryDecisionMade = false;
            }
        }

        //----------------------------------------------------------------------
        // TURN state machine.

        if (turnState == TurnState::fadingOut)
        {
            ++turnSamplesDone;

            if (turnSamplesDone >= turnSamplesTotal)
            {
                // At silence: flip.
                reversePlayback =
                    !reversePlayback;

                playPosition =
                    juce::jlimit(
                        loopStart,
                        loopEnd - sourceStep,
                        playPosition);

                turnState =
                    TurnState::fadingIn;

                turnSamplesDone = 0;
            }
        }
        else if (turnState == TurnState::fadingIn)
        {
            ++turnSamplesDone;

            if (turnSamplesDone >= turnSamplesTotal)
            {
                turnState = TurnState::idle;
                turnSamplesDone = 0;

                if (currentJourney ==
                    MutationJourney::turn)
                {
                    currentJourney =
                        MutationJourney::none;
                }

                boundaryDecisionMade = false;
            }
        }

        //----------------------------------------------------------------------
        // Emergency containment.

        if (!crossfadeActive &&
            turnState == TurnState::idle &&
            octaveTransitionState == OctaveTransitionState::idle)
        {
            if (!reversePlayback &&
                playPosition >= loopEnd)
            {
                playPosition = loopStart;
                boundaryDecisionMade = false;
            }

            if (reversePlayback &&
                playPosition <= loopStart)
            {
                playPosition =
                    loopEnd - sourceStep;

                boundaryDecisionMade = false;
            }
        }
    }
}

//==============================================================================
void AssTapTrack::drawActivityStrip(juce::Graphics& g,
    juce::Rectangle<float> area,
    juce::Colour colour,
    float currentA,
    float targetA,
    bool active,
    float currentB,
    float targetB) const
{
    currentA = juce::jlimit(0.0f, 1.0f, currentA);
    targetA = juce::jlimit(0.0f, 1.0f, targetA);

    const bool hasSecond =
        currentB >= 0.0f && targetB >= 0.0f;

    if (hasSecond)
    {
        currentB = juce::jlimit(0.0f, 1.0f, currentB);
        targetB = juce::jlimit(0.0f, 1.0f, targetB);
    }

    const float y = area.getCentreY();
    const float x0 = area.getX();
    const float x1 = area.getRight();

    auto xFor = [x0, x1](float p)
        {
            return x0 + p * (x1 - x0);
        };

    // Resting = faint glow. Moving = brighter glow.
    g.setColour(colour.withAlpha(active ? 0.12f : 0.04f));
    g.drawLine(x0, y, x1, y, 4.0f);
    g.setColour(colour.withAlpha(active ? 0.90f : 0.28f));
    g.drawLine(x0, y, x1, y, active ? 1.35f : 0.8f);

    auto drawDot = [&](float p, float alpha, float size)
        {
            const float px = xFor(p);
            g.setColour(colour.withAlpha(alpha * 0.18f));
            g.fillEllipse(px - 3.0f, y - 3.0f, 6.0f, 6.0f);
            g.setColour(colour.brighter(0.30f).withAlpha(alpha));
            g.fillEllipse(px - size * 0.5f, y - size * 0.5f, size, size);
        };

    // Persistent current state dot(s).
    drawDot(currentA, active ? 0.95f : 0.58f, 2.0f);
    if (hasSecond)
        drawDot(currentB, active ? 0.95f : 0.58f, 2.0f);

    // Destination blinks during a journey.
    if (active)
    {
        const double phase =
            std::fmod(
                juce::Time::getMillisecondCounterHiRes(),
                700.0)
            / 700.0;

        if (phase < 0.52)
        {
            drawDot(targetA, 1.0f, 3.0f);
            if (hasSecond)
                drawDot(targetB, 1.0f, 3.0f);
        }
    }
}

float AssTapTrack::getFilterDisplayPosition(bool target) const
{
    const double hp =
        target ? targetHpHz : currentHpHz;

    const double lp =
        target ? targetLpHz : currentLpHz;

    const double peakGain =
        target ? targetPeakGainDb : currentPeakGainDb;

    const double safeTop =
        juce::jmax(2000.0, deviceSampleRate * 0.45);

    const double hpAmount =
        juce::jlimit(0.0, 1.0, hp / 5000.0);

    const double lpAmount =
        juce::jlimit(
            0.0,
            1.0,
            1.0 - (lp - 500.0)
            / juce::jmax(1.0, safeTop - 500.0));

    const double peakAmount =
        juce::jlimit(
            0.0,
            1.0,
            std::abs(peakGain) / 3.0);

    return static_cast<float>(
        juce::jlimit(
            0.0,
            1.0,
            0.42 * hpAmount
            + 0.42 * lpAmount
            + 0.16 * peakAmount));
}

float AssTapTrack::getVolumeDisplayPosition(bool target) const
{
    const double db =
        target
        ? targetMutationVolumeDb
        : currentMutationVolumeDb;

    return static_cast<float>(
        juce::jlimit(
            0.0,
            1.0,
            (db - minMutationVolumeDb)
            / juce::jmax(
                0.001,
                maxMutationVolumeDb
                - minMutationVolumeDb)));
}
//==============================================================================
void AssTapTrack::mouseDown(const juce::MouseEvent& event)
{
    const int loadW = 54;
    const int ejectW = 18;
    const int rightW = 210;

    const int tapeX = loadW + ejectW + 12;
    const int tapeY = 16;
    const int tapeH = 26;

    const int tapeW =
        juce::jmax(
            180,
            getWidth()
            - tapeX
            - rightW
            - 8);

    const juce::Rectangle<int> tapeArea(
        tapeX,
        tapeY,
        tapeW,
        tapeH);

    if (loadedLengthInSamples > 0 &&
        tapeArea.contains(event.getPosition()))
    {
        tapeTouchHeld.store(
            true,
            std::memory_order_relaxed);

        repaint();
    }
}

//==============================================================================
void AssTapTrack::mouseUp(const juce::MouseEvent&)
{
    if (tapeTouchHeld.exchange(
        false,
        std::memory_order_relaxed))
    {
        repaint();
    }
}
//==============================================================================
void AssTapTrack::paint(juce::Graphics& g)
{
    auto bounds =
        getLocalBounds().reduced(6);

    // ----------------------------------------------------------
    // Small header: track + filename + current action

    juce::String journeyLabel = "IDLE";

    switch (currentJourney)
    {
    case MutationJourney::turn:       journeyLabel = "TURN";        break;
    case MutationJourney::loop:       journeyLabel = "LOOP";        break;
    case MutationJourney::volume:     journeyLabel = "VOL";         break;
    case MutationJourney::pan:        journeyLabel = "PAN";         break;
    case MutationJourney::filter:     journeyLabel = "FILTER";      break;
    case MutationJourney::octave:     journeyLabel = "OCT";         break;
    case MutationJourney::loopVolume: journeyLabel = "LOOP+VOL";    break;
    case MutationJourney::loopPan:    journeyLabel = "LOOP+PAN";    break;
    case MutationJourney::loopFilter: journeyLabel = "LOOP+FILTER"; break;
    case MutationJourney::none:       break;
    }

    g.setFont(juce::FontOptions(9.5f));
    g.setColour(waveformColour.brighter(0.15f));

    g.drawText(
        "TAPE " + juce::String(trackNumber),
        0, 0, 54, 13,
        juce::Justification::centredLeft,
        false);

    g.setColour(
        juce::Colours::whitesmoke.withAlpha(0.82f));

    g.drawText(
        currentFileName,
        58, 0,
        juce::jmax(120, getWidth() - 360),
        13,
        juce::Justification::centredLeft,
        true);

    g.setColour(
        waveformColour.withAlpha(0.72f));

    g.drawText(
        "<" + journeyLabel + ">",
        juce::jmax(220, getWidth() - 335),
        0,
        120,
        13,
        juce::Justification::centredRight,
        false);

    // ----------------------------------------------------------
    // Main tape row

    const int loadW = 54;
    const int ejectW = 18;
    const int rightW = 210;
    const int tapeX = loadW + ejectW + 12;
    const int tapeY = 16;
    const int tapeH = 26;

    const int tapeW =
        juce::jmax(
            180,
            getWidth()
            - tapeX
            - rightW
            - 8);

    juce::Rectangle<int> tapeArea(
        tapeX,
        tapeY,
        tapeW,
        tapeH);

    g.setColour(juce::Colour(50, 54, 58));
    g.fillRoundedRectangle(
        tapeArea.toFloat(), 4.0f);

    g.setColour(juce::Colour(88, 94, 98));
    g.drawRoundedRectangle(
        tapeArea.toFloat(), 4.0f, 1.0f);
    if (tapeTouchHeld.load(
        std::memory_order_relaxed))
    {
        g.setColour(
            juce::Colours::black
            .withAlpha(0.16f));

        g.fillRoundedRectangle(
            tapeArea.toFloat(),
            4.0f);
    }
    if (waveformDisplayChannels > 0 &&
        !waveformDisplayMin[0].empty())
    {
        g.setColour(waveformColour);

        const auto waveBounds =
            tapeArea.reduced(3);

        const int channelsToDraw =
            juce::jlimit(
                1,
                2,
                waveformDisplayChannels);

        for (int channel = 0;
            channel < channelsToDraw;
            ++channel)
        {
            const float channelTop =
                static_cast<float>(
                    waveBounds.getY())
                + static_cast<float>(channel)
                * static_cast<float>(
                    waveBounds.getHeight())
                / static_cast<float>(
                    channelsToDraw);

            const float channelHeight =
                static_cast<float>(
                    waveBounds.getHeight())
                / static_cast<float>(
                    channelsToDraw);

            const float centreY =
                channelTop
                + channelHeight * 0.5f;

            const float halfHeight =
                channelHeight * 0.44f;

            juce::Path waveformPath;

            for (int bin = 0;
                bin < waveformDisplayBins;
                ++bin)
            {
                const float x =
                    static_cast<float>(
                        waveBounds.getX())
                    + static_cast<float>(bin)
                    / static_cast<float>(
                        waveformDisplayBins - 1)
                    * static_cast<float>(
                        waveBounds.getWidth());

                const float yTop =
                    centreY
                    - waveformDisplayMax[channel][bin]
                    * halfHeight;

                const float yBottom =
                    centreY
                    - waveformDisplayMin[channel][bin]
                    * halfHeight;

                waveformPath.startNewSubPath(
                    x, yTop);

                waveformPath.lineTo(
                    x, yBottom);
            }

            g.strokePath(
                waveformPath,
                juce::PathStrokeType(1.0f));
        }
    }
    else
    {
        g.setColour(
            juce::Colours::grey.withAlpha(0.65f));

        g.setFont(
            juce::FontOptions(8.5f));

        g.drawText(
            "NO TAPE LOADED",
            tapeArea,
            juce::Justification::centred,
            true);
    }

    // ----------------------------------------------------------
    // Loop brackets + target markers

    const float bracketTop =
        static_cast<float>(
            tapeArea.getY() + 3);

    const float bracketBottom =
        static_cast<float>(
            tapeArea.getBottom() - 3);

    const float loopStartX =
        tapeArea.getX()
        + static_cast<float>(
            loopStartNormalized)
        * tapeArea.getWidth();

    const float loopEndX =
        tapeArea.getX()
        + static_cast<float>(
            loopEndNormalized)
        * tapeArea.getWidth();

    const float targetLoopStartX =
        tapeArea.getX()
        + static_cast<float>(
            targetLoopStartNormalized)
        * tapeArea.getWidth();

    const float targetLoopEndX =
        tapeArea.getX()
        + static_cast<float>(
            targetLoopEndNormalized)
        * tapeArea.getWidth();

    if (loopTargetActive)
    {
        g.setColour(
            juce::Colour(235, 190, 90)
            .withAlpha(0.28f));

        g.drawLine(
            targetLoopStartX,
            bracketTop,
            targetLoopStartX,
            bracketBottom,
            1.0f);

        g.drawLine(
            targetLoopEndX,
            bracketTop,
            targetLoopEndX,
            bracketBottom,
            1.0f);
    }

    g.setColour(
        juce::Colour(235, 190, 90));

    g.drawLine(
        loopStartX,
        bracketTop,
        loopStartX,
        bracketBottom,
        2.0f);

    g.drawLine(
        loopStartX,
        bracketTop,
        loopStartX + 6.0f,
        bracketTop,
        2.0f);

    g.drawLine(
        loopEndX,
        bracketTop,
        loopEndX,
        bracketBottom,
        2.0f);

    g.drawLine(
        loopEndX - 6.0f,
        bracketTop,
        loopEndX,
        bracketTop,
        2.0f);

    // ----------------------------------------------------------
    // Tape head

    if (loadedLengthInSamples > 0)
    {
        const double normalizedPosition =
            juce::jlimit(
                0.0,
                1.0,
                playPosition
                / static_cast<double>(
                    loadedLengthInSamples));

        const float x =
            tapeArea.getX()
            + static_cast<float>(
                normalizedPosition)
            * tapeArea.getWidth();

        g.setColour(
            juce::Colour(220, 250, 250));

        g.fillRect(
            x - 0.75f,
            bracketTop,
            1.5f,
            bracketBottom - bracketTop);
    }

    // ----------------------------------------------------------
    // Six uniform activity strips:
    // LOOP / FILTER / VOL / PAN / DIR / OCT
    // No repeated labels; MainComponent owns one legend.

    const int barsX =
        getWidth() - 180;

    // Activity bank follows the tape width exactly.
    // Four continuous parameters get the useful real estate;
    // the two binary indicators (DIR/OCT) only need half-width lanes.
    juce::Rectangle<float> stripBank(
        static_cast<float>(tapeX),
        50.0f,
        static_cast<float>(tapeW),
        9.0f);

    const float gap = 7.0f;
    const float usableWidth =
        stripBank.getWidth() - gap * 5.0f;

    // 4 long lanes + 2 half lanes = 5 equal "long-lane" units.
    const float longWidth =
        usableWidth / 5.0f;

    const float shortWidth =
        longWidth * 0.5f;

    auto takeStrip = [&](float width)
        {
            auto r =
                stripBank.removeFromLeft(width);

            if (stripBank.getWidth() > 0.0f)
                stripBank.removeFromLeft(gap);

            return r;
        };

    auto loopArea = takeStrip(longWidth);
    auto filterArea = takeStrip(longWidth);
    auto volumeArea = takeStrip(longWidth);
    auto panArea = takeStrip(longWidth);
    auto reverseArea = takeStrip(shortWidth);
    auto octaveArea = stripBank;

    const bool loopMoving =
        loopTargetActive;

    const bool filterMoving =
        filterSamplesRemaining > 0;

    const bool volumeMoving =
        volumeSamplesRemaining > 0;

    const bool panMoving =
        panSamplesRemaining > 0;

    const bool reverseMoving =
        turnState != TurnState::idle;

    const bool octaveMoving =
        octaveTogglePending ||
        octaveTransitionState
        != OctaveTransitionState::idle;

    // DIR telemetry represents physical direction of travel:
    // left = reverse, right = forward.
    const float reverseNow =
        reversePlayback ? 0.0f : 1.0f;

    const float reverseTarget =
        reverseMoving
        ? (reversePlayback ? 1.0f : 0.0f)
        : reverseNow;

    const float octaveNow =
        halfSpeedActive ? 1.0f : 0.0f;

    const float octaveTarget =
        octaveMoving
        ? (halfSpeedActive ? 0.0f : 1.0f)
        : octaveNow;

    drawActivityStrip(
        g,
        loopArea,
        juce::Colour(155, 110, 220),
        static_cast<float>(
            loopStartNormalized),
        static_cast<float>(
            targetLoopStartNormalized),
        loopMoving,
        static_cast<float>(
            loopEndNormalized),
        static_cast<float>(
            targetLoopEndNormalized));

    drawActivityStrip(
        g,
        filterArea,
        juce::Colour(205, 145, 85),
        getFilterDisplayPosition(false),
        getFilterDisplayPosition(true),
        filterMoving);

    drawActivityStrip(
        g,
        volumeArea,
        juce::Colour(105, 195, 105),
        getVolumeDisplayPosition(false),
        getVolumeDisplayPosition(true),
        volumeMoving);

    drawActivityStrip(
        g,
        panArea,
        juce::Colour(65, 125, 205),
        static_cast<float>(
            (currentPan + 1.0) * 0.5),
        static_cast<float>(
            (targetPan + 1.0) * 0.5),
        panMoving);

    drawActivityStrip(
        g,
        reverseArea,
        juce::Colour(225, 75, 70),
        reverseNow,
        reverseTarget,
        reverseMoving);

    drawActivityStrip(
        g,
        octaveArea,
        juce::Colour(80, 195, 190),
        octaveNow,
        octaveTarget,
        octaveMoving);

    // Tiny bar labels - user settings, not activity.
    const char* labels[] =
    { "PROB", "TRIM", "CHO", "REV" };

    for (int i = 0; i < 4; ++i)
    {
        g.setFont(
            juce::FontOptions(7.5f));

        g.setColour(
            juce::Colours::white
            .withAlpha(0.50f));

        g.drawText(
            labels[i],
            barsX + i * 30,
            45,
            22,
            10,
            juce::Justification::centred,
            false);
    }
}


//==============================================================================
void AssTapTrack::resized()
{
    const int loadW = 54;
    const int ejectW = 18;
    const int rightW = 210;
    const int tapeX = loadW + ejectW + 12;
    const int tapeY = 16;
    const int tapeH = 26;
    const int tapeW = juce::jmax(180, getWidth() - tapeX - rightW - 8);

    loadButton.setBounds(0, tapeY, loadW, tapeH);
    unloadButton.setBounds(
        loadW + 4,
        tapeY + 4,
        ejectW,
        18);
    const int nudgeX = tapeX + tapeW + 6;
    reverseButton.setBounds(nudgeX, tapeY, 28, 18);
    loopMutateButton.setBounds(nudgeX, tapeY + 20, 28, 18);
    reverseButton.setButtonText("< >");
    loopMutateButton.setButtonText("[ ]");

    muteButton.setBounds(nudgeX + 34, tapeY, 48, tapeH);
    mutateButton.setBounds(nudgeX + 86, tapeY, 58, tapeH);

    const int barY = 54, barH = 35, barW = 22, gap = 8;
    int x = getWidth() - 180;
    probabilityKnob.setBounds(x, barY, barW, barH); x += barW + gap;
    trimKnob.setBounds(x, barY, barW, barH); x += barW + gap;
    chorusSendKnob.setBounds(x, barY, barW, barH); x += barW + gap;
    reverbSendKnob.setBounds(x, barY, barW, barH);
}



//==============================================================================
void AssTapTrack::chooseNewLoopDestination(bool automaticJourney)
{
    const juce::ScopedLock stateLock(audioStateLock);

    // Every new LOOP gets a real start, duration and exact end.
    loopJourneyStartStartNormalized = loopStartNormalized;
    loopJourneyStartEndNormalized = loopEndNormalized;
    loopJourneyElapsedSeconds = 0.0;
    loopJourneyDurationSeconds = chooseJourneySeconds();

    const double fileSeconds =
        (sourceSampleRate > 0.0)
        ? static_cast<double>(loadedLengthInSamples) / sourceSampleRate
        : 0.0;

    // Keep the old 10% floor, but never allow an automatic loop shorter
    // than 300 ms. Short files remain useful without collapsing into
    // pathological micro-loops.
    const double durationFloorNormalized =
        (fileSeconds > 0.0)
        ? minimumLoopSeconds / fileSeconds
        : 1.0;

    const double minimumLoopSize =
        juce::jlimit(
            0.10,
            0.90,
            juce::jmax(0.10, durationFloorNormalized));

    double a = random.nextDouble();
    double b = random.nextDouble();

    if (a > b)
        std::swap(a, b);

    int attempts = 0;

    while ((b - a) < minimumLoopSize &&
        attempts < 100)
    {
        a = random.nextDouble();
        b = random.nextDouble();

        if (a > b)
            std::swap(a, b);

        ++attempts;
    }

    if ((b - a) < minimumLoopSize)
    {
        a = 0.20;
        b = 0.70;
    }

    targetLoopStartNormalized = a;
    targetLoopEndNormalized = b;

    loopTargetActive = true;

    if (automaticJourney)
        currentJourney = MutationJourney::loop;

    repaint();
}

//==============================================================================
void AssTapTrack::loadAudioFile()
{
    auto chooser =
        std::make_shared<juce::FileChooser>(
            "Choose audio for Tape "
            + juce::String(trackNumber) + "...",
            juce::File{},
            "*.wav;*.aiff;*.aif;*.flac;*.mp3");

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,

        [this, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();

            if (!file.existsAsFile())
                return;

            auto reader =
                std::unique_ptr<juce::AudioFormatReader>(
                    formatManager.createReaderFor(file));

            if (reader == nullptr)
                return;

            const juce::int64 newLength =
                reader->lengthInSamples;

            const int newNumChannels =
                static_cast<int>(
                    reader->numChannels);

            const double newSourceSampleRate =
                reader->sampleRate;

            const double newDurationSeconds =
                (newSourceSampleRate > 0.0)
                ? static_cast<double>(newLength) / newSourceSampleRate
                : 0.0;

            if (newDurationSeconds < minimumSourceSeconds)
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "File too short",
                    "AssTap needs at least 200 ms of source audio.\n\n"
                    "This keeps the loop/crossfade engine out of pathological "
                    "micro-loop territory.");
                return;
            }

            if (newDurationSeconds > maximumSourceSeconds)
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "File too long",
                    "This AssTap build accepts files up to 10 minutes long.\n\n"
                    "That is already an enormous amount of source terrain.");
                return;
            }

            const juce::int64 estimatedBytes =
                newLength
                * static_cast<juce::int64>(
                    newNumChannels)
                * static_cast<juce::int64>(
                    sizeof(float));

            constexpr juce::int64 maxRamBytes =
                256LL * 1024LL * 1024LL;

            if (estimatedBytes > maxRamBytes)
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "File too large",
                    "This build currently loads each source fully into RAM.\n\n"
                    "The file is within the 10-minute time limit, but its "
                    "channel count/sample rate would exceed the current "
                    "256 MB per-track RAM ceiling.");

                return;
            }

            //------------------------------------------------------------------
            // Decode privately.
            // The currently playing tape remains untouched during disk I/O.

            juce::AudioBuffer<float> newBuffer;

            newBuffer.setSize(
                newNumChannels,
                static_cast<int>(
                    newLength));

            const bool readOK =
                reader->read(
                    &newBuffer,
                    0,
                    static_cast<int>(
                        newLength),
                    0,
                    true,
                    true);

            if (!readOK)
                return;

            //------------------------------------------------------------------
            // Conservative whole-file RMS normalization.
            //
            // We analyse both channels together and calculate one hidden gain
            // for the source. This does NOT ride the gain during playback, so
            // quiet gaps, piano decays and evolving loop dynamics remain intact.

            long double sumSquares = 0.0L;
            juce::int64 sampleCount = 0;

            for (int channel = 0;
                channel < newBuffer.getNumChannels();
                ++channel)
            {
                const float* data =
                    newBuffer.getReadPointer(channel);

                for (int sample = 0;
                    sample < newBuffer.getNumSamples();
                    ++sample)
                {
                    const long double value =
                        static_cast<long double>(data[sample]);

                    sumSquares += value * value;
                    ++sampleCount;
                }
            }

            float newNormalizationGain = 1.0f;

            if (sampleCount > 0 && sumSquares > 0.0L)
            {
                const long double meanSquare =
                    sumSquares /
                    static_cast<long double>(sampleCount);

                const double rms =
                    std::sqrt(
                        static_cast<double>(meanSquare));

                if (rms > 1.0e-9)
                {
                    const double rmsDb =
                        juce::Decibels::gainToDecibels(
                            static_cast<float>(rms),
                            -120.0f);

                    double correctionDb =
                        normalizationTargetDb - rmsDb;

                    correctionDb =
                        juce::jlimit(
                            normalizationMaxCutDb,
                            normalizationMaxBoostDb,
                            correctionDb);

                    newNormalizationGain =
                        juce::Decibels::decibelsToGain(
                            static_cast<float>(correctionDb));
                }
            }

            //------------------------------------------------------------------
            // Build a display-only waveform. Quiet material is visually
            // magnified, but the audio itself is never altered by this step.

            std::array<std::vector<float>, 2> newWaveformMin;
            std::array<std::vector<float>, 2> newWaveformMax;

            const int visualChannels =
                juce::jlimit(1, 2, newBuffer.getNumChannels());

            float visualPeak = 0.0f;

            for (int channel = 0; channel < visualChannels; ++channel)
            {
                newWaveformMin[channel].resize(waveformDisplayBins, 0.0f);
                newWaveformMax[channel].resize(waveformDisplayBins, 0.0f);

                for (int bin = 0; bin < waveformDisplayBins; ++bin)
                {
                    const int startSample = static_cast<int>(
                        (static_cast<juce::int64>(bin)
                            * newBuffer.getNumSamples())
                        / waveformDisplayBins);

                    const int endSample = static_cast<int>(
                        (static_cast<juce::int64>(bin + 1)
                            * newBuffer.getNumSamples())
                        / waveformDisplayBins);

                    const int count = juce::jmax(1, endSample - startSample);

                    const auto range = newBuffer.findMinMax(
                        channel,
                        startSample,
                        juce::jmin(count, newBuffer.getNumSamples() - startSample));

                    newWaveformMin[channel][bin] = range.getStart();
                    newWaveformMax[channel][bin] = range.getEnd();

                    visualPeak = juce::jmax(
                        visualPeak,
                        std::abs(range.getStart()),
                        std::abs(range.getEnd()));
                }
            }

            const float waveformDisplayGain =
                visualPeak > 1.0e-7f
                ? juce::jmin(100.0f, 0.88f / visualPeak)
                : 1.0f;

            for (int channel = 0; channel < visualChannels; ++channel)
            {
                for (int bin = 0; bin < waveformDisplayBins; ++bin)
                {
                    newWaveformMin[channel][bin] = juce::jlimit(
                        -1.0f, 1.0f,
                        newWaveformMin[channel][bin] * waveformDisplayGain);

                    newWaveformMax[channel][bin] = juce::jlimit(
                        -1.0f, 1.0f,
                        newWaveformMax[channel][bin] * waveformDisplayGain);
                }
            }

            //------------------------------------------------------------------
            // Commit finished tape in one protected operation.

            {
                const juce::ScopedLock stateLock(
                    audioStateLock);

                audioBuffer =
                    std::move(newBuffer);

                sourceSampleRate =
                    newSourceSampleRate;

                sourceNormalizationGain =
                    newNormalizationGain;

                loadedLengthInSamples =
                    newLength;

                loadedNumChannels =
                    newNumChannels;

                waveformDisplayMin = std::move(newWaveformMin);
                waveformDisplayMax = std::move(newWaveformMax);
                waveformDisplayChannels = visualChannels;

                loopStartNormalized = 0.20;
                loopEndNormalized = 0.70;

                targetLoopStartNormalized =
                    loopStartNormalized;

                targetLoopEndNormalized =
                    loopEndNormalized;

                loopTargetActive = false;

                playPosition =
                    loopStartNormalized
                    * static_cast<double>(
                        loadedLengthInSamples);

                incomingPosition =
                    playPosition;

                reversePlayback = false;

                crossfadeActive = false;
                crossfadeSamplesDone = 0;

                turnState =
                    TurnState::idle;

                turnSamplesDone = 0;

                manualReverseRequested = false;
                boundaryDecisionMade = false;

                halfSpeedActive = false;
                octaveTogglePending = false;
                octaveTransitionActive = false;
                octaveTransitionState =
                    OctaveTransitionState::idle;
                octaveTransitionSamplesDone = 0;

                currentJourney =
                    MutationJourney::none;

                currentMutationVolumeDb = 0.0;
                targetMutationVolumeDb = 0.0;
                volumeSamplesRemaining = 0;

                currentPan = 0.0;
                targetPan = 0.0;
                panSamplesRemaining = 0;

                currentHpHz = targetHpHz = 20.0;

                const double safeTop =
                    juce::jmax(
                        2000.0,
                        deviceSampleRate * 0.45);

                currentLpHz = targetLpHz =
                    juce::jmin(
                        20000.0,
                        safeTop);

                currentPeakHz =
                    targetPeakHz = 1000.0;

                currentPeakGainDb =
                    targetPeakGainDb = 0.0;

                currentPeakQ =
                    targetPeakQ = 0.90;

                filterSamplesRemaining = 0;

                hpFilterL.reset();
                hpFilterR.reset();
                lpFilterL.reset();
                lpFilterR.reset();
                peakFilterL.reset();
                peakFilterR.reset();

                updateFilterCoefficients();
            }

            //------------------------------------------------------------------
            // GUI-only state after the audio swap.

            thumbnail.setSource(
                new juce::FileInputSource(file));

            currentFileName =
                file.getFileName();

            repaint();
        });
}

//==============================================================================
void AssTapTrack::timerUpdate()
{
    const juce::ScopedLock stateLock(audioStateLock);
    // Finish an EJECT only after its temporary fade has reached silence.
    if (unloadPending &&
        muteSamplesRemaining <= 0 &&
        currentMuteGain <= 0.001f)
    {
        audioBuffer.setSize(0, 0);

        loadedLengthInSamples = 0;
        loadedNumChannels = 0;

        waveformDisplayMin[0].clear();
        waveformDisplayMin[1].clear();
        waveformDisplayMax[0].clear();
        waveformDisplayMax[1].clear();
        waveformDisplayChannels = 0;

        thumbnail.clear();

        currentFileName = "No Tape Loaded";

        playPosition = 0.0;
        incomingPosition = 0.0;

        loopStartNormalized = 0.20;
        loopEndNormalized = 0.70;
        targetLoopStartNormalized = 0.20;
        targetLoopEndNormalized = 0.70;
        loopTargetActive = false;

        reversePlayback = false;
        crossfadeActive = false;
        crossfadeSamplesDone = 0;

        turnState = TurnState::idle;
        turnSamplesDone = 0;
        manualReverseRequested = false;

        halfSpeedActive = false;
        octaveTogglePending = false;
        octaveTransitionActive = false;
        octaveTransitionState = OctaveTransitionState::idle;
        octaveTransitionSamplesDone = 0;

        currentJourney = MutationJourney::none;

        volumeSamplesRemaining = 0;
        panSamplesRemaining = 0;
        filterSamplesRemaining = 0;

        boundaryDecisionMade = false;

        // Restore the user's MUTE state.
        targetMuteGain = muteTargetBeforeUnload;
        currentMuteGain = muteTargetBeforeUnload;
        muteSamplesRemaining = 0;

        unloadPending = false;
    }
    if (loopTargetActive &&
        !crossfadeActive &&
        turnState == TurnState::idle)
    {
        loopJourneyElapsedSeconds += 1.0 / 30.0;

        const double progress =
            juce::jlimit(
                0.0,
                1.0,
                loopJourneyElapsedSeconds /
                juce::jmax(0.001, loopJourneyDurationSeconds));

        // Smoothstep: gentle start and finish, but exact arrival.
        const double eased =
            progress * progress * (3.0 - 2.0 * progress);

        loopStartNormalized =
            loopJourneyStartStartNormalized
            + (targetLoopStartNormalized - loopJourneyStartStartNormalized)
            * eased;

        loopEndNormalized =
            loopJourneyStartEndNormalized
            + (targetLoopEndNormalized - loopJourneyStartEndNormalized)
            * eased;

        if (progress >= 1.0)
        {
            loopStartNormalized = targetLoopStartNormalized;
            loopEndNormalized = targetLoopEndNormalized;
            loopTargetActive = false;

            if (currentJourney == MutationJourney::loop)
                currentJourney = MutationJourney::none;
            else if (currentJourney == MutationJourney::loopVolume &&
                volumeSamplesRemaining <= 0)
                currentJourney = MutationJourney::none;
            else if (currentJourney == MutationJourney::loopPan &&
                panSamplesRemaining <= 0)
                currentJourney = MutationJourney::none;
            else if (currentJourney == MutationJourney::loopFilter &&
                filterSamplesRemaining <= 0)
                currentJourney = MutationJourney::none;
        }
    }
    const bool muteTarget =
        targetMuteGain < 0.5f;

    const bool muteTransition =
        muteSamplesRemaining > 0;

    const bool muteBlink =
        ((juce::Time::getMillisecondCounter() / 350) % 2) == 0;

    muteButton.getProperties().set(
        "muteTarget",
        muteTarget);

    muteButton.getProperties().set(
        "muteTransition",
        muteTransition);

    muteButton.getProperties().set(
        "muteBlink",
        muteBlink);

    muteButton.repaint();
    repaint();
}