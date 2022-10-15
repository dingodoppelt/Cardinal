/*
 * DISTRHO Cardinal Plugin
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

/**
 * This file uses code adapted from VCVRack's CV_MIDI.cpp and midi.hpp
 * Copyright (C) 2016-2021 VCV.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 */

#include "plugincontext.hpp"
#include "Expander.hpp"

#ifndef HEADLESS
# include "ImGuiWidget.hpp"
# include "ModuleWidgets.hpp"
# include "extra/Mutex.hpp"
# include "extra/Runner.hpp"
# include "extra/ScopedPointer.hpp"
# include "../../src/extra/SharedResourcePointer.hpp"
#else
# include "extra/Mutex.hpp"
#endif

#include "CarlaNativePlugin.h"
#include "CarlaBackendUtils.hpp"
#include "CarlaEngine.hpp"
#include "water/streams/MemoryOutputStream.h"
#include "water/xml/XmlDocument.h"

#include <string>

#ifndef CARDINAL_SYSDEPS
// private method that takes ownership, we can use it to avoid superfulous allocations
extern "C" {
json_t *jsonp_stringn_nocheck_own(const char* value, size_t len);
}
#endif

// defined elsewhere
namespace rack {
#ifdef ARCH_WIN
enum SpecialPath {
    kSpecialPathUserProfile,
    kSpecialPathCommonProgramFiles,
    kSpecialPathProgramFiles,
    kSpecialPathAppData,
};
std::string getSpecialPath(const SpecialPath type);
#endif
std::string homeDir();
}

#define BUFFER_SIZE 128

// generates a warning if this is defined as anything else
#define CARLA_API

// --------------------------------------------------------------------------------------------------------------------
// strcasestr

#ifdef DISTRHO_OS_WINDOWS
# include <shlwapi.h>
namespace ildaeil {
    inline const char* strcasestr(const char* const haystack, const char* const needle)
    {
        return StrStrIA(haystack, needle);
    }
    // using strcasestr = StrStrIA;
}
#else
namespace ildaeil {
    using ::strcasestr;
}
#endif

// --------------------------------------------------------------------------------------------------------------------

using namespace CARLA_BACKEND_NAMESPACE;

static uint32_t host_get_buffer_size(NativeHostHandle);
static double host_get_sample_rate(NativeHostHandle);
static bool host_is_offline(NativeHostHandle);
static const NativeTimeInfo* host_get_time_info(NativeHostHandle handle);
static bool host_write_midi_event(NativeHostHandle handle, const NativeMidiEvent* event);
static void host_ui_parameter_changed(NativeHostHandle handle, uint32_t index, float value);
static void host_ui_midi_program_changed(NativeHostHandle handle, uint8_t channel, uint32_t bank, uint32_t program);
static void host_ui_custom_data_changed(NativeHostHandle handle, const char* key, const char* value);
static void host_ui_closed(NativeHostHandle handle);
static const char* host_ui_open_file(NativeHostHandle handle, bool isDir, const char* title, const char* filter);
static const char* host_ui_save_file(NativeHostHandle handle, bool isDir, const char* title, const char* filter);
static intptr_t host_dispatcher(NativeHostHandle handle, NativeHostDispatcherOpcode opcode, int32_t index, intptr_t value, void* ptr, float opt);
static void projectLoadedFromDSP(void* ui);

// --------------------------------------------------------------------------------------------------------------------

static Mutex sPluginInfoLoadMutex;

static const char* getPathForJSFX()
{
    static std::string path;

    if (path.empty())
    {
       #if defined(CARLA_OS_MAC)
        path = homeDir() + "/Library/Application Support/REAPER/Effects";
       #elif defined(CARLA_OS_WASM)
        path = "/jsfx";
       #elif defined(CARLA_OS_WIN)
        path = getSpecialPath(kSpecialPathAppData) + "\\REAPER\\Effects";
        if (! system::exists(path))
            path = getSpecialPath(kSpecialPathProgramFiles) + "\\REAPER\\InstallData\\Effects";
       #else
        if (const char* const configHome = std::getenv("XDG_CONFIG_HOME"))
            path = configHome;
        else
            path = homeDir() + "/.config";
        path += "/REAPER/Effects";
       #endif
    }

    return path.c_str();
}

/*
#ifndef HEADLESS
struct JuceInitializer {
    JuceInitializer() { carla_juce_init(); }
    ~JuceInitializer() { carla_juce_cleanup(); }
};
#endif
*/

struct IldaeilModule : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
        INPUT1,
        INPUT2,
        NUM_INPUTS
    };
    enum OutputIds {
        OUTPUT1,
        OUTPUT2,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    /*
#ifndef HEADLESS
    SharedResourcePointer<JuceInitializer> juceInitializer;
#endif
    */

    const CardinalPluginContext* const pcontext;

    const NativePluginDescriptor* fCarlaPluginDescriptor = nullptr;
    NativePluginHandle fCarlaPluginHandle = nullptr;

    NativeHostDescriptor fCarlaHostDescriptor = {};
    CarlaHostHandle fCarlaHostHandle = nullptr;

    NativeTimeInfo fCarlaTimeInfo;

    void* fUI = nullptr;
    bool canUseBridges = true;

    float audioDataIn1[BUFFER_SIZE];
    float audioDataIn2[BUFFER_SIZE];
    float audioDataOut1[BUFFER_SIZE];
    float audioDataOut2[BUFFER_SIZE];
    unsigned audioDataFill = 0;
    uint32_t lastProcessCounter = 0;
    CardinalExpanderFromCarlaMIDIToCV* midiOutExpander = nullptr;

    volatile bool resetMeterIn = true;
    volatile bool resetMeterOut = true;
    float meterInL = 0.0f;
    float meterInR = 0.0f;
    float meterOutL = 0.0f;
    float meterOutR = 0.0f;

    IldaeilModule()
        : pcontext(static_cast<CardinalPluginContext*>(APP))
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (uint i=0; i<2; ++i)
        {
            const char name[] = { 'A','u','d','i','o',' ','#',static_cast<char>('0'+i+1),'\0' };
            configInput(i, name);
            configOutput(i, name);
        }
        std::memset(audioDataOut1, 0, sizeof(audioDataOut1));
        std::memset(audioDataOut2, 0, sizeof(audioDataOut2));

        fCarlaPluginDescriptor = carla_get_native_rack_plugin();
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaPluginDescriptor != nullptr,);

        memset(&fCarlaHostDescriptor, 0, sizeof(fCarlaHostDescriptor));
        memset(&fCarlaTimeInfo, 0, sizeof(fCarlaTimeInfo));

        fCarlaHostDescriptor.handle = this;
        fCarlaHostDescriptor.resourceDir = carla_get_library_folder();
        fCarlaHostDescriptor.uiName = "Ildaeil";
        fCarlaHostDescriptor.uiParentId = 0;

        fCarlaHostDescriptor.get_buffer_size = host_get_buffer_size;
        fCarlaHostDescriptor.get_sample_rate = host_get_sample_rate;
        fCarlaHostDescriptor.is_offline = host_is_offline;

        fCarlaHostDescriptor.get_time_info = host_get_time_info;
        fCarlaHostDescriptor.write_midi_event = host_write_midi_event;
        fCarlaHostDescriptor.ui_parameter_changed = host_ui_parameter_changed;
        fCarlaHostDescriptor.ui_midi_program_changed = host_ui_midi_program_changed;
        fCarlaHostDescriptor.ui_custom_data_changed = host_ui_custom_data_changed;
        fCarlaHostDescriptor.ui_closed = host_ui_closed;
        fCarlaHostDescriptor.ui_open_file = host_ui_open_file;
        fCarlaHostDescriptor.ui_save_file = host_ui_save_file;
        fCarlaHostDescriptor.dispatcher = host_dispatcher;

        fCarlaPluginHandle = fCarlaPluginDescriptor->instantiate(&fCarlaHostDescriptor);
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaPluginHandle != nullptr,);

        fCarlaHostHandle = carla_create_native_plugin_host_handle(fCarlaPluginDescriptor, fCarlaPluginHandle);
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaHostHandle != nullptr,);

