#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DrumSimulatorAudioProcessor::DrumSimulatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
	: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
		// Sostituiamo la vecchia uscita singola con Main + 8 uscite separate
		.withOutput("Main", juce::AudioChannelSet::stereo(), true)
		.withOutput("Kick", juce::AudioChannelSet::mono(), true)
		.withOutput("Snare", juce::AudioChannelSet::mono(), true)
		.withOutput("HiHat", juce::AudioChannelSet::stereo(), true)
		.withOutput("Crash", juce::AudioChannelSet::stereo(), true)
		.withOutput("Tom1", juce::AudioChannelSet::mono(), true)
		.withOutput("Tom2", juce::AudioChannelSet::mono(), true)
		.withOutput("Tom3", juce::AudioChannelSet::mono(), true)
		.withOutput("Ride", juce::AudioChannelSet::stereo(), true)
#endif
	),
#endif
	parameters(*this, nullptr, juce::Identifier("DrumSimulator"),
		{
			std::make_unique<juce::AudioParameterFloat>("kick_gain", "Kick Gain", 0.0f, 2.0f, 1.0f),
			std::make_unique<juce::AudioParameterFloat>("snare_gain", "Snare Gain", 0.0f, 2.0f, 1.0f),
			std::make_unique<juce::AudioParameterFloat>("hihat_gain", "Hi-Hat Gain", 0.0f, 2.0f, 1.0f),
			std::make_unique<juce::AudioParameterFloat>("crash_gain", "Crash Gain", 0.0f, 2.0f, 1.0f),
			std::make_unique<juce::AudioParameterFloat>("tom1_gain", "Tom 1 Gain", 0.0f, 2.0f, 1.0f),
			std::make_unique<juce::AudioParameterFloat>("tom2_gain", "Tom 2 Gain", 0.0f, 2.0f, 1.0f),
			std::make_unique<juce::AudioParameterFloat>("tom3_gain", "Tom 3 Gain", 0.0f, 2.0f, 1.0f),
			std::make_unique<juce::AudioParameterFloat>("ride_gain", "Ride Gain", 0.0f, 2.0f, 1.0f)
		})
{
	// Record only Lossless / Professional formats
	formatManager.registerFormat(new juce::WavAudioFormat(), true);
	formatManager.registerFormat(new juce::AiffAudioFormat(), false);
	formatManager.registerFormat(new juce::FlacAudioFormat(), false);
	// Note: does not record MP3 or Ogg

	// Setup drum names
	setupDrumNames();

	// Get parameter pointers
	gainParameters[KICK] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("kick_gain"));
	gainParameters[SNARE] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("snare_gain"));
	gainParameters[HIHAT] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("hihat_gain"));
	gainParameters[CRASH] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("crash_gain"));
	gainParameters[TOM1] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("tom1_gain"));
	gainParameters[TOM2] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("tom2_gain"));
	gainParameters[TOM3] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("tom3_gain"));
	gainParameters[RIDE] = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("ride_gain"));
}

DrumSimulatorAudioProcessor::~DrumSimulatorAudioProcessor()
{
}

//==============================================================================
const juce::String DrumSimulatorAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool DrumSimulatorAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool DrumSimulatorAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool DrumSimulatorAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double DrumSimulatorAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int DrumSimulatorAudioProcessor::getNumPrograms()
{
	return 1;
}

int DrumSimulatorAudioProcessor::getCurrentProgram()
{
	return 0;
}

void DrumSimulatorAudioProcessor::setCurrentProgram(int index)
{
	juce::ignoreUnused(index);
}

const juce::String DrumSimulatorAudioProcessor::getProgramName(int index)
{
	juce::ignoreUnused(index);
	return {};
}

void DrumSimulatorAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
	juce::ignoreUnused(index, newName);
}

