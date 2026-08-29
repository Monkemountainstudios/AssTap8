#include "MainComponent.h"
#include "KnobImageData.h"
#include "MetalImageData.h"
#include "SonicOnionImageData.h"

namespace
{
    class AssTapTimeKnobLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        AssTapTimeKnobLookAndFeel()
        {
            knobImage = juce::ImageFileFormat::loadFrom(
                AssTapKnobImageData::knobPng,
                AssTapKnobImageData::knobPngSize);
        }

        void drawRotarySlider(juce::Graphics& g,
            int x, int y, int width, int height,
            float sliderPosProportional,
            float, float,
            juce::Slider&) override
        {
            if (!knobImage.isValid())
                return;

            auto area = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(3.0f);
            const float side = juce::jmin(area.getWidth(), area.getHeight());
            auto knobArea = juce::Rectangle<float>(
                area.getCentreX() - side * 0.5f,
                area.getCentreY() - side * 0.5f,
                side, side);

            g.setColour(juce::Colours::black.withAlpha(0.30f));
            g.fillEllipse(knobArea.translated(2.0f, 3.0f).reduced(2.0f));

            const float angle = juce::jmap(sliderPosProportional, -2.35619449f, 2.35619449f);

            juce::Graphics::ScopedSaveState save(g);
            g.addTransform(juce::AffineTransform::rotation(
                angle, knobArea.getCentreX(), knobArea.getCentreY()));
            g.drawImage(knobImage, knobArea, juce::RectanglePlacement::centred, false);
        }

    private:
        juce::Image knobImage;
    };
    class AssTapRecordButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getTextButtonFont(
            juce::TextButton&,
            int buttonHeight) override
        {
            juce::ignoreUnused(buttonHeight);

            return juce::Font(
                juce::FontOptions(22.0f)
                .withStyle("Bold"));
        }
    };
}


