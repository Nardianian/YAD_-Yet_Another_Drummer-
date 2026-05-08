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
		// We replace the old single output with Main + 8 separate outputs
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
	// We only record Lossless / Professional formats
	formatManager.registerFormat(new juce::WavAudioFormat(), true);
	formatManager.registerFormat(new juce::AiffAudioFormat(), false);
	formatManager.registerFormat(new juce::FlacAudioFormat(), false);
	// Nota: Non registriamo MP3 o Ogg

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
	// because we read directly from the buffers during the processBlock.
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

	// 2. On Other Bus: JUCE manages the compatibility defined in the constructor (since we have a mix of Mono and Stereo).
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

	int numExtraSamples = buffer.getNumSamples();
	for (auto const& [note, artPtr] : extraMap)
	{
		auto& art = *artPtr;
		if (!art.isPlaying || art.activeLayerIndex < 0) continue;

		auto* buf = art.buffers[art.activeLayerIndex].get();
		if (buf == nullptr) continue;

		float effectiveGain = art.gain * art.triggerVelocity;
		int sampleLength = buf->getNumSamples();
		int sampleChannels = buf->getNumChannels();
		int busIndex = art.outputBus;

		for (int s = 0; s < numExtraSamples; ++s)
		{
			if (art.currentSampleIndex >= sampleLength) { art.isPlaying = false; break; }

			for (int ch = 0; ch < 2; ++ch)
			{
				float val = buf->getSample(ch % sampleChannels, art.currentSampleIndex) * effectiveGain;

				if (busIndex == 0)
				{
					buffer.addSample(ch, s, val);
				}
				else
				{
					int outputChannelIndex = 7 + busIndex;
					auto outBus = getBusBuffer(buffer, false, 1);
					if (outputChannelIndex < outBus.getNumChannels())
						outBus.addSample(outputChannelIndex, s, val);
				}
			}
			art.currentSampleIndex++;
		}
	}
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
		// Se carichiamo un file WAV singolo, puliamo i layer precedenti e creiamo un layer unico
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
	juce::File kitFolder = file.getParentDirectory(); // La cartella dove risiede il file .yadkit

	// 1. Standard Pad Save
	auto* padsNode = xml.createNewChildElement("STANDARD_PADS");
	for (int i = 0; i < NUM_SOUNDS; ++i)
	{
		auto* p = padsNode->createNewChildElement("PAD");
		p->setAttribute("index", i);
		for (int l = 0; l < 5; ++l)
		{
			// Gets the relative path to the kit folder
			juce::String relPath = kitFolder.getRelativePathFrom(multiLayerFiles[i][l]);
			p->setAttribute("file_layer_" + juce::String(l), relPath);
		}
	}

	// 2. Extra Map Save
	auto* extraNode = xml.createNewChildElement("EXTRA_MAP");
	for (auto const& [note, art] : extraMap)
	{
		auto* e = extraNode->createNewChildElement("ARTICULATION");
		e->setAttribute("midiNote", note);
		e->setAttribute("label", art->label);
		e->setAttribute("outputBus", art->outputBus);

		for (int l = 0; l < 3; ++l)
		{
			juce::String relPath = kitFolder.getRelativePathFrom(art->sampleFiles[l]);
			e->setAttribute("file_layer_" + juce::String(l), relPath);
		}
	}

	xml.writeTo(file);
}

void DrumSimulatorAudioProcessor::loadFullKit(const juce::File& file)
{
	auto xml = juce::XmlDocument::parse(file);
	if (xml == nullptr || !xml->hasTagName("YAD_KIT")) return;

	juce::File kitFolder = file.getParentDirectory();

	// 1. Standard Pad Loading 
	if (auto* padsNode = xml->getChildByName("STANDARD_PADS"))
	{
		for (auto* p : padsNode->getChildIterator())
		{
			int idx = p->getIntAttribute("index");
			for (int l = 0; l < 5; ++l)
			{
				juce::String relPath = p->getStringAttribute("file_layer_" + juce::String(l));
				if (relPath.isNotEmpty())
				{
					// Rebuilds the absolute path starting from the current folder
					juce::File f = kitFolder.getChildFile(relPath);
					if (f.existsAsFile()) {
						loadSpecificLayer(idx, l, f);
						multiLayerFiles[idx][l] = f; // Update the reference
					}
				}
			}
		}
	}

	// 2. Extra Map Loading 
	extraMap.clear();
	if (auto* extraNode = xml->getChildByName("EXTRA_MAP"))
	{
		for (auto* e : extraNode->getChildIterator())
		{
			int note = e->getIntAttribute("midiNote");
			juce::String label = e->getStringAttribute("label");

			extraMap[note] = std::make_unique<ExtraArticulation>();
			extraMap[note]->midiNote = note;
			extraMap[note]->label = label;
			extraMap[note]->outputBus = e->getIntAttribute("outputBus", 0);

			for (int l = 0; l < 3; ++l)
			{
				juce::String relPath = e->getStringAttribute("file_layer_" + juce::String(l));
				if (relPath.isNotEmpty())
				{
					juce::File f = kitFolder.getChildFile(relPath);
					if (f.existsAsFile()) loadExtraSample(note, l, f);
				}
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

		// 2. If this is the first layer we load, we clean up any old data
		// (but keep any other manual slots if they exist)
		if (voice.layers.size() > 5) voice.layers.clear();

		// Make sure the layers vector has at least 5 positions
		while (voice.layers.size() < 5) voice.layers.push_back(DrumLayer());

		// 3. Load audio into the specific slot
		auto& layer = voice.layers[layerIndex];
		layer.buffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
		reader->read(&layer.buffer, 0, (int)reader->lengthInSamples, 0, true, true);
		layer.loaded = true;

		// 4. Automatic recalculation of dynamics (Velocity Mapping)
		// Let's count how many layers are actually loaded
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
			// Covers the entire range up to 127
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

	auto& art = *(it->second);

	// Determine the layer based on velocity
	int layer = 0;
	if (velocity > 0.33f) layer = 1;
	if (velocity > 0.66f) layer = 2;

	// If the chosen layer is empty, search for the first available layer
	if (art.buffers[layer] == nullptr || art.buffers[layer]->getNumSamples() == 0)
	{
		for (int l = 0; l < 3; ++l) {
			if (art.buffers[l] != nullptr && art.buffers[l]->getNumSamples() > 0) {
				layer = l;
				break;
			}
		}
	}

	if (art.buffers[layer] != nullptr && art.buffers[layer]->getNumSamples() > 0)
	{
		art.activeLayerIndex = layer;
		art.currentSampleIndex = 0;
		art.triggerVelocity = velocity;
		art.isPlaying = true;
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

void DrumSimulatorAudioProcessor::clearDrumLayers(int drumIndex)
{
	if (drumIndex >= 0 && drumIndex < NUM_SOUNDS)
	{
		auto& voice = drumVoices[drumIndex];
		voice.stop();

		// Clear the buffers of each layer
		for (auto& layer : voice.layers)
		{
			layer.buffer.setSize(0, 0);
			layer.loaded = false;
		}

		// Clear file references
		for (int i = 0; i < 5; ++i)
			multiLayerFiles[drumIndex][i] = juce::File();
	}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new DrumSimulatorAudioProcessor();
}
