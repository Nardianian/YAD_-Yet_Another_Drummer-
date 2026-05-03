#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

//==============================================================================

struct ExtraArticulation {
	int midiNote;
	juce::String label;
	std::unique_ptr<juce::AudioSampleBuffer> buffers[3]; // 3 Layer
	juce::File sampleFiles[3];
	float gain = 1.0f;
};

//==============================================================================

class DrumSimulatorAudioProcessor : public juce::AudioProcessor,
	public juce::ValueTree::Listener
{
public:
	enum DrumSounds
	{
		KICK = 0,
		SNARE,
		HIHAT,
		CRASH,
		TOM1,
		TOM2,
		TOM3,
		RIDE,
		NUM_SOUNDS
	};
	//==============================================================================
	DrumSimulatorAudioProcessor();
	~DrumSimulatorAudioProcessor() override;
	juce::AudioProcessorValueTreeState parameters;

	//==============================================================================
	std::map<int, std::unique_ptr<ExtraArticulation>> extraMap;
	// Stores up to 5 file paths for each Pad (8 pads x 5 layers)
	std::array<std::array<juce::File, 5>, NUM_SOUNDS> multiLayerFiles;
	void triggerExtra(int midiNote, float velocity);
	void loadExtraSample(int midiNote, int layer, const juce::File& file);
	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
	bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

	void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override;

	//==============================================================================
	const juce::String getName() const override;

	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool isMidiEffect() const override;
	double getTailLengthSeconds() const override;

	//==============================================================================
	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram(int index) override;
	const juce::String getProgramName(int index) override;
	void changeProgramName(int index, const juce::String& newName) override;

	//==============================================================================
	void getStateInformation(juce::MemoryBlock& destData) override;
	void setStateInformation(const void* data, int sizeInBytes) override;

	//==============================================================================
	// Custom methods for drum functionality
	void triggerDrum(int drumIndex, float velocity = 1.0f);
	void loadSample(int drumIndex, const juce::File& file);
	void loadSpecificLayer(int drumIndex, int layerIndex, const juce::File& file);
	bool isDrumLoaded(int drumIndex) const;
	juce::String getDrumName(int drumIndex) const;
	void setRoutingMode(int index, int mode) { if (index >= 0 && index < NUM_SOUNDS) drumVoices[index].routingMode = mode; }
	int getRoutingMode(int index) const { return (index >= 0 && index < NUM_SOUNDS) ? drumVoices[index].routingMode : 0; }
	int getMidiNoteForPad(int index) const { return (index >= 0 && index < NUM_SOUNDS) ? midiNoteMapping[index] : -1; }
	void saveFullKit(const juce::File& file);
	void loadFullKit(const juce::File& file);

	//==============================================================================
	// ValueTree::Listener
	void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override {}

private:
	struct DrumLayer
	{
		juce::AudioBuffer<float> buffer;
		int lowVel = 0;
		int highVel = 127;
		bool loaded = false;
	};

	struct DrumVoice
	{
		std::vector<DrumLayer> layers; // Multiple layer support (user defined)

		int currentLayerIndex = -1;
		int currentSampleIndex = 0;
		bool isPlaying = false;
		float triggerVelocity = 0.0f;
		int routingMode = 0; // If 0: Both, 1: Main Only, 2: Direct Only
		juce::String name;

		void trigger(float vel)
		{
			int v = juce::jlimit(0, 127, (int)(vel * 127.0f));
			currentLayerIndex = -1;

			for (int i = 0; i < (int)layers.size(); ++i)
			{
				if (layers[i].loaded && v >= layers[i].lowVel && v <= layers[i].highVel)
				{
					currentLayerIndex = i;
					break;
				}
			}

			if (currentLayerIndex != -1)
			{
				triggerVelocity = vel;
				currentSampleIndex = 0;
				isPlaying = true;
			}
		}

		void stop() { isPlaying = false; currentLayerIndex = -1; }

		bool hasValidSample() const {
			for (const auto& l : layers) if (l.loaded) return true;
			return false;
		}
	};

	//==============================================================================
	void processDrumVoices(juce::AudioBuffer<float>& buffer);
	void processMidiEvents(const juce::MidiBuffer& midiMessages);
	void setupDrumNames();

	//==============================================================================
	std::array<DrumVoice, NUM_SOUNDS> drumVoices;
	juce::AudioFormatManager formatManager;

	// MIDI note mappings for drum sounds
	std::array<int, NUM_SOUNDS> midiNoteMapping = {
		36, // KICK (C1)
		38, // SNARE (D1)
		42, // HIHAT (F#1)
		49, // CRASH (C#2)
		45, // TOM1 (A1)
		47, // TOM2 (B1)
		48, // TOM3 (C2)
		51  // RIDE (D#2)
	};

	// Parameters
	std::array<juce::AudioParameterFloat*, NUM_SOUNDS> gainParameters;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSimulatorAudioProcessor)
};