//==============================================================================
MainComponent::MainComponent()
{
    setSize(1000, 900);

    metalBackground = juce::ImageFileFormat::loadFrom(
        AssTapMetalImageData::metalPng,
        AssTapMetalImageData::metalPngSize);

    sonicOnionLogo = juce::ImageFileFormat::loadFrom(
        SonicOnionImageData::logoPng,
        SonicOnionImageData::logoPngSize);

    addAndMakeVisible(track1);
    addAndMakeVisible(track2);
    addAndMakeVisible(track3);
    addAndMakeVisible(track4);
    addAndMakeVisible(track5);
    addAndMakeVisible(track6);
    addAndMakeVisible(track7);
    addAndMakeVisible(track8);

    addAndMakeVisible(recordButton);
    recordButtonLookAndFeel =
        std::make_unique<AssTapRecordButtonLookAndFeel>();

    recordButton.setLookAndFeel(
        recordButtonLookAndFeel.get());

    recordButton.setButtonText(
        juce::String::fromUTF8("\xE2\x97\x8F"));

    recordButton.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(105, 30, 30));

    recordButton.setColour(
        juce::TextButton::buttonOnColourId,
        juce::Colour(175, 45, 40));

    recordButton.setColour(
        juce::TextButton::textColourOffId,
        juce::Colour(245, 105, 95));

    recordButton.setColour(
        juce::TextButton::textColourOnId,
        juce::Colours::white);

    recordButton.onClick =
        [this] { toggleRecording(); };
    recordingThread.startThread();

    //--------------------------------------------------------------------------
    // Global TIME
    //
    // This does not synchronize the tracks. It only shifts the range from
    // which each track independently chooses its next journey duration.

    addAndMakeVisible(timeKnob);

    timeKnobLookAndFeel =
        std::make_unique<AssTapTimeKnobLookAndFeel>();

    timeKnob.setLookAndFeel(
        timeKnobLookAndFeel.get());

    timeKnob.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);

    timeKnob.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        62,
        16);

    timeKnob.setRange(0.0, 100.0, 1.0);
    timeKnob.setValue(0.0);
    timeKnob.setTextValueSuffix(" %");

    // Stereo output, no input.
    setAudioChannels(0, 2);

    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    recordButton.setLookAndFeel(nullptr);
    timeKnob.setLookAndFeel(nullptr);

    stopRecording();
    shutdownAudio();
    recordingThread.stopThread(1500);
}
//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected,
    double sampleRate)
{
    track1.prepareToPlay(
        samplesPerBlockExpected,
        sampleRate);

    track2.prepareToPlay(
        samplesPerBlockExpected,
        sampleRate);

    track3.prepareToPlay(
        samplesPerBlockExpected,
        sampleRate);

    track4.prepareToPlay(
        samplesPerBlockExpected,
        sampleRate);

    track5.prepareToPlay(samplesPerBlockExpected, sampleRate);
    track6.prepareToPlay(samplesPerBlockExpected, sampleRate);
    track7.prepareToPlay(samplesPerBlockExpected, sampleRate);
    track8.prepareToPlay(samplesPerBlockExpected, sampleRate);

    masterSampleRate = sampleRate;

    trackBuffer1.setSize(
        2,
        samplesPerBlockExpected);

    trackBuffer2.setSize(
        2,
        samplesPerBlockExpected);

    trackBuffer3.setSize(
        2,
        samplesPerBlockExpected);

    trackBuffer4.setSize(
        2,
        samplesPerBlockExpected);

    trackBuffer5.setSize(2, samplesPerBlockExpected);
    trackBuffer6.setSize(2, samplesPerBlockExpected);
    trackBuffer7.setSize(2, samplesPerBlockExpected);
    trackBuffer8.setSize(2, samplesPerBlockExpected);

    //---------- Shared warm chorus
    juce::dsp::ProcessSpec chorusSpec;
    chorusSpec.sampleRate = sampleRate;
    chorusSpec.maximumBlockSize =
        static_cast<juce::uint32>(samplesPerBlockExpected);
    chorusSpec.numChannels = 2;

    sharedChorus.prepare(chorusSpec);
    sharedChorus.reset();

    // Slow, warm, modest modulation: Chorus-I-ish rather than seasick.
    sharedChorus.setRate(0.45f);
    sharedChorus.setDepth(0.24f);
    sharedChorus.setCentreDelay(8.0f);
    sharedChorus.setFeedback(0.04f);
    sharedChorus.setMix(1.0f); // wet-only send bus

    chorusBuffer.setSize(
        2,
        samplesPerBlockExpected);
    //----------Reverb
    sharedReverb.setSampleRate(sampleRate);
    sharedReverb.reset();

    juce::Reverb::Parameters params;

    params.roomSize = 0.88f;
    params.damping = 0.28f;
    params.wetLevel = 1.0f;
    params.dryLevel = 0.0f;
    params.width = 1.0f;
    params.freezeMode = 0.0f;

    sharedReverb.setParameters(params);
    reverbBuffer.setSize(
        2,
        samplesPerBlockExpected);
}
//==============================================================================
void MainComponent::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::ScopedNoDenormals noDenormals;

    auto& output = *bufferToFill.buffer;
    const int numSamples = bufferToFill.numSamples;

    // Make sure track buffers are large enough.
    if (trackBuffer1.getNumSamples() < numSamples)
        trackBuffer1.setSize(2, numSamples, false, false, true);

    if (trackBuffer2.getNumSamples() < numSamples)
        trackBuffer2.setSize(2, numSamples, false, false, true);

    if (trackBuffer3.getNumSamples() < numSamples)
        trackBuffer3.setSize(2, numSamples, false, false, true);

    if (trackBuffer4.getNumSamples() < numSamples)
        trackBuffer4.setSize(2, numSamples, false, false, true);
    if (trackBuffer5.getNumSamples() < numSamples)
        trackBuffer5.setSize(2, numSamples, false, false, true);
    if (trackBuffer6.getNumSamples() < numSamples)
        trackBuffer6.setSize(2, numSamples, false, false, true);
    if (trackBuffer7.getNumSamples() < numSamples)
        trackBuffer7.setSize(2, numSamples, false, false, true);
    if (trackBuffer8.getNumSamples() < numSamples)
        trackBuffer8.setSize(2, numSamples, false, false, true);

    if (chorusBuffer.getNumSamples() < numSamples)
        chorusBuffer.setSize(2, numSamples, false, false, true);

    if (reverbBuffer.getNumSamples() < numSamples)
        reverbBuffer.setSize(2, numSamples, false, false, true);

    trackBuffer1.clear();
    trackBuffer2.clear();
    trackBuffer3.clear();
    trackBuffer4.clear();
    trackBuffer5.clear();
    trackBuffer6.clear();
    trackBuffer7.clear();
    trackBuffer8.clear();
    chorusBuffer.clear();
    reverbBuffer.clear();

    // Generate eight fully independent AssTap tracks.
    track1.processBlock(trackBuffer1, numSamples);
    track2.processBlock(trackBuffer2, numSamples);
    track3.processBlock(trackBuffer3, numSamples);
    track4.processBlock(trackBuffer4, numSamples);
    track5.processBlock(trackBuffer5, numSamples);
    track6.processBlock(trackBuffer6, numSamples);
    track7.processBlock(trackBuffer7, numSamples);
    track8.processBlock(trackBuffer8, numSamples);

    // ----------------------------------------------------------
    // Build shared CHORUS bus from both independent track sends.

    const float chorusSend1 =
        track1.getChorusSend();

    const float chorusSend2 =
        track2.getChorusSend();

    const float chorusSend3 =
        track3.getChorusSend();

    const float chorusSend4 =
        track4.getChorusSend();
    const float chorusSend5 = track5.getChorusSend();
    const float chorusSend6 = track6.getChorusSend();
    const float chorusSend7 = track7.getChorusSend();
    const float chorusSend8 = track8.getChorusSend();

    for (int channel = 0; channel < 2; ++channel)
    {
        chorusBuffer.addFrom(
            channel, 0,
            trackBuffer1, channel, 0,
            numSamples,
            chorusSend1);

        chorusBuffer.addFrom(
            channel, 0,
            trackBuffer2, channel, 0,
            numSamples,
            chorusSend2);

        chorusBuffer.addFrom(
            channel, 0,
            trackBuffer3, channel, 0,
            numSamples,
            chorusSend3);

        chorusBuffer.addFrom(
            channel, 0,
            trackBuffer4, channel, 0,
            numSamples,
            chorusSend4);
        chorusBuffer.addFrom(channel, 0, trackBuffer5, channel, 0, numSamples, chorusSend5);
        chorusBuffer.addFrom(channel, 0, trackBuffer6, channel, 0, numSamples, chorusSend6);
        chorusBuffer.addFrom(channel, 0, trackBuffer7, channel, 0, numSamples, chorusSend7);
        chorusBuffer.addFrom(channel, 0, trackBuffer8, channel, 0, numSamples, chorusSend8);
    }

    auto chorusBlock =
        juce::dsp::AudioBlock<float>(chorusBuffer)
        .getSubBlock(
            0,
            static_cast<size_t>(numSamples));

    juce::dsp::ProcessContextReplacing<float>
        chorusContext(chorusBlock);

    sharedChorus.process(chorusContext);

    // Keep the shared FX return from poisoning the main mix if something
    // ever produces a non-finite value.
    bool chorusBecameInvalid = false;

    for (int channel = 0; channel < 2; ++channel)
    {
        auto* data =
            chorusBuffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (!std::isfinite(data[sample]))
            {
                data[sample] = 0.0f;
                chorusBecameInvalid = true;
            }

            data[sample] =
                juce::jlimit(-4.0f, 4.0f, data[sample]);
        }
    }

    if (chorusBecameInvalid)
    {
        sharedChorus.reset();
        chorusBuffer.clear();
    }

    // ----------------------------------------------------------
   // ----------------------------------------------------------