#if defined(CARLA_OS_MAC)
        if (system::exists("~/Applications/Carla.app"))
        {
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, "~/Applications/Carla.app/Contents/MacOS");
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "~/Applications/Carla.app/Contents/MacOS/resources");
        }
        else if (system::exists("/Applications/Carla.app"))
        {
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, "/Applications/Carla.app/Contents/MacOS");
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "/Applications/Carla.app/Contents/MacOS/resources");
        }
#elif defined(CARLA_OS_WASM)
        if (true)
        {}
#elif defined(CARLA_OS_WIN)
        const std::string winBinaryDir = system::join(asset::systemDir, "Carla");

        if (system::exists(winBinaryDir))
        {
            const std::string winResourceDir = system::join(winBinaryDir, "resources");
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, winBinaryDir.c_str());
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, winResourceDir.c_str());
        }
#else
        if (system::exists("/usr/local/lib/carla"))
        {
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, "/usr/local/lib/carla");
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "/usr/local/share/carla/resources");
        }
        else if (system::exists("/usr/lib/carla"))
        {
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, "/usr/lib/carla");
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "/usr/share/carla/resources");
        }
#endif
        else
        {
            canUseBridges = false;

            static bool warningShown = false;
            if (! warningShown)
            {
                warningShown = true;
                async_dialog_message("Carla is not installed on this system, bridged plugins will not work");
            }
        }

        if (const char* const path = std::getenv("LV2_PATH"))
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PLUGIN_PATH, PLUGIN_LV2, path);

        carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PLUGIN_PATH, PLUGIN_JSFX, getPathForJSFX());

#ifdef CARLA_OS_MAC
        carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PREFER_UI_BRIDGES, 0, nullptr);
#endif

        fCarlaPluginDescriptor->dispatcher(fCarlaPluginHandle, NATIVE_PLUGIN_OPCODE_HOST_USES_EMBED,
                                           0, 0, nullptr, 0.0f);

        fCarlaPluginDescriptor->activate(fCarlaPluginHandle);
    }

    ~IldaeilModule() override
    {
        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->deactivate(fCarlaPluginHandle);

        if (fCarlaHostHandle != nullptr)
            carla_host_handle_free(fCarlaHostHandle);

        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->cleanup(fCarlaPluginHandle);
    }

    const NativeTimeInfo* hostGetTimeInfo() const noexcept
    {
        return &fCarlaTimeInfo;
    }

    intptr_t hostDispatcher(const NativeHostDispatcherOpcode opcode,
                            const int32_t index, const intptr_t value, void* const ptr, const float opt)
    {
        switch (opcode)
        {
        // cannnot be supported
        case NATIVE_HOST_OPCODE_HOST_IDLE:
            break;
        // other stuff
        case NATIVE_HOST_OPCODE_NULL:
        case NATIVE_HOST_OPCODE_UPDATE_PARAMETER:
        case NATIVE_HOST_OPCODE_UPDATE_MIDI_PROGRAM:
        case NATIVE_HOST_OPCODE_RELOAD_PARAMETERS:
        case NATIVE_HOST_OPCODE_RELOAD_MIDI_PROGRAMS:
        case NATIVE_HOST_OPCODE_RELOAD_ALL:
        case NATIVE_HOST_OPCODE_UI_UNAVAILABLE:
        case NATIVE_HOST_OPCODE_INTERNAL_PLUGIN:
        case NATIVE_HOST_OPCODE_QUEUE_INLINE_DISPLAY:
        case NATIVE_HOST_OPCODE_UI_TOUCH_PARAMETER:
        case NATIVE_HOST_OPCODE_REQUEST_IDLE:
        case NATIVE_HOST_OPCODE_GET_FILE_PATH:
        case NATIVE_HOST_OPCODE_UI_RESIZE:
        case NATIVE_HOST_OPCODE_PREVIEW_BUFFER_DATA:
            // TESTING
            d_stdout("dispatcher %i, %i, %li, %p, %f", opcode, index, value, ptr, opt);
            break;
        }

        return 0;
    }

    json_t* dataToJson() override
    {
        if (fCarlaHostHandle == nullptr)
            return nullptr;

        CarlaEngine* const engine = carla_get_engine_from_handle(fCarlaHostHandle);

        water::MemoryOutputStream projectState;
        engine->saveProjectInternal(projectState);

        const size_t dataSize = projectState.getDataSize();
#ifndef CARDINAL_SYSDEPS
        return jsonp_stringn_nocheck_own(static_cast<const char*>(projectState.getDataAndRelease()), dataSize);
#else
        return json_stringn(static_cast<const char*>(projectState.getData()), dataSize);
#endif
    }

    void dataFromJson(json_t* const rootJ) override
    {
        if (fCarlaHostHandle == nullptr)
            return;

        const char* const projectState = json_string_value(rootJ);
        DISTRHO_SAFE_ASSERT_RETURN(projectState != nullptr,);

        CarlaEngine* const engine = carla_get_engine_from_handle(fCarlaHostHandle);

        water::XmlDocument xml(projectState);

        {
            const MutexLocker cml(sPluginInfoLoadMutex);
            engine->loadProjectInternal(xml, true);
        }

        projectLoadedFromDSP(fUI);
    }

    void process(const ProcessArgs& args) override
    {
        if (fCarlaPluginHandle == nullptr)
            return;

        const unsigned i = audioDataFill++;

        audioDataIn1[i] = inputs[INPUT1].getVoltage() * 0.1f;
        audioDataIn2[i] = inputs[INPUT2].getVoltage() * 0.1f;
        outputs[OUTPUT1].setVoltage(audioDataOut1[i] * 10.0f);
        outputs[OUTPUT2].setVoltage(audioDataOut2[i] * 10.0f);

        if (audioDataFill == BUFFER_SIZE)
        {
            const uint32_t processCounter = pcontext->processCounter;

            // Update time position if running a new audio block
            if (lastProcessCounter != processCounter)
            {
                lastProcessCounter = processCounter;
                fCarlaTimeInfo.playing = pcontext->playing;
                fCarlaTimeInfo.frame = pcontext->frame;
                fCarlaTimeInfo.bbt.valid = pcontext->bbtValid;
                fCarlaTimeInfo.bbt.bar = pcontext->bar;
                fCarlaTimeInfo.bbt.beat = pcontext->beat;
                fCarlaTimeInfo.bbt.tick = pcontext->tick;
                fCarlaTimeInfo.bbt.barStartTick = pcontext->barStartTick;
                fCarlaTimeInfo.bbt.beatsPerBar = pcontext->beatsPerBar;
                fCarlaTimeInfo.bbt.beatType = pcontext->beatType;
                fCarlaTimeInfo.bbt.ticksPerBeat = pcontext->ticksPerBeat;
                fCarlaTimeInfo.bbt.beatsPerMinute = pcontext->beatsPerMinute;
            }
            // or advance time by BUFFER_SIZE frames if still under the same audio block
            else if (fCarlaTimeInfo.playing)
            {
                fCarlaTimeInfo.frame += BUFFER_SIZE;

                // adjust BBT as well
                if (fCarlaTimeInfo.bbt.valid)
                {
                    const double samplesPerTick = 60.0 * args.sampleRate
                                                / fCarlaTimeInfo.bbt.beatsPerMinute
                                                / fCarlaTimeInfo.bbt.ticksPerBeat;

                    int32_t newBar = fCarlaTimeInfo.bbt.bar;
                    int32_t newBeat = fCarlaTimeInfo.bbt.beat;
                    double newTick = fCarlaTimeInfo.bbt.tick + (double)BUFFER_SIZE / samplesPerTick;

                    while (newTick >= fCarlaTimeInfo.bbt.ticksPerBeat)
                    {
                        newTick -= fCarlaTimeInfo.bbt.ticksPerBeat;

                        if (++newBeat > fCarlaTimeInfo.bbt.beatsPerBar)
                        {
                            newBeat = 1;

                            ++newBar;
                            fCarlaTimeInfo.bbt.barStartTick += fCarlaTimeInfo.bbt.beatsPerBar * fCarlaTimeInfo.bbt.ticksPerBeat;
                        }
                    }

                    fCarlaTimeInfo.bbt.bar = newBar;
                    fCarlaTimeInfo.bbt.beat = newBeat;
                    fCarlaTimeInfo.bbt.tick = newTick;
                }
            }

            NativeMidiEvent* midiEvents;
            uint midiEventCount;

            if (CardinalExpanderFromCVToCarlaMIDI* const midiInExpander = leftExpander.module != nullptr && leftExpander.module->model == modelExpanderInputMIDI
                                                                        ? static_cast<CardinalExpanderFromCVToCarlaMIDI*>(leftExpander.module)
                                                                        : nullptr)
            {
                midiEvents = midiInExpander->midiEvents;
                midiEventCount = midiInExpander->midiEventCount;
                midiInExpander->midiEventCount = midiInExpander->frame = 0;
            }
            else
            {
                midiEvents = nullptr;
                midiEventCount = 0;
            }

            if ((midiOutExpander = rightExpander.module != nullptr && rightExpander.module->model == modelExpanderOutputMIDI
                                 ? static_cast<CardinalExpanderFromCarlaMIDIToCV*>(rightExpander.module)
                                 : nullptr))
                midiOutExpander->midiEventCount = 0;

            audioDataFill = 0;
            float* ins[2] = { audioDataIn1, audioDataIn2 };
            float* outs[2] = { audioDataOut1, audioDataOut2 };

            if (resetMeterIn)
                meterInL = meterInR = 0.0f;

            meterInL = std::max(meterInL, d_findMaxNormalizedFloat(audioDataIn1, BUFFER_SIZE));
            meterInR = std::max(meterInR, d_findMaxNormalizedFloat(audioDataIn2, BUFFER_SIZE));

            fCarlaPluginDescriptor->process(fCarlaPluginHandle, ins, outs, BUFFER_SIZE, midiEvents, midiEventCount);

            if (resetMeterOut)
                meterOutL = meterOutR = 0.0f;

            meterOutL = std::max(meterOutL, d_findMaxNormalizedFloat(audioDataOut1, BUFFER_SIZE));
            meterOutR = std::max(meterOutR, d_findMaxNormalizedFloat(audioDataOut2, BUFFER_SIZE));

            resetMeterIn = resetMeterOut = false;
        }
    }

    void onReset() override
    {
        resetMeterIn = resetMeterOut = true;
        midiOutExpander = nullptr;
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override
    {
        if (fCarlaPluginHandle == nullptr)
            return;

        resetMeterIn = resetMeterOut = true;
        midiOutExpander = nullptr;

        fCarlaPluginDescriptor->deactivate(fCarlaPluginHandle);
        fCarlaPluginDescriptor->dispatcher(fCarlaPluginHandle, NATIVE_PLUGIN_OPCODE_SAMPLE_RATE_CHANGED,
                                           0, 0, nullptr, e.sampleRate);
        fCarlaPluginDescriptor->activate(fCarlaPluginHandle);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IldaeilModule)
};