//==============================================================================
void DrumSimulatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	// The new layer system does not require specific preparation for each entry 
	// (it reads directly from the buffers during the processBlock).
	juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void DrumSimulatorAudioProcessor::releaseResources()
{
	// Resources automatically released by destroying buffers in layers
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DrumSimulatorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
	juce::ignoreUnused(layouts);
	return true;
#else
	// 1. Accept setup only if Main Output is stereo
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
		return false;

	// 2. We're lenient on the other buses: since we have a mix of Mono and Stereo,
	// we let JUCE handle the manufacturer-defined compatibility.
	return true;
#endif
}
#endif

void DrumSimulatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	// Clear channels
	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
		buffer.clear(i, 0, buffer.getNumSamples());

	// =============================================================================
	// Process MIDI events
	processMidiEvents(midiMessages);

	// Process drum voices
	processDrumVoices(buffer);
}

//==============================================================================
bool DrumSimulatorAudioProcessor::hasEditor() const
{
	return true;
}

juce::AudioProcessorEditor* DrumSimulatorAudioProcessor::createEditor()
{
	return new DrumSimulatorAudioProcessorEditor(*this);
}

//==============================================================================
void DrumSimulatorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	auto state = parameters.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void DrumSimulatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
	{
		if (xmlState->hasTagName(parameters.state.getType()))
		{
			parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
		}
	}
}

//==============================================================================
void DrumSimulatorAudioProcessor::triggerDrum(int drumIndex, float velocity)
{
	if (drumIndex >= 0 && drumIndex < NUM_SOUNDS)
	{
		auto& voice = drumVoices[drumIndex];
		if (voice.hasValidSample())
		{
			voice.trigger(velocity);
		}
	}
}

void DrumSimulatorAudioProcessor::loadSample(int drumIndex, const juce::File& file)
{
	if (drumIndex < 0 || drumIndex >= NUM_SOUNDS) return;

	auto& voice = drumVoices[drumIndex];
	std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

	if (reader != nullptr)
	{
		// If we load a single WAV file, we clean up the previous layers and create a single layer
		voice.layers.clear();

		DrumLayer newLayer;
		newLayer.buffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
		reader->read(&newLayer.buffer, 0, (int)reader->lengthInSamples, 0, true, true);

		newLayer.lowVel = 0;
		newLayer.highVel = 127;
		newLayer.loaded = true;

		voice.layers.push_back(std::move(newLayer));
		multiLayerFiles[drumIndex][0] = file;
	}
}

bool DrumSimulatorAudioProcessor::isDrumLoaded(int drumIndex) const
{
	if (drumIndex >= 0 && drumIndex < NUM_SOUNDS)
		return drumVoices[drumIndex].hasValidSample();
	return false;
}

juce::String DrumSimulatorAudioProcessor::getDrumName(int drumIndex) const
{
	if (drumIndex >= 0 && drumIndex < NUM_SOUNDS)
		return drumVoices[drumIndex].name;
	return {};
}

//==============================================================================
void DrumSimulatorAudioProcessor::processDrumVoices(juce::AudioBuffer<float>& buffer)
{
	auto numSamples = buffer.getNumSamples();

	for (int i = 0; i < NUM_SOUNDS; ++i)
	{
		auto& voice = drumVoices[i];
		if (!voice.isPlaying || voice.currentLayerIndex < 0 || voice.currentLayerIndex >= (int)voice.layers.size())
			continue;

		auto outBuffer = getBusBuffer(buffer, false, i + 1);
		auto& activeLayer = voice.layers[voice.currentLayerIndex];

		auto gain = gainParameters[i] ? gainParameters[i]->get() : 1.0f;
		float effectiveGain = gain * voice.triggerVelocity;

		int sampleLength = activeLayer.buffer.getNumSamples();
		int sampleChannels = activeLayer.buffer.getNumChannels();

		for (int s = 0; s < numSamples; ++s)
		{
			if (voice.currentSampleIndex >= sampleLength) { voice.stop(); break; }

			for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
			{
				float val = activeLayer.buffer.getSample(ch % sampleChannels, voice.currentSampleIndex) * effectiveGain;

				// Routing Logic
				// Writes to Main if mode is 0 (Both) or 1 (Main Only)
				if (voice.routingMode == 0 || voice.routingMode == 1)
					buffer.addSample(ch, s, val);

				// Writes to separate output if mode is 0 (Both) or 2 (Direct Only)
				if (voice.routingMode == 0 || voice.routingMode == 2)
				{
					if (outBuffer.getNumChannels() > ch)
						outBuffer.setSample(ch, s, val);
				}
			}
			voice.currentSampleIndex++;
		}
	}
}