// ----------------------------------------------------------
// Build shared reverb input from BOTH tracks.

    reverbBuffer.clear();

    const float send1 = track1.getReverbSend();
    const float send2 = track2.getReverbSend();
    const float send3 = track3.getReverbSend();
    const float send4 = track4.getReverbSend();
    const float send5 = track5.getReverbSend();
    const float send6 = track6.getReverbSend();
    const float send7 = track7.getReverbSend();
    const float send8 = track8.getReverbSend();

    for (int channel = 0; channel < 2; ++channel)
    {
        reverbBuffer.addFrom(
            channel,
            0,
            trackBuffer1,
            channel,
            0,
            numSamples,
            send1);

        reverbBuffer.addFrom(
            channel,
            0,
            trackBuffer2,
            channel,
            0,
            numSamples,
            send2);

        reverbBuffer.addFrom(
            channel,
            0,
            trackBuffer3,
            channel,
            0,
            numSamples,
            send3);

        reverbBuffer.addFrom(
            channel,
            0,
            trackBuffer4,
            channel,
            0,
            numSamples,
            send4);
        reverbBuffer.addFrom(channel, 0, trackBuffer5, channel, 0, numSamples, send5);
        reverbBuffer.addFrom(channel, 0, trackBuffer6, channel, 0, numSamples, send6);
        reverbBuffer.addFrom(channel, 0, trackBuffer7, channel, 0, numSamples, send7);
        reverbBuffer.addFrom(channel, 0, trackBuffer8, channel, 0, numSamples, send8);
    }

    sharedReverb.processStereo(
        reverbBuffer.getWritePointer(0, 0),
        reverbBuffer.getWritePointer(1, 0),
        numSamples);

    // ----------------------------------------------------------
    // Safety fence.
    // Keep bad floating-point values out of the final mix.

    bool reverbBecameInvalid = false;

    for (int channel = 0; channel < 2; ++channel)
    {
        auto* data = reverbBuffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (!std::isfinite(data[sample]))
            {
                data[sample] = 0.0f;
                reverbBecameInvalid = true;
            }

            data[sample] =
                juce::jlimit(-4.0f, 4.0f, data[sample]);
        }
    }

    // Repair the internal feedback state too, not only its output.
    if (reverbBecameInvalid)
    {
        sharedReverb.reset();
        reverbBuffer.clear();
    }
    // ----------------------------------------------------------
    // Dry+wet output ONLY.

    bufferToFill.clearActiveBufferRegion();

    for (int channel = 0;
        channel < output.getNumChannels();
        ++channel)
    {
        const int sourceChannel =
            juce::jmin(channel, 1);

        output.addFrom(
            channel,
            bufferToFill.startSample,
            trackBuffer1,
            sourceChannel,
            0,
            numSamples);

        output.addFrom(
            channel,
            bufferToFill.startSample,
            trackBuffer2,
            sourceChannel,
            0,
            numSamples);

        output.addFrom(
            channel,
            bufferToFill.startSample,
            trackBuffer3,
            sourceChannel,
            0,
            numSamples);

        output.addFrom(
            channel,
            bufferToFill.startSample,
            trackBuffer4,
            sourceChannel,
            0,
            numSamples);
        output.addFrom(channel, bufferToFill.startSample, trackBuffer5, sourceChannel, 0, numSamples);
        output.addFrom(channel, bufferToFill.startSample, trackBuffer6, sourceChannel, 0, numSamples);
        output.addFrom(channel, bufferToFill.startSample, trackBuffer7, sourceChannel, 0, numSamples);
        output.addFrom(channel, bufferToFill.startSample, trackBuffer8, sourceChannel, 0, numSamples);

        // Shared chorus return. The send knob provides the main amount;
        // this fixed return keeps full send lush rather than cartoonish.
        output.addFrom(
            channel,
            bufferToFill.startSample,
            chorusBuffer,
            sourceChannel,
            0,
            numSamples,
            0.55f);

        output.addFrom(
            channel,
            bufferToFill.startSample,
            reverbBuffer,
            sourceChannel,
            0,
            numSamples,
            0.70f);
    }

    if (output.getNumChannels() > 0 && numSamples > 0)
    {
        masterRmsL.store(output.getRMSLevel(0, bufferToFill.startSample, numSamples), std::memory_order_relaxed);
        masterRmsR.store(output.getRMSLevel(juce::jmin(1, output.getNumChannels() - 1), bufferToFill.startSample, numSamples), std::memory_order_relaxed);
    }

    if (auto* writer = activeWriter.load(std::memory_order_acquire))
    {
        const float* channels[2] =
        {
            output.getReadPointer(0, bufferToFill.startSample),
            output.getReadPointer(juce::jmin(1, output.getNumChannels() - 1), bufferToFill.startSample)
        };

        writer->write(channels, numSamples);
    }
}
//==============================================================================
void MainComponent::releaseResources()
{
    track1.releaseResources();
    track2.releaseResources();
    track3.releaseResources();
    track4.releaseResources();
    track5.releaseResources();
    track6.releaseResources();
    track7.releaseResources();
    track8.releaseResources();

    sharedChorus.reset();
    sharedReverb.reset();
    trackBuffer1.setSize(0, 0);
    trackBuffer2.setSize(0, 0);
    trackBuffer3.setSize(0, 0);
    trackBuffer4.setSize(0, 0);
    trackBuffer5.setSize(0, 0);
    trackBuffer6.setSize(0, 0);
    trackBuffer7.setSize(0, 0);
    trackBuffer8.setSize(0, 0);
    chorusBuffer.setSize(0, 0);
    reverbBuffer.setSize(0, 0);
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    if (metalBackground.isValid())
        g.drawImage(metalBackground, getLocalBounds().toFloat(),
            juce::RectanglePlacement::fillDestination);
    else
        g.fillAll(juce::Colour(24, 26, 28));

    g.setColour(juce::Colours::black.withAlpha(0.08f));
    g.fillRect(getLocalBounds());

    g.setColour(juce::Colour(202, 205, 202));
    g.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
    g.drawText("AssTap-8 //", 22, 12, 240, 24, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText("Asynchronous", 22, 38, 240, 19, juce::Justification::centredLeft);
    g.drawText("Tape Machine", 22, 57, 240, 19, juce::Justification::centredLeft);

    auto drawVu = [&](juce::Rectangle<float> r, float level, const juce::String& lr)
        {
            g.setColour(juce::Colours::black.withAlpha(0.72f));
            g.fillRoundedRectangle(r.expanded(4.0f), 4.0f);
            auto face = r.reduced(3.0f);

            juce::ColourGradient grad(juce::Colour(173, 121, 63), face.getCentreX(), face.getY(),
                juce::Colour(249, 205, 112), face.getCentreX(), face.getBottom(), false);
            grad.addColour(0.72, juce::Colour(226, 168, 82));
            g.setGradientFill(grad);
            g.fillRoundedRectangle(face, 2.0f);

            const float cx = face.getCentreX(), cy = face.getBottom() + 14.0f;
            const float rad = face.getWidth() * 0.43f;
            const float a0 = -2.66f, a1 = -0.48f;
            g.setColour(juce::Colour(54, 38, 24).withAlpha(0.9f));

            for (int i = 0; i <= 14; ++i)
            {
                float t = (float)i / 14.0f;
                float a = juce::jmap(t, a0, a1);

                bool major = (i % 2) == 0;
                float inner = rad - (major ? 6.0f : 3.5f);

                // Final part of the scale = UhOh zone.
                if (i >= 11)
                    g.setColour(juce::Colour(175, 45, 35));
                else
                    g.setColour(juce::Colour(54, 38, 24).withAlpha(0.9f));

                g.drawLine(
                    cx + std::cos(a) * rad,
                    cy + std::sin(a) * rad,
                    cx + std::cos(a) * inner,
                    cy + std::sin(a) * inner,
                    major ? 1.0f : 0.7f);
            }  
            

            const float db = juce::Decibels::gainToDecibels(juce::jmax(level, 0.000001f), -48.0f);
            const float n = juce::jlimit(0.0f, 1.0f, (db + 36.0f) / 39.0f);
            const float a = juce::jmap(n, a0, a1);
            const float nx = cx + std::cos(a) * (rad - 2.0f), ny = cy + std::sin(a) * (rad - 2.0f);

            {
                juce::Graphics::ScopedSaveState needleClip(g);

                // The movement pivot lives below the visible meter face,
                // but the needle itself may only appear inside the glass.
                g.reduceClipRegion(face.toNearestInt());

                g.setColour(juce::Colour(70, 31, 22));
                g.drawLine(cx, cy, nx, ny, 1.25f);

                const float rx =
                    cx + (nx - cx) * 0.78f;

                const float ry =
                    cy + (ny - cy) * 0.78f;

                g.setColour(juce::Colour(165, 48, 35));
                g.drawLine(rx, ry, nx, ny, 1.35f);
            }

            g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
            g.setColour(juce::Colour(50, 31, 20));
            g.drawText("VU", face.toNearestInt(), juce::Justification::centredBottom, false);
            g.setFont(juce::FontOptions(7.0f));
            g.drawText(lr, (int)face.getX() + 4, (int)face.getY() + 3, 10, 9,
                juce::Justification::centred, false);
        };

    const float mw = 108.0f, mh = 52.0f, mg = 10.0f;
    const float total = mw * 2.0f + mg;
    const float mx = getWidth() * 0.5f - total * 0.5f;
    drawVu({ mx,13.0f,mw,mh }, vuDisplayL, "L");
    drawVu({ mx + mw + mg,13.0f,mw,mh }, vuDisplayR, "R");

    const struct { const char* name; juce::Colour colour; } legend[] = {
        {"LOOP",juce::Colour(155,110,220)},
        {"FILTER",juce::Colour(205,145,85)},
        {"VOL",juce::Colour(105,195,105)},
        {"PAN",juce::Colour(65,125,205)},
        {"DIR",juce::Colour(225,75,70)},
        {"OCT",juce::Colour(80,195,190)}
    };

    // Vertical hardware-style legend on the extreme right chassis edge.
    // Drawn in a rotated graphics context so the top panel stays uncluttered.
    {
        juce::Graphics::ScopedSaveState save(g);

        const float pivotX =
            static_cast<float>(getWidth()) - 11.0f;

        const float pivotY =
            static_cast<float>(getHeight()) * 0.50f;

        g.addTransform(
            juce::AffineTransform::rotation(
                -juce::MathConstants<float>::halfPi,
                pivotX,
                pivotY));

        const float totalLegendW = 6.0f * 55.0f;
        float lx = pivotX - totalLegendW * 0.5f;

        const float ly = pivotY - 8.0f;

        for (const auto& item : legend)
        {
            g.setColour(item.colour.withAlpha(0.90f));

            g.fillRoundedRectangle(
                lx,
                ly + 6.0f,
                13.0f,
                2.5f,
                1.0f);

            g.setFont(
                juce::FontOptions(7.5f));

            g.drawText(
                item.name,
                static_cast<int>(lx + 17.0f),
                static_cast<int>(ly),
                34,
                14,
                juce::Justification::centredLeft,
                false);

            lx += 55.0f;
        }
    }

    g.setColour(juce::Colour(202, 205, 202));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("TIME", getWidth() - 202, 7, 88, 14, juce::Justification::centred);

    if (activeWriter.load(std::memory_order_acquire) != nullptr)
    {
        const auto elapsed = juce::Time::getCurrentTime() - recordingStartTime;
        const int secs = (int)elapsed.inSeconds();
        g.setColour(juce::Colour(235, 95, 85));
        g.setFont(juce::FontOptions(8.0f));
        g.drawText(juce::String::formatted("%02d:%02d", secs / 60, secs % 60),
            getWidth() - 80, 75, 54, 12, juce::Justification::centred, false);
    }

    if (sonicOnionLogo.isValid())
    {
        const int logoSize = 60;
        const int margin = 14;

        const int x =
            getWidth() - logoSize - margin;

        const int y =
            getHeight() - logoSize - margin;

        g.setOpacity(0.15f);

        g.drawImageWithin(
            sonicOnionLogo,
            x, y,
            logoSize, logoSize,
            juce::RectanglePlacement::centred,
            false);

        g.setOpacity(1.0f);
    }
}


//==============================================================================
void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    const int legendGutter = 32;
    bounds.removeFromRight(legendGutter);
    const int topPanelHeight = juce::jlimit(
        94, 126, (int)(getHeight() * 0.115f));
    bounds.removeFromTop(topPanelHeight);

    timeKnob.setBounds(getWidth() - 204, 18, 94, 80);
    recordButton.setBounds(getWidth() - 80, 29, 52, 42);

    const int gap = 2;
    const int available = juce::jmax(0, bounds.getHeight() - gap * 7);
    const int trackHeight = juce::jlimit(82, 104, available / 8);

    track1.setBounds(bounds.removeFromTop(trackHeight)); bounds.removeFromTop(gap);
    track2.setBounds(bounds.removeFromTop(trackHeight)); bounds.removeFromTop(gap);
    track3.setBounds(bounds.removeFromTop(trackHeight)); bounds.removeFromTop(gap);
    track4.setBounds(bounds.removeFromTop(trackHeight)); bounds.removeFromTop(gap);
    track5.setBounds(bounds.removeFromTop(trackHeight)); bounds.removeFromTop(gap);
    track6.setBounds(bounds.removeFromTop(trackHeight)); bounds.removeFromTop(gap);
    track7.setBounds(bounds.removeFromTop(trackHeight)); bounds.removeFromTop(gap);
    track8.setBounds(bounds.removeFromTop(trackHeight));
}


