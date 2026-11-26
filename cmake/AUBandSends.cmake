# AUv2 dispatches element properties in Apple's AUBase before reaching JUCE.
# Build private copies of the two affected sources; never edit the JUCE checkout.
set(client "${juce_SOURCE_DIR}/modules/juce_audio_plugin_client")
set(overlay "${CMAKE_CURRENT_BINARY_DIR}/au-band-sends/juce_audio_plugin_client")
file(MAKE_DIRECTORY "${overlay}/AU/AudioUnitSDK")
foreach(src juce_audio_plugin_client_AU_1.mm AU/AudioUnitSDK/AUBase.cpp)
    configure_file("${client}/${src}" "${overlay}/${src}" @ONLY NEWLINE_STYLE UNIX)
endforeach()
set(patch "${CMAKE_CURRENT_LIST_DIR}/au-band-sends.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patch}")
execute_process(COMMAND /usr/bin/patch -p1 -i "${patch}"
    WORKING_DIRECTORY "${overlay}" COMMAND_ERROR_IS_FATAL ANY)
get_target_property(wrapper_sources juce_audio_plugin_client_AU INTERFACE_SOURCES)
list(FIND wrapper_sources "${client}/juce_audio_plugin_client_AU_1.mm" wrapper_index)
if(wrapper_index EQUAL -1)
    message(FATAL_ERROR "Cannot locate the JUCE AUv2 wrapper source")
endif()
list(REMOVE_AT wrapper_sources ${wrapper_index})
list(APPEND wrapper_sources "${overlay}/juce_audio_plugin_client_AU_1.mm")
set_property(TARGET juce_audio_plugin_client_AU PROPERTY INTERFACE_SOURCES "${wrapper_sources}")
target_include_directories(MultiSqueeze_AU BEFORE PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/au-band-sends" "${client}/AU/AudioUnitSDK"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source")

if(MULTISQUEEZE_BUILD_TESTS)
    add_executable(MultiSqueezeAUSendTests Tests/AUSendTests.cpp)
    target_link_libraries(MultiSqueezeAUSendTests PRIVATE "-framework AudioToolbox" "-framework CoreFoundation")
    add_dependencies(MultiSqueezeProcessorTests MultiSqueezeAUSendTests MultiSqueeze_AU)
    # JUCE selects host-specific behaviour from the host executable path.
    set(studio_one_host "$<TARGET_FILE_DIR:MultiSqueezeAUSendTests>/Studio One AU Test Host")
    add_custom_command(TARGET MultiSqueezeAUSendTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:MultiSqueezeAUSendTests>" "${studio_one_host}" VERBATIM)
    add_test(NAME MultiSqueezeAUSharedSend COMMAND "${studio_one_host}" $<TARGET_FILE:MultiSqueeze_AU> --studio-one)
    add_test(NAME MultiSqueezeAUSends COMMAND MultiSqueezeAUSendTests $<TARGET_FILE:MultiSqueeze_AU>)
endif()
