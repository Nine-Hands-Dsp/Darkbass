# Outil hors plugin : compile le processeur + la fenetre de DARKBASS sous Linux, teste l'audio et exporte des captures PNG
cmake_minimum_required(VERSION 3.22)
project(DarkbassRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_binary_data(DarkbassAssets SOURCES
  ${W}/background.jpg ${W}/toggle_strip.png ${W}/glow_l.png ${W}/glow_r.png ${W}/led_on.png ${W}/fs_refl.png ${W}/Poppins-Medium.ttf ${W}/Poppins-Bold.ttf)
juce_add_console_app(DarkbassRender PRODUCT_NAME "DarkbassRender")
target_sources(DarkbassRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp)
target_include_directories(DarkbassRender PRIVATE ${W})
target_compile_definitions(DarkbassRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JucePlugin_Name="Darkbass" JucePlugin_Build_VST3=0
  JucePlugin_VersionString="1.1.5" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(DarkbassRender PRIVATE DarkbassAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