//==============================================================================
void MainComponent::timerCallback()
{
    const double time =
        timeKnob.getValue() / 100.0;

    track1.setTimeTemperament(time);
    track2.setTimeTemperament(time);
    track3.setTimeTemperament(time);
    track4.setTimeTemperament(time);
    track5.setTimeTemperament(time);
    track6.setTimeTemperament(time);
    track7.setTimeTemperament(time);
    track8.setTimeTemperament(time);

    track1.timerUpdate();
    track2.timerUpdate();
    track3.timerUpdate();
    track4.timerUpdate();
    track5.timerUpdate();
    track6.timerUpdate();
    track7.timerUpdate();
    track8.timerUpdate();

    const float ml = masterRmsL.load(std::memory_order_relaxed);
    const float mr = masterRmsR.load(std::memory_order_relaxed);
    auto ballistic = [](float now, float target)
        {
            const float k = target > now ? 0.24f : 0.055f;
            return now + (target - now) * k;
        };
    vuDisplayL = ballistic(vuDisplayL, ml);
    vuDisplayR = ballistic(vuDisplayR, mr);

    repaint();
}

//==============================================================================
void MainComponent::toggleRecording()
{
    if (activeWriter.load(std::memory_order_acquire) != nullptr)
        stopRecording();
    else
        startRecording();
}