// -----------------------------------------------------------------------------------------------------------

static uint32_t host_get_buffer_size(const NativeHostHandle handle)
{
    return BUFFER_SIZE;
}

static double host_get_sample_rate(const NativeHostHandle handle)
{
    const CardinalPluginContext* const pcontext = static_cast<IldaeilModule*>(handle)->pcontext;
    DISTRHO_SAFE_ASSERT_RETURN(pcontext != nullptr, 48000.0);
    return pcontext->sampleRate;
}

static bool host_is_offline(NativeHostHandle)
{
    return false;
}

static const NativeTimeInfo* host_get_time_info(const NativeHostHandle handle)
{
    return static_cast<IldaeilModule*>(handle)->hostGetTimeInfo();
}

static bool host_write_midi_event(const NativeHostHandle handle, const NativeMidiEvent* const event)
{
    if (CardinalExpanderFromCarlaMIDIToCV* const expander = static_cast<IldaeilModule*>(handle)->midiOutExpander)
    {
        if (expander->midiEventCount == CardinalExpanderFromCarlaMIDIToCV::MAX_MIDI_EVENTS)
            return false;

        NativeMidiEvent& expanderEvent(expander->midiEvents[expander->midiEventCount++]);
        carla_copyStruct(expanderEvent, *event);
        return true;
    }

    return false;
}

static void host_ui_midi_program_changed(NativeHostHandle handle, uint8_t channel, uint32_t bank, uint32_t program)
{
    d_stdout("%s %p %u %u %u", __FUNCTION__, handle, channel, bank, program);
}

static void host_ui_custom_data_changed(NativeHostHandle handle, const char* key, const char* value)
{
    d_stdout("%s %p %s %s", __FUNCTION__, handle, key, value);
}

static const char* host_ui_save_file(NativeHostHandle, bool, const char*, const char*)
{
    return nullptr;
}

static intptr_t host_dispatcher(const NativeHostHandle handle, const NativeHostDispatcherOpcode opcode,
                                const int32_t index, const intptr_t value, void* const ptr, const float opt)
{
    return static_cast<IldaeilModule*>(handle)->hostDispatcher(opcode, index, value, ptr, opt);
}

// --------------------------------------------------------------------------------------------------------------------

#ifndef HEADLESS
struct IldaeilWidget : ImGuiWidget, IdleCallback, Runner {
    static constexpr const uint kButtonHeight = 20;

    struct PluginInfoCache {
        char* name;
        char* label;

        PluginInfoCache()
            : name(nullptr),
              label(nullptr) {}

        ~PluginInfoCache()
        {
            std::free(name);
            std::free(label);
        }
    };

    struct PluginGenericUI {
        char* title;
        uint parameterCount;
        struct Parameter {
            char* name;
            char* printformat;
            uint32_t rindex;
            bool boolean, bvalue, log, readonly;
            float min, max, power;
            Parameter()
                : name(nullptr),
                  printformat(nullptr),
                  rindex(0),
                  boolean(false),
                  bvalue(false),
                  log(false),
                  readonly(false),
                  min(0.0f),
                  max(1.0f) {}
            ~Parameter()
            {
                std::free(name);
                std::free(printformat);
            }
        }* parameters;
        float* values;

        PluginGenericUI()
            : title(nullptr),
              parameterCount(0),
              parameters(nullptr),
              values(nullptr) {}