void DrumSimulatorAudioProcessor::processMidiEvents(const juce::MidiBuffer& midiMessages)
{
	for (const auto metadata : midiMessages)
	{
		auto message = metadata.getMessage();
		if (message.isNoteOn())
		{
			int midiNote = message.getNoteNumber();
			float velocity = message.getFloatVelocity();

			// 1. Check if it's a standard Pad
			bool isStandardPad = false;
			for (int i = 0; i < NUM_SOUNDS; ++i)
			{
				if (midiNoteMapping[i] == midiNote)
				{
					triggerDrum(i, velocity);
					isStandardPad = true;
					break;
				}
			}

			// 2. If it's not a pad, check if it's an "Extra" note
			if (!isStandardPad)
			{
				auto it = extraMap.find(midiNote);
				if (it != extraMap.end())
				{
					triggerExtra(midiNote, velocity);
				}
			}
		}
	}
}

void DrumSimulatorAudioProcessor::setupDrumNames()
{
	drumVoices[KICK].name = "KICK";
	drumVoices[SNARE].name = "SNARE";
	drumVoices[HIHAT].name = "HI-HAT";
	drumVoices[CRASH].name = "CRASH";
	drumVoices[TOM1].name = "TOM 1";
	drumVoices[TOM2].name = "TOM 2";
	drumVoices[TOM3].name = "TOM 3";
	drumVoices[RIDE].name = "RIDE";
}

void DrumSimulatorAudioProcessor::saveFullKit(const juce::File& file)
{
	juce::XmlElement xml("YAD_KIT");

	// 1. Standard Pad Save
	auto* padsNode = xml.createNewChildElement("STANDARD_PADS");
	for (int i = 0; i < NUM_SOUNDS; ++i)
	{
		auto* p = padsNode->createNewChildElement("PAD");
		p->setAttribute("index", i);
		for (int l = 0; l < 5; ++l)
		{
			p->setAttribute("file_layer_" + juce::String(l), multiLayerFiles[i][l].getFullPathName());
		}
	}

	// 2. Extra Map Save
	auto* extraNode = xml.createNewChildElement("EXTRA_MAP");
	for (auto const& [note, art] : extraMap)
	{
		auto* e = extraNode->createNewChildElement("ARTICULATION");
		e->setAttribute("midiNote", note);
		e->setAttribute("label", art->label);
		for (int l = 0; l < 3; ++l)
		{
			e->setAttribute("file_layer_" + juce::String(l), art->sampleFiles[l].getFullPathName());
		}
	}

	xml.writeTo(file);
}

void DrumSimulatorAudioProcessor::loadFullKit(const juce::File& file)
{
	auto xml = juce::XmlDocument::parse(file);
	if (xml == nullptr || !xml->hasTagName("YAD_KIT")) return;

	// 1. Pad Loading Standard
	if (auto* padsNode = xml->getChildByName("STANDARD_PADS"))
	{
		for (auto* p : padsNode->getChildIterator())
		{
			int idx = p->getIntAttribute("index");
			for (int l = 0; l < 5; ++l)
			{
				juce::File f(p->getStringAttribute("file_layer_" + juce::String(l)));
				if (f.existsAsFile()) loadSpecificLayer(idx, l, f);
			}
		}
	}

	// 2. Extra Map Loading 
	extraMap.clear(); // Let's clean up the current map
	if (auto* extraNode = xml->getChildByName("EXTRA_MAP"))
	{
		for (auto* e : extraNode->getChildIterator())
		{
			int note = e->getIntAttribute("midiNote");
			juce::String label = e->getStringAttribute("label");

			// Let's create the Articulation
			extraMap[note] = std::make_unique<ExtraArticulation>();
			extraMap[note]->midiNote = note;
			extraMap[note]->label = label;

			for (int l = 0; l < 3; ++l)
			{
				juce::File f(e->getStringAttribute("file_layer_" + juce::String(l)));
				if (f.existsAsFile()) loadExtraSample(note, l, f);
			}
		}
	}
}

