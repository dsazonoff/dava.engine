#pragma once

#include "Base/Any.h"
#include "Base/BaseTypes.h"
#include "Base/FastName.h"
#include "Debug/DVAssert.h"
#include "Functional/Signal.h"
#include "Reflection/Reflection.h"
#include "Reflection/ReflectedMeta.h"

namespace DAVA
{
#if 0
class FilePath;

/**
    \ingroup engine
        Engine setting contain global setting if whole engine. List of settings is predefined.
        To add new setting you should to add new enum value and call `SetupSetting` in reflection-impl block.
        In setting-setup you should specify setting-type, setting string-name and default value.
        For now type of setting can be `bool`, `int32`, `float32`, `String` and `enum`.
        Also you can specify range of valid values, it will be added as meta-data in reflection.
        To access range just get meta-data from reflection by type `SettingRange`.

        If you want to use enum as setting-type - add necessary values to `eSettingValue`,
        call `SetupSettingValue` in reflection-impl block to set string-name of value.
        Then setup setting with type `eSettingValue` and specify range of valid enum values.

        It is possible to load setting from yaml-file. To do it call EngineSettings::Load().
        File with settings represents as list of key-value pairs, for example:
            Landscape.RenderMode: Landscape.RenderMode.NoInstancing
        The key is a setting-name. Value depends of setting-type. If setting has enum-type, the key is string-name of `eSettingValue`
*/

class EngineSettings
{
    struct EngineSettingsDetails;

    DAVA_REFLECTION(EngineSettings);

public:
    /**
        List of engine setting
    */
    enum eSetting : uint32
    {
        SETTING_LANDSCAPE_RENDERMODE,
        SETTING_PROFILE_DLC_MANAGER,
        SETTING_INTERPOLATION_MODE,
        SETTING_INTERPOLATION_SPEED,

        //don't forget setup new enum values in reflection block
        SETTING_COUNT
    };

    /**
        List of setting-values for settings with enum-type
    */
    enum eSettingValue : uint32
    {
        //'SETTING_LANDSCAPE_MODE'
        LANDSCAPE_NO_INSTANCING = 0,
        LANDSCAPE_INSTANCING,
        LANDSCAPE_MORPHING,

        INTERPOLATION_OFF,
        INTERPOLATION_LINEAR,
        INTERPOLATION_SIN,
        INTERPOLATION_LOG,
        INTERPOLATION_CUBIC,

        //don't forget setup new enum values in reflection block
        SETTING_VALUE_COUNT
    };

    /**
        Range of valid values for setting. Used to retrieving from reflection meta-data
    */
    template <typename T>
    struct SettingRange
    {
        SettingRange(const T&, const T&);

        T min;
        T max;
    };

    EngineSettings();

    /**
        Reset setting to defaults
    */
    void Reset();

    /**
        Load setting from yaml-file
    */
    bool Load(const FilePath& filepath);

    /**
        Returns value of setting with `ID`
    */
    template <eSetting ID>
    const Any& GetSetting() const;

    /**
        Set value for setting with `ID`
    */
    template <eSetting ID>
    void SetSetting(const Any& value);

    /**
        Returns string-name of `setting`
    */
    static const FastName& GetSettingName(eSetting setting);

    /**
        Returns string-name of `value`
    */
    static const FastName& GetSettingValueName(eSettingValue value);

    /**
        Returns `eStringValue` by `name`
    */
    static eSettingValue GetSettingValueByName(const FastName& name);

    Signal<eSetting> settingChanged; //!< Emitted when any setting is changed. Setting-ID passes as param

protected:
    template <eSetting ID, typename T>
    const T& GetSettingRefl() const;

    template <eSetting ID, typename T>
    void SetSettingRefl(const T& value);

    std::array<Any, SETTING_COUNT> setting;

    friend struct EngineSettingsDetails;
};
#endif

class EngineSettingsVar;
class EngineSettings
{
public:
    size_t GetVarsCount() const;
    EngineSettingsVar* RegisterVar(FastName name, Any defaultValue, String helpString, ReflectedMeta&& meta = ReflectedMeta());
    EngineSettingsVar* GetVar(FastName name);
    EngineSettingsVar* GetVar(size_t index);

    const Any& GetValue(FastName name);
    void SetValue(FastName name, Any value);

    void Save(String path);
    void Load(String path);

    Signal<EngineSettingsVar*> varChanged; //!< Emitted when any setting var is changed
    Signal<EngineSettingsVar*> varRegistered; //!< Emitted when new setting var is registered

private:
    Vector<std::unique_ptr<EngineSettingsVar>> vars;
    UnorderedMap<FastName, size_t> varsNameMap;
};

class EngineSettingsVar final
{
    friend class EngineSettings;

public:
    FastName GetName() const;
    const Any& GetValue() const;
    const String& GetHelp() const;
    const ReflectedMeta* GetMeta() const;

    void SetValue(Any value);
    void SetValueWithCast(Any value);

private:
    Any value;
    FastName name;
    String help;
    std::unique_ptr<ReflectedMeta> meta;
    EngineSettings* settings = nullptr;
};
} //ns DAVA

#define __DAVA_ENGINE_SETTINGS_IMPL__
#include "Private/EngineSettings_impl.h"