        ~PluginGenericUI()
        {
            std::free(title);
            delete[] parameters;
            delete[] values;
        }
    };

    enum {
        kDrawingLoading,
        kDrawingPluginError,
        kDrawingPluginList,
        kDrawingPluginGenericUI,
        kDrawingErrorInit,
        kDrawingErrorDraw
    } fDrawingState = kDrawingLoading;

    enum {
        kIdleInit,
        kIdleInitPluginAlreadyLoaded,
        kIdleLoadSelectedPlugin,
        kIdlePluginLoadedFromDSP,
        kIdleResetPlugin,
        kIdleOpenFileUI,
        kIdleShowCustomUI,
        kIdleHidePluginUI,
        kIdleGiveIdleToUI,
        kIdleChangePluginType,
        kIdleNothing
    } fIdleState = kIdleInit;

    struct RunnerData {
        bool needsReinit = true;
        uint pluginCount = 0;
        uint pluginIndex = 0;

        void init()
        {
            needsReinit = true;
            pluginCount = 0;
            pluginIndex = 0;
        }
    } fRunnerData;

   #ifdef CARLA_OS_WASM
    PluginType fPluginType = PLUGIN_JSFX;
   #else
    PluginType fPluginType = PLUGIN_LV2;
   #endif
    PluginType fNextPluginType = fPluginType;
    uint fPluginCount = 0;
    int fPluginSelected = -1;
    bool fPluginScanningFinished = false;
    bool fPluginHasCustomUI = false;
    bool fPluginHasFileOpen = false;
    bool fPluginHasOutputParameters = false;
    bool fPluginRunning = false;
    bool fPluginWillRunInBridgeMode = false;
    PluginInfoCache* fPlugins = nullptr;
    ScopedPointer<PluginGenericUI> fPluginGenericUI;

    bool fPluginSearchActive = false;
    bool fPluginSearchFirstShow = false;
    char fPluginSearchString[0xff] = {};

    String fPopupError, fPluginFilename;

    bool idleCallbackActive = false;
    IldaeilModule* const module;

    IldaeilWidget(IldaeilModule* const m)
        : ImGuiWidget(),
          module(m)
    {
        std::strcpy(fPluginSearchString, "Search...");

        if (m != nullptr)
        {
            if (m->fCarlaHostHandle == nullptr)
            {
                fDrawingState = kDrawingErrorInit;
                fIdleState = kIdleNothing;
                fPopupError = "Ildaeil backend failed to init properly, cannot continue.";
                return;
            }

            if (checkIfPluginIsLoaded())
                fIdleState = kIdleInitPluginAlreadyLoaded;

            m->fUI = this;
        }
        else
        {
            fDrawingState = kDrawingPluginList;
            fIdleState = kIdleNothing;
        }
    }

    ~IldaeilWidget() override
    {
        if (module != nullptr && module->fCarlaHostHandle != nullptr)
        {
            if (idleCallbackActive)
                module->pcontext->removeIdleCallback(this);

            if (fPluginRunning)
                carla_show_custom_ui(module->fCarlaHostHandle, 0, false);

            carla_set_engine_option(module->fCarlaHostHandle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, "0");

            module->fUI = nullptr;
        }

        stopRunner();

        fPluginGenericUI = nullptr;

        delete[] fPlugins;
    }

    bool checkIfPluginIsLoaded()
    {
        const CarlaHostHandle handle = module->fCarlaHostHandle;

        if (carla_get_current_plugin_count(handle) == 0)
            return false;

        const uint hints = carla_get_plugin_info(handle, 0)->hints;
        updatePluginFlags(hints);

        fPluginRunning = true;
        return true;
    }

    void updatePluginFlags(const uint hints) noexcept
    {
        if (hints & PLUGIN_HAS_CUSTOM_UI_USING_FILE_OPEN)
        {
            fPluginHasCustomUI = false;
            fPluginHasFileOpen = true;
        }
        else
        {
            fPluginHasCustomUI = hints & PLUGIN_HAS_CUSTOM_UI;
            fPluginHasFileOpen = false;
        }
    }

    void projectLoadedFromDSP()
    {
        if (checkIfPluginIsLoaded())
            fIdleState = kIdlePluginLoadedFromDSP;
    }

    void changeParameterFromDSP(const uint32_t index, const float value)
    {
        if (PluginGenericUI* const ui = fPluginGenericUI)
        {
            for (uint32_t i=0; i < ui->parameterCount; ++i)
            {
                if (ui->parameters[i].rindex != index)
                    continue;

                ui->values[i] = value;

                if (ui->parameters[i].boolean)
                    ui->parameters[i].bvalue = value > ui->parameters[i].min;

                break;
            }
        }

        setDirty(true);
    }

    void closeUI() noexcept
    {
        if (fIdleState == kIdleGiveIdleToUI)
            fIdleState = kIdleNothing;
    }

    void openFileFromDSP(bool /* isDir */, const char* const title, const char* /* filter */)
    {
        DISTRHO_SAFE_ASSERT_RETURN(idleCallbackActive,);
        DISTRHO_SAFE_ASSERT_RETURN(fPluginType == PLUGIN_INTERNAL || fPluginType == PLUGIN_LV2,);

        const CarlaHostHandle handle = module->fCarlaHostHandle;
        async_dialog_filebrowser(false, nullptr, nullptr, title, [handle](char* path)
        {
            if (path == nullptr)
                return;

            carla_set_custom_data(handle, 0, CUSTOM_DATA_TYPE_PATH, "file", path);
            std::free(path);
        });
    }

    void createOrUpdatePluginGenericUI(const CarlaHostHandle handle)
    {
        const CarlaPluginInfo* const info = carla_get_plugin_info(handle, 0);

        fDrawingState = kDrawingPluginGenericUI;
        updatePluginFlags(info->hints);

        if (fPluginGenericUI == nullptr)
            createPluginGenericUI(handle, info);
        else
            updatePluginGenericUI(handle);

        setDirty(true);
    }

    void hidePluginUI(const CarlaHostHandle handle)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fPluginRunning,);

        carla_show_custom_ui(handle, 0, false);
    }

    void createPluginGenericUI(const CarlaHostHandle handle, const CarlaPluginInfo* const info)
    {
        PluginGenericUI* const ui = new PluginGenericUI;

        String title(info->name);
        title += " by ";
        title += info->maker;
        ui->title = title.getAndReleaseBuffer();

        fPluginHasOutputParameters = false;

        const uint32_t pcount = ui->parameterCount = carla_get_parameter_count(handle, 0);

        // make count of valid parameters
        for (uint32_t i=0; i < pcount; ++i)
        {
            const ParameterData* const pdata = carla_get_parameter_data(handle, 0, i);

            if ((pdata->hints & PARAMETER_IS_ENABLED) == 0x0)
            {
                --ui->parameterCount;
                continue;
            }

            if (pdata->type == PARAMETER_OUTPUT)
                fPluginHasOutputParameters = true;
        }

        ui->parameters = new PluginGenericUI::Parameter[ui->parameterCount];
        ui->values = new float[ui->parameterCount];

        // now safely fill in details
        for (uint32_t i=0, j=0; i < pcount; ++i)
        {
            const ParameterData* const pdata = carla_get_parameter_data(handle, 0, i);

            if ((pdata->hints & PARAMETER_IS_ENABLED) == 0x0)
                continue;

            const CarlaParameterInfo* const pinfo = carla_get_parameter_info(handle, 0, i);
            const ::ParameterRanges* const pranges = carla_get_parameter_ranges(handle, 0, i);

            String printformat;

            if (pdata->hints & PARAMETER_IS_INTEGER)
                printformat = "%.0f ";
            else
                printformat = "%.3f ";

            printformat += pinfo->unit;

            PluginGenericUI::Parameter& param(ui->parameters[j]);
            param.name = strdup(pinfo->name);
            param.printformat = printformat.getAndReleaseBuffer();
            param.rindex = i;
            param.boolean = pdata->hints & PARAMETER_IS_BOOLEAN;
            param.log = pdata->hints & PARAMETER_IS_LOGARITHMIC;
            param.readonly = pdata->type != PARAMETER_INPUT || (pdata->hints & PARAMETER_IS_READ_ONLY);
            param.min = pranges->min;
            param.max = pranges->max;

            ui->values[j] = carla_get_current_parameter_value(handle, 0, i);

            if (param.boolean)
                param.bvalue = ui->values[j] > param.min;
            else
                param.bvalue = false;

            ++j;
        }

        fPluginGenericUI = ui;
    }

    void updatePluginGenericUI(const CarlaHostHandle handle)
    {
        PluginGenericUI* const ui = fPluginGenericUI;
        DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

        for (uint32_t i=0; i < ui->parameterCount; ++i)
        {
            ui->values[i] = carla_get_current_parameter_value(handle, 0, ui->parameters[i].rindex);

            if (ui->parameters[i].boolean)
                ui->parameters[i].bvalue = ui->values[i] > ui->parameters[i].min;
        }
    }

    void loadPlugin(const CarlaHostHandle handle, const char* const label)
    {
        if (fPluginRunning)
        {
            carla_show_custom_ui(handle, 0, false);
            carla_replace_plugin(handle, 0);
        }

        carla_set_engine_option(handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, fPluginWillRunInBridgeMode, nullptr);

        const MutexLocker cml(sPluginInfoLoadMutex);

        if (carla_add_plugin(handle, BINARY_NATIVE, fPluginType, nullptr, nullptr,
                             label, 0, 0x0, PLUGIN_OPTIONS_NULL))
        {
            fPluginRunning = true;
            fPluginGenericUI = nullptr;
            fPluginFilename.clear();
            createOrUpdatePluginGenericUI(handle);
        }
        else
        {
            fPopupError = carla_get_last_error(handle);
            d_stdout("got error: %s", fPopupError.buffer());
            fDrawingState = kDrawingPluginError;
        }

        setDirty(true);
    }

    void loadFileAsPlugin(const CarlaHostHandle handle, const char* const filename)
    {
        if (fPluginRunning)
        {
            carla_show_custom_ui(handle, 0, false);
            carla_replace_plugin(handle, 0);
        }

        carla_set_engine_option(handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, fPluginWillRunInBridgeMode, nullptr);

        const MutexLocker cml(sPluginInfoLoadMutex);

        if (carla_load_file(handle, filename))
        {
            fPluginRunning = true;
            fPluginGenericUI = nullptr;
            fPluginFilename = filename;
            createOrUpdatePluginGenericUI(handle);
        }
        else
        {
            fPopupError = carla_get_last_error(handle);
            d_stdout("got error: %s", fPopupError.buffer());
            fPluginFilename.clear();
            fDrawingState = kDrawingPluginError;
        }

        setDirty(true);
    }

    void onContextCreate(const ContextCreateEvent& e) override
    {
        ImGuiWidget::onContextCreate(e);
        widgetCreated();
    }

    void onContextDestroy(const ContextDestroyEvent& e) override
    {
        widgetDestroyed();
        ImGuiWidget::onContextDestroy(e);
    }

    void onAdd(const AddEvent& e) override
    {
        ImGuiWidget::onAdd(e);
        widgetCreated();
    }

    void onRemove(const RemoveEvent& e) override
    {
        widgetDestroyed();
        ImGuiWidget::onRemove(e);
    }

    void widgetCreated()
    {
        if (module == nullptr)
            return;

        if (const CarlaHostHandle handle = module->fCarlaHostHandle)
        {
            const CardinalPluginContext* const pcontext = module->pcontext;

            char winIdStr[24];
            std::snprintf(winIdStr, sizeof(winIdStr), "%llx", (ulonglong)pcontext->nativeWindowId);

            module->fCarlaHostDescriptor.uiParentId = pcontext->nativeWindowId;
            carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, winIdStr);

            if (pcontext->window != nullptr)
                carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_UI_SCALE, pcontext->window->pixelRatio*1000, nullptr);

            if (! idleCallbackActive)
            {
                idleCallbackActive = pcontext->addIdleCallback(this);
            }
        }
    }

    void widgetDestroyed()
    {
        if (module == nullptr)
            return;

        if (const CarlaHostHandle handle = module->fCarlaHostHandle)
        {
            const CardinalPluginContext* const pcontext = module->pcontext;

            module->fCarlaHostDescriptor.uiParentId = 0;
            carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, "0");

            if (idleCallbackActive)
            {
                idleCallbackActive = false;
                pcontext->removeIdleCallback(this);
            }
        }
    }

    void idleCallback() override
    {
        const CarlaHostHandle handle = module->fCarlaHostHandle;
        DISTRHO_SAFE_ASSERT_RETURN(handle != nullptr,);

        /*
        carla_juce_idle();
        */

        if (fDrawingState == kDrawingPluginGenericUI && fPluginGenericUI != nullptr && fPluginHasOutputParameters)
        {
            updatePluginGenericUI(handle);
            setDirty(true);
        }

        switch (fIdleState)
        {
        case kIdleInit:
            fIdleState = kIdleNothing;
            initAndStartRunner();
            break;

        case kIdleInitPluginAlreadyLoaded:
            fIdleState = kIdleNothing;
            createOrUpdatePluginGenericUI(handle);
            initAndStartRunner();
            break;

        case kIdlePluginLoadedFromDSP:
            fIdleState = kIdleNothing;
            createOrUpdatePluginGenericUI(handle);
            if (fRunnerData.needsReinit)
                initAndStartRunner();
            break;

        case kIdleLoadSelectedPlugin:
            fIdleState = kIdleNothing;
            loadSelectedPlugin(handle);
            break;

        case kIdleResetPlugin:
            fIdleState = kIdleNothing;
            if (fPluginFilename.isNotEmpty())
                loadFileAsPlugin(handle, fPluginFilename.buffer());
            else
                loadPlugin(handle, carla_get_plugin_info(handle, 0)->label);
            break;

        case kIdleOpenFileUI:
            fIdleState = kIdleNothing;
            carla_show_custom_ui(handle, 0, true);
            break;

        case kIdleShowCustomUI:
            fIdleState = kIdleGiveIdleToUI;
            carla_show_custom_ui(handle, 0, true);
            break;

        case kIdleHidePluginUI:
            fIdleState = kIdleNothing;
            carla_show_custom_ui(handle, 0, false);
            break;

        case kIdleGiveIdleToUI:
            if (module->fCarlaPluginDescriptor->ui_idle != nullptr)
                module->fCarlaPluginDescriptor->ui_idle(module->fCarlaPluginHandle);
            break;

        case kIdleChangePluginType:
            fIdleState = kIdleNothing;
            if (fNextPluginType == PLUGIN_TYPE_COUNT)
            {
                if (fPluginRunning)
                    carla_show_custom_ui(handle, 0, false);

                async_dialog_filebrowser(false, nullptr, nullptr, "Load from file", [this](char* path)
                {
                    if (path == nullptr)
                        return;

                    loadFileAsPlugin(module->fCarlaHostHandle, path);
                    std::free(path);
                });
            }
            else
            {
                fPluginSelected = -1;
                stopRunner();
                fPluginType = fNextPluginType;
                initAndStartRunner();
            }
            break;

        case kIdleNothing:
            break;
        }
    }

    void loadSelectedPlugin(const CarlaHostHandle handle)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fPluginSelected >= 0,);

        const PluginInfoCache& info(fPlugins[fPluginSelected]);

        const char* label = nullptr;

        switch (fPluginType)
        {
        case PLUGIN_INTERNAL:
        case PLUGIN_AU:
        case PLUGIN_JSFX:
        case PLUGIN_SFZ:
            label = info.label;
            break;
        case PLUGIN_LV2: {
            const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
            DISTRHO_SAFE_ASSERT_RETURN(slash != nullptr,);
            label = slash+1;
            break;
        }
        default:
            break;
        }

        DISTRHO_SAFE_ASSERT_RETURN(label != nullptr,);

        d_stdout("Loading %s...", info.name);
        loadPlugin(handle, label);
    }

    bool initAndStartRunner()
    {
        if (isRunnerActive())
            stopRunner();

        fRunnerData.init();
        return startRunner();
    }

    bool run() override
    {
        if (fRunnerData.needsReinit)
        {
            fRunnerData.needsReinit = false;

            const char* path;
            switch (fPluginType)
            {
            case PLUGIN_LV2:
                path = std::getenv("LV2_PATH");
                break;
            case PLUGIN_JSFX:
                path = getPathForJSFX();
                break;
            default:
                path = nullptr;
                break;
            }

            fPluginCount = 0;
            delete[] fPlugins;

            {
                const MutexLocker cml(sPluginInfoLoadMutex);

                d_stdout("Will scan plugins now...");
                fRunnerData.pluginCount = carla_get_cached_plugin_count(fPluginType, path);
                d_stdout("Scanning found %u plugins", fRunnerData.pluginCount);
            }

            if (fDrawingState == kDrawingLoading)
            {
                fDrawingState = kDrawingPluginList;
                fPluginSearchFirstShow = true;
            }

            if (fRunnerData.pluginCount != 0)
            {
                fPlugins = new PluginInfoCache[fRunnerData.pluginCount];
                fPluginScanningFinished = false;
                return true;
            }
            else
            {
                fPlugins = nullptr;
                fPluginScanningFinished = true;
                return false;
            }
        }

        const uint index = fRunnerData.pluginIndex++;
        DISTRHO_SAFE_ASSERT_UINT2_RETURN(index < fRunnerData.pluginCount,
                                         index, fRunnerData.pluginCount, false);

        do {
            const MutexLocker cml(sPluginInfoLoadMutex);

            const CarlaCachedPluginInfo* const info = carla_get_cached_plugin_info(fPluginType, index);
            DISTRHO_SAFE_ASSERT_CONTINUE(info != nullptr);

            if (! info->valid)
                break;
            if (info->audioIns > 2)
                break;
            if (info->midiIns != 0 && info->midiIns != 1)
                break;
            if (info->midiOuts != 0 && info->midiOuts != 1)
                break;

            if (fPluginType == PLUGIN_INTERNAL)
            {
                if (std::strcmp(info->label, "audiogain_s") == 0)
                    break;
                if (std::strcmp(info->label, "cv2audio") == 0)
                    break;
                if (std::strcmp(info->label, "lfo") == 0)
                    break;
                if (std::strcmp(info->label, "midi2cv") == 0)
                    break;
                if (std::strcmp(info->label, "midithrough") == 0)
                    break;
                if (std::strcmp(info->label, "3bandsplitter") == 0)
                    break;
            }

            const uint pindex = fPluginCount;
            fPlugins[pindex].name = strdup(info->name);
            fPlugins[pindex].label = strdup(info->label);
            ++fPluginCount;
        } while (false);

        // run again
        if (fRunnerData.pluginIndex != fRunnerData.pluginCount)
            return true;

        // stop here
        fPluginScanningFinished = true;
        return false;
    }

    void drawImGui() override
    {
        switch (fDrawingState)
        {
        case kDrawingLoading:
            drawLoading();
            break;
        case kDrawingPluginError:
            ImGui::OpenPopup("Plugin Error");
            // call ourselves again with the plugin list
            fDrawingState = kDrawingPluginList;
            drawImGui();
            break;
        case kDrawingPluginList:
            drawPluginList();
            break;
        case kDrawingPluginGenericUI:
            drawTopBar();
            drawGenericUI();
            break;
        case kDrawingErrorInit:
            fDrawingState = kDrawingErrorDraw;
            drawError(true);
            break;
        case kDrawingErrorDraw:
            drawError(false);
            break;
        }
    }

    void drawError(const bool open)
    {
        const float scaleFactor = getScaleFactor();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(box.size.x * scaleFactor, box.size.y * scaleFactor));

        const int flags = ImGuiWindowFlags_NoSavedSettings
                        | ImGuiWindowFlags_NoTitleBar
                        | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoCollapse
                        | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("Error Window", nullptr, flags))
        {
            if (open)
                ImGui::OpenPopup("Engine Error");

            const int pflags = ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoResize
                             | ImGuiWindowFlags_NoCollapse
                             | ImGuiWindowFlags_NoScrollbar
                             | ImGuiWindowFlags_NoScrollWithMouse
                             | ImGuiWindowFlags_AlwaysAutoResize
                             | ImGuiWindowFlags_AlwaysUseWindowPadding;

            if (ImGui::BeginPopupModal("Engine Error", nullptr, pflags))
            {
                ImGui::TextUnformatted(fPopupError.buffer(), nullptr);
                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }

    void drawTopBar()
    {
        const float scaleFactor = getScaleFactor();
        const float padding = ImGui::GetStyle().WindowPadding.y * 2;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(box.size.x * scaleFactor, kButtonHeight * scaleFactor + padding));

        const int flags = ImGuiWindowFlags_NoSavedSettings
                        | ImGuiWindowFlags_NoTitleBar
                        | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoCollapse
                        | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("Current Plugin", nullptr, flags))
        {
            if (ImGui::Button("Pick Another..."))
            {
                fIdleState = kIdleHidePluginUI;
                fDrawingState = kDrawingPluginList;
            }

            ImGui::SameLine();

            if (ImGui::Button("Reset"))
                fIdleState = kIdleResetPlugin;

            if (fDrawingState == kDrawingPluginGenericUI)
            {
                if (fPluginHasCustomUI)
                {
                    ImGui::SameLine();

                    if (ImGui::Button("Show Custom GUI"))
                        fIdleState = kIdleShowCustomUI;
                }

                if (fPluginHasFileOpen)
                {
                    ImGui::SameLine();

                    if (ImGui::Button("Open File..."))
                        fIdleState = kIdleOpenFileUI;
                }
            }
        }

        ImGui::End();
    }

    void setupMainWindowPos()
    {
        const float scaleFactor = getScaleFactor();

        float y = 0;
        float width = box.size.x * scaleFactor;
        float height = box.size.y * scaleFactor;

        if (fDrawingState == kDrawingPluginGenericUI)
        {
            y = kButtonHeight * scaleFactor + ImGui::GetStyle().WindowPadding.y * 2 - scaleFactor;
            height -= y;
        }

        ImGui::SetNextWindowPos(ImVec2(0, y));
        ImGui::SetNextWindowSize(ImVec2(width, height));
    }

    void drawGenericUI()
    {
        setupMainWindowPos();

        PluginGenericUI* const ui = fPluginGenericUI;
        DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

        const int pflags = ImGuiWindowFlags_NoSavedSettings
                         | ImGuiWindowFlags_NoResize
                         | ImGuiWindowFlags_NoCollapse
                         | ImGuiWindowFlags_AlwaysAutoResize;

        if (ImGui::Begin(ui->title, nullptr, pflags))
        {
            const CarlaHostHandle handle = module->fCarlaHostHandle;

            for (uint32_t i=0; i < ui->parameterCount; ++i)
            {
                PluginGenericUI::Parameter& param(ui->parameters[i]);

                if (param.readonly)
                {
                    ImGui::BeginDisabled();
                    ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat,
                                       ImGuiSliderFlags_NoInput | (param.log ? ImGuiSliderFlags_Logarithmic : 0x0));
                    ImGui::EndDisabled();
                    continue;
                }

                if (param.boolean)
                {
                    if (ImGui::Checkbox(param.name, &ui->parameters[i].bvalue))
                    {
                        if (ImGui::IsItemActivated())
                        {
                            carla_set_parameter_touch(handle, 0, param.rindex, true);
                            // editParameter(0, true);
                        }

                        ui->values[i] = ui->parameters[i].bvalue ? ui->parameters[i].max : ui->parameters[i].min;
                        carla_set_parameter_value(handle, 0, param.rindex, ui->values[i]);
                        // setParameterValue(0, ui->values[i]);
                    }
                }
                else
                {
                    const bool ret = param.log
                                   ? ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat, ImGuiSliderFlags_Logarithmic)
                                   : ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat);
                    if (ret)
                    {
                        if (ImGui::IsItemActivated())
                        {
                            carla_set_parameter_touch(handle, 0, param.rindex, true);
                            // editParameter(0, true);
                        }

                        carla_set_parameter_value(handle, 0, param.rindex, ui->values[i]);
                        // setParameterValue(0, ui->values[i]);
                    }
                }

                if (ImGui::IsItemDeactivated())
                {
                    carla_set_parameter_touch(handle, 0, param.rindex, false);
                    // editParameter(0, false);
                }
            }
        }

        ImGui::End();
    }

    void drawLoading()
    {
        setupMainWindowPos();

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
            ImGui::TextUnformatted("Loading...", nullptr);

        ImGui::End();
    }

    void drawPluginList()
    {
        static const char* pluginTypes[] = {
            getPluginTypeAsString(PLUGIN_INTERNAL),
            getPluginTypeAsString(PLUGIN_LV2),
            getPluginTypeAsString(PLUGIN_JSFX),
            "Load from file..."
        };

        setupMainWindowPos();

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
        {
            const int pflags = ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoResize
                             | ImGuiWindowFlags_NoCollapse
                             | ImGuiWindowFlags_NoScrollbar
                             | ImGuiWindowFlags_NoScrollWithMouse
                             | ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::BeginPopupModal("Plugin Error", nullptr, pflags))
            {
                ImGui::TextWrapped("Failed to load plugin, error was:\n%s", fPopupError.buffer());

                ImGui::Separator();

                if (ImGui::Button("Ok"))
                    ImGui::CloseCurrentPopup();

                ImGui::SameLine();
                ImGui::Dummy(ImVec2(500 * getScaleFactor(), 1));
                ImGui::EndPopup();
            }
            else if (fPluginSearchFirstShow)
            {
                fPluginSearchFirstShow = false;
                ImGui::SetKeyboardFocusHere();
            }

            if (ImGui::InputText("##pluginsearch", fPluginSearchString, sizeof(fPluginSearchString)-1,
                                 ImGuiInputTextFlags_CharsNoBlank|ImGuiInputTextFlags_AutoSelectAll))
                fPluginSearchActive = true;

            if (ImGui::IsKeyDown(ImGuiKey_Escape))
                fPluginSearchActive = false;

            ImGui::SameLine();
            ImGui::PushItemWidth(-1.0f);

            int current;
            switch (fPluginType)
            {
            case PLUGIN_JSFX:
                current = 2;
                break;
            case PLUGIN_LV2:
                current = 1;
                break;
            default:
                current = 0;
                break;
            }

            if (ImGui::Combo("##plugintypes", &current, pluginTypes, ARRAY_SIZE(pluginTypes)))
            {
                fIdleState = kIdleChangePluginType;
                switch (current)
                {
                case 0:
                    fNextPluginType = PLUGIN_INTERNAL;
                    break;
                case 1:
                    fNextPluginType = PLUGIN_LV2;
                    break;
                case 2:
                    fNextPluginType = PLUGIN_JSFX;
                    break;
                case 3:
                    fNextPluginType = PLUGIN_TYPE_COUNT;
                    break;
                }
            }

            ImGui::BeginDisabled(!fPluginScanningFinished || fPluginSelected < 0);

            if (ImGui::Button("Load Plugin"))
                fIdleState = kIdleLoadSelectedPlugin;

           #ifndef CARLA_OS_WASM
            if (fPluginType != PLUGIN_INTERNAL && (module == nullptr || module->canUseBridges))
            {
                ImGui::SameLine();
                ImGui::Checkbox("Run in bridge mode", &fPluginWillRunInBridgeMode);
            }
           #endif

            ImGui::EndDisabled();

            if (fPluginRunning)
            {
                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                    fDrawingState = kDrawingPluginGenericUI;
            }

            if (ImGui::BeginChild("pluginlistwindow"))
            {
                if (ImGui::BeginTable("pluginlist", 2, ImGuiTableFlags_NoSavedSettings))
                {
                    const char* const search = fPluginSearchActive && fPluginSearchString[0] != '\0' ? fPluginSearchString : nullptr;

                    switch (fPluginType)
                    {
                    case PLUGIN_INTERNAL:
                    case PLUGIN_AU:
                    case PLUGIN_SFZ:
                    case PLUGIN_JSFX:
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Label");
                        ImGui::TableHeadersRow();
                        break;
                    case PLUGIN_LV2:
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("URI");
                        ImGui::TableHeadersRow();
                        break;
                    default:
                        break;
                    }

                    for (uint i=0; i<fPluginCount; ++i)
                    {
                        const PluginInfoCache& info(fPlugins[i]);

                        if (search != nullptr && ildaeil::strcasestr(info.name, search) == nullptr)
                            continue;

                        bool selected = fPluginSelected >= 0 && static_cast<uint>(fPluginSelected) == i;

                        switch (fPluginType)
                        {
                        case PLUGIN_INTERNAL:
                        case PLUGIN_AU:
                        case PLUGIN_JSFX:
                        case PLUGIN_SFZ:
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info.name, &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(info.label, &selected);
                            break;
                        case PLUGIN_LV2: {
                            const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
                            DISTRHO_SAFE_ASSERT_CONTINUE(slash != nullptr);
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info.name, &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(slash+1, &selected);
                            break;
                        }
                        default:
                            break;
                        }

                        if (selected)
                            fPluginSelected = i;
                    }

                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }
        }

        ImGui::End();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IldaeilWidget)
};

// --------------------------------------------------------------------------------------------------------------------

static void host_ui_parameter_changed(const NativeHostHandle handle, const uint32_t index, const float value)
{
    if (IldaeilWidget* const ui = static_cast<IldaeilWidget*>(static_cast<IldaeilModule*>(handle)->fUI))
        ui->changeParameterFromDSP(index, value);
}

static void host_ui_closed(const NativeHostHandle handle)
{
    if (IldaeilWidget* const ui = static_cast<IldaeilWidget*>(static_cast<IldaeilModule*>(handle)->fUI))
        ui->closeUI();
}

static const char* host_ui_open_file(const NativeHostHandle handle,
                                     const bool isDir, const char* const title, const char* const filter)
{
    if (IldaeilWidget* const ui = static_cast<IldaeilWidget*>(static_cast<IldaeilModule*>(handle)->fUI))
        ui->openFileFromDSP(isDir, title, filter);

    return nullptr;
}

static void projectLoadedFromDSP(void* const ui)
{
    if (IldaeilWidget* const uiw = static_cast<IldaeilWidget*>(ui))
        uiw->projectLoadedFromDSP();
}

// --------------------------------------------------------------------------------------------------------------------

struct IldaeilNanoMeterIn : NanoMeter {
    IldaeilModule* const module;

    IldaeilNanoMeterIn(IldaeilModule* const m)
        : module(m)
    {
        withBackground = false;
    }

    void updateMeters() override
    {
        if (module == nullptr || module->resetMeterIn)
            return;

        // Only fetch new values once DSP side is updated
        gainMeterL = module->meterInL;
        gainMeterR = module->meterInR;
        module->resetMeterIn = true;
    }
};

struct IldaeilNanoMeterOut : NanoMeter {
    IldaeilModule* const module;

    IldaeilNanoMeterOut(IldaeilModule* const m)
        : module(m)
    {
        withBackground = false;
    }

    void updateMeters() override
    {
        if (module == nullptr || module->resetMeterOut)
            return;

        // Only fetch new values once DSP side is updated
        gainMeterL = module->meterOutL;
        gainMeterR = module->meterOutR;
        module->resetMeterOut = true;
    }
};

// --------------------------------------------------------------------------------------------------------------------

struct IldaeilModuleWidget : ModuleWidgetWithSideScrews<26> {
    bool hasLeftSideExpander = false;
    bool hasRightSideExpander = false;
    IldaeilWidget* ildaeilWidget = nullptr;

    IldaeilModuleWidget(IldaeilModule* const module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Ildaeil.svg")));
        createAndAddScrews();

        if (module == nullptr || module->pcontext != nullptr)
        {
            ildaeilWidget = new IldaeilWidget(module);
            ildaeilWidget->box.pos = Vec(3 * RACK_GRID_WIDTH + 1, 1);
            ildaeilWidget->box.size = Vec(box.size.x - 6 * RACK_GRID_WIDTH - 2, box.size.y - 2);
            addChild(ildaeilWidget);
        }

        for (uint i=0; i<IldaeilModule::NUM_INPUTS; ++i)
            createAndAddInput(i);

        for (uint i=0; i<IldaeilModule::NUM_OUTPUTS; ++i)
            createAndAddOutput(i);

        IldaeilNanoMeterIn* const meterIn = new IldaeilNanoMeterIn(module);
        meterIn->box.pos = Vec(2.0f, startY + padding * 2);
        meterIn->box.size = Vec(RACK_GRID_WIDTH * 3 - 2.0f, box.size.y - meterIn->box.pos.y - 19.0f);
        addChild(meterIn);

        IldaeilNanoMeterOut* const meterOut = new IldaeilNanoMeterOut(module);
        meterOut->box.pos = Vec(box.size.x - RACK_GRID_WIDTH * 3 + 1.0f, startY + padding * 2);
        meterOut->box.size = Vec(RACK_GRID_WIDTH * 3 - 2.0f, box.size.y - meterOut->box.pos.y - 19.0f);
        addChild(meterOut);
    }

    void draw(const DrawArgs& args) override
    {
        drawBackground(args.vg);

        if (hasLeftSideExpander)
        {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 1, 90 - 19, 18, 49 * 6 - 4);
            nvgFillPaint(args.vg, nvgLinearGradient(args.vg, 0, 0, 18, 0, nvgRGB(0xd0, 0xd0, 0xd0), nvgRGBA(0xd0, 0xd0, 0xd0, 0)));
            nvgFill(args.vg);

            for (int i=1; i<6; ++i)
            {
                const float y = 90 + 49 * i - 23;
                const int col1 = 0x18 + static_cast<int>((y / box.size.y) * (0x21 - 0x18) + 0.5f);
                const int col2 = 0x19 + static_cast<int>((y / box.size.y) * (0x22 - 0x19) + 0.5f);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, 1, y, 18, 4);
                nvgFillColor(args.vg, nvgRGB(col1, col2, col2));
                nvgFill(args.vg);
            }
        }

        if (hasRightSideExpander)
        {
            // i == 0
            nvgBeginPath(args.vg);
            nvgRect(args.vg, box.size.x - 19, 90 - 19, 18, 49 - 4);
            nvgFillColor(args.vg, nvgRGB(0xd0, 0xd0, 0xd0));
            nvgFill(args.vg);

            // gradient for i > 0
            nvgBeginPath(args.vg);
            nvgRect(args.vg, box.size.x - 19, 90 + 49 - 23, 18, 49 * 5);
            nvgFillPaint(args.vg, nvgLinearGradient(args.vg,
                                                    box.size.x - 19, 0, box.size.x - 1, 0,
                                                    nvgRGBA(0xd0, 0xd0, 0xd0, 0), nvgRGB(0xd0, 0xd0, 0xd0)));
            nvgFill(args.vg);

            for (int i=1; i<6; ++i)
            {
                const float y = 90 + 49 * i - 23;
                const int col1 = 0x18 + static_cast<int>((y / box.size.y) * (0x21 - 0x18) + 0.5f);
                const int col2 = 0x19 + static_cast<int>((y / box.size.y) * (0x22 - 0x19) + 0.5f);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, box.size.x - 19, y, 18, 4);
                nvgFillColor(args.vg, nvgRGB(col1, col2, col2));
                nvgFill(args.vg);
            }
        }

        drawOutputJacksArea(args.vg, 2);

        ModuleWidgetWithSideScrews<26>::draw(args);
    }

    void step() override
    {
        hasLeftSideExpander = module != nullptr
                            && module->leftExpander.module != nullptr
                            && module->leftExpander.module->model == modelExpanderInputMIDI;

        hasRightSideExpander = module != nullptr
                             && module->rightExpander.module != nullptr
                             && module->rightExpander.module->model == modelExpanderOutputMIDI;

        ModuleWidgetWithSideScrews<26>::step();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IldaeilModuleWidget)
};
#else
static void host_ui_parameter_changed(NativeHostHandle, uint32_t, float) {}
static void host_ui_closed(NativeHostHandle) {}
static const char* host_ui_open_file(NativeHostHandle, bool, const char*, const char*) { return nullptr; }
static void projectLoadedFromDSP(void*) {}
struct IldaeilModuleWidget : ModuleWidget {
    IldaeilModuleWidget(IldaeilModule* const module) {
        setModule(module);

        addInput(createInput<PJ301MPort>({}, module, IldaeilModule::INPUT1));
        addInput(createInput<PJ301MPort>({}, module, IldaeilModule::INPUT2));
        addOutput(createOutput<PJ301MPort>({}, module, IldaeilModule::OUTPUT1));
        addOutput(createOutput<PJ301MPort>({}, module, IldaeilModule::OUTPUT2));
    }
};
#endif

// --------------------------------------------------------------------------------------------------------------------

Model* modelIldaeil = createModel<IldaeilModule, IldaeilModuleWidget>("Ildaeil");

// --------------------------------------------------------------------------------------------------------------------