void DrumSimulatorAudioProcessor::loadSpecificLayer(int drumIndex, int layerIndex, const juce::File& file)
{
	if (drumIndex < 0 || drumIndex >= NUM_SOUNDS || layerIndex < 0 || layerIndex >= 5) return;

	auto& voice = drumVoices[drumIndex];
	std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

	if (reader != nullptr)
	{
		// 1. Save the path for future saving
		multiLayerFiles[drumIndex][layerIndex] = file;

		// 2. If it's the first layer keeps the other slots if exists
		if (voice.layers.size() > 5) voice.layers.clear();

		// Ensures that the layers vector has at least 5 positions
		while (voice.layers.size() < 5) voice.layers.push_back(DrumLayer());

		// 3. Load audio into the specific slot
		auto& layer = voice.layers[layerIndex];
		layer.buffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
		reader->read(&layer.buffer, 0, (int)reader->lengthInSamples, 0, true, true);
		layer.loaded = true;

		// 4. Velocity Mapping (Automatic recalculation of dynamics)
		int loadedCount = 0;
		for (int i = 0; i < 5; ++i) if (voice.layers[i].loaded) loadedCount++;

		if (loadedCount > 0)
		{
			int rangeSize = 128 / loadedCount;
			int currentLow = 0;

			for (int i = 0; i < 5; ++i)
			{
				if (voice.layers[i].loaded)
				{
					voice.layers[i].lowVel = currentLow;
					voice.layers[i].highVel = (currentLow + rangeSize) - 1;

					// The last layer loaded must always reach 127
					currentLow += rangeSize;
				}
			}
			// Fix to cover the entire range up to 127
			for (int i = 4; i >= 0; --i) {
				if (voice.layers[i].loaded) {
					voice.layers[i].highVel = 127;
					break;
				}
			}
		}
	}
}

void DrumSimulatorAudioProcessor::triggerExtra(int midiNote, float velocity)
{
	auto it = extraMap.find(midiNote);
	if (it == extraMap.end()) return;

	// Determines the layer (0, 1 or 2) based on velocity
	int layer = 0;
	if (velocity > 0.4f) layer = 1;
	if (velocity > 0.8f) layer = 2;

	auto& art = *(it->second);
	if (art.buffers[layer] != nullptr && art.buffers[layer]->getNumSamples() > 0)
	{
		// Note: We'd need some "Voice" logic here to handle the current sample index.
		// For now, let's make a logical trigger: in a real drum machine, extra articulations typically use a dynamic voice system. 
		// We'll look at the playhead implementation in the next step.
	}
}

void DrumSimulatorAudioProcessor::loadExtraSample(int midiNote, int layer, const juce::File& file)
{
	if (extraMap.find(midiNote) == extraMap.end()) return;
	if (layer < 0 || layer > 2) return;

	std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
	if (reader != nullptr)
	{
		auto& art = extraMap[midiNote];
		art->buffers[layer] = std::make_unique<juce::AudioSampleBuffer>((int)reader->numChannels, (int)reader->lengthInSamples);
		reader->read(art->buffers[layer].get(), 0, (int)reader->lengthInSamples, 0, true, true);
		art->sampleFiles[layer] = file;
	}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new DrumSimulatorAudioProcessor();
}
