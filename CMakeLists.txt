cmake_minimum_required(VERSION 3.22)
project(CoreSampler VERSION 1.0.0)

# Tell CMake where to find JUCE
add_subdirectory(/Users/nicolasborromeo/code/JUCE CoreSampler_JUCE)

# Point to the AAX SDK bundled inside JUCE (no separate download needed)
juce_set_aax_sdk_path("/Users/nicolasborromeo/code/JUCE/modules/juce_audio_plugin_client/AAX/SDK")

# Create the plugin target
juce_add_plugin(CoreSampler
    COMPANY_NAME "MyPlugins"
    PLUGIN_MANUFACTURER_CODE Mypl
    PLUGIN_CODE Csmp
    FORMATS VST3 AAX Standalone
    AAX_CATEGORY "SWGenerators"
    PRODUCT_NAME "Core Sampler"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

# Generate the JuceHeader.h file (lets us use #include <JuceHeader.h> in our code)
juce_generate_juce_header(CoreSampler)

# Embed PNG assets as compile-time binary data
juce_add_binary_data(CoreSamplerBinaryData
    SOURCES
        Resources/knobStrip.png
)

# Source files
target_sources(CoreSampler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# Preprocessor definitions
target_compile_definitions(CoreSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link against the JUCE modules we need
target_link_libraries(CoreSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_dsp
        CoreSamplerBinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