//==============================================================================
void MainComponent::startRecording()
{
    if (activeWriter.load(std::memory_order_acquire) != nullptr)
        return;

    auto recordingsFolder =
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("AssTap Recordings");

    if (!recordingsFolder.exists())
        recordingsFolder.createDirectory();

    const auto now = juce::Time::getCurrentTime();
    const juce::String timestamp = now.formatted("%Y-%m-%d_%H-%M-%S");

    currentRecordingFile = recordingsFolder.getNonexistentChildFile(
        "ASSTAP_" + timestamp, ".wav", false);

    auto fileStream = currentRecordingFile.createOutputStream();

    if (fileStream == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Recording failed",
            "AssTap could not create the recording file.");
        return;
    }

    juce::WavAudioFormat wavFormat;
    auto* writer = wavFormat.createWriterFor(
        fileStream.release(), masterSampleRate, 2, 24, {}, 0);

    if (writer == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Recording failed",
            "AssTap could not create a WAV writer.");
        return;
    }

    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, recordingThread, 32768);

    recordingStartTime = now;
    activeWriter.store(threadedWriter.get(), std::memory_order_release);
    recordButton.setToggleState(
        true,
        juce::dontSendNotification);

    recordButton.setButtonText(
        juce::String::fromUTF8("\xE2\x96\xA0"));
    repaint();
}

//==============================================================================
void MainComponent::stopRecording()
{
    activeWriter.store(nullptr, std::memory_order_release);
    threadedWriter.reset();
    recordButton.setToggleState(
        false,
        juce::dontSendNotification);

    recordButton.setButtonText(
        juce::String::fromUTF8("\xE2\x97\x8F"));
    repaint();
}