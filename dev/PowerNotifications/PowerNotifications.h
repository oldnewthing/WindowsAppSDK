// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <mutex>
#include <PowerManager.g.h>
#include <PowerNotificationsPal.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

// Forward-declarations
namespace winrt::Microsoft::ProjectReunion::implementation
{
    struct PowerManager;
}

namespace winrt::Microsoft::ProjectReunion::factory_implementation
{
    struct PowerManager;

    using PowerEventHandler =
        Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>;

    struct PowerCallbackBase;

    struct PowerManagerEventBase
    {
        PowerManagerEventBase(PowerCallbackBase& callback) : m_callback(callback) {}
        fire_and_forget NotifyListeners(PowerManager const* sender);
        operator bool() const { return static_cast<bool>(m_event); }

        event<PowerEventHandler> m_event;
        PowerCallbackBase& m_callback;
    };

    template<typename TValue>
    struct PowerManagerEvent : PowerManagerEventBase
    {
        using PowerManagerEventBase::PowerManagerEventBase;
        auto Value() const { return m_value; }
        fire_and_forget UpdateValue(TValue value, PowerManager const* sender);
    private:
        TValue m_value;
    };

    struct PowerCallbackBase
    {
        virtual void Register() = 0;
        virtual void Unregister() = 0;
        virtual void RefreshValue() = 0;

        event_token AddCallback(PowerManagerEventBase& powerEvent, PowerEventHandler const& handler);
        void RemoveCallback(PowerManagerEventBase& powerEvent, event_token const& token);
        void UpdateValueIfNecessary(PowerManagerEventBase& powerEvent);

        template<typename Value>
        Value GetLatestValue(PowerManagerEvent<Value>& powerEvent)
        {
            UpdateValueIfNecessary(powerEvent);
            return powerEvent.Value();
        }

        template<auto Callback>
        static auto MakeStaticCallback()
        {
            return [](auto... value) -> void
            {
                return (make_self<PowerManager>().get()->*Callback)(value...);
            };
        }

    protected:
        std::mutex m_mutex;
    };

    struct EnergySaverPowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(::EnergySaverStatus value);

        void OnCallback(::EnergySaverStatus status);

        using unique_registration = wil::unique_any<EnergySaverStatusRegistration, decltype(::UnregisterEnergySaverStatusChangedListener), ::UnregisterEnergySaverStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::EnergySaverStatus> m_event{ *this };
    };

    struct CompositeBatteryPowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(CompositeBatteryStatus const& status);

        void OnCallback(CompositeBatteryStatus* compositeBatteryStatus);

        using unique_registration = wil::unique_any<CompositeBatteryStatusRegistration, decltype(::UnregisterCompositeBatteryStatusChangedListener), ::UnregisterCompositeBatteryStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::BatteryStatus> m_batteryStatusEvent{ *this };
        PowerManagerEvent<ProjectReunion::PowerSupplyStatus> m_powerSupplyStatusEvent{ *this };
        PowerManagerEvent<int32_t> m_remainingChargePercentEvent{ *this };

        static const auto UNKNOWN_BATTERY_PERCENT = 99999; // NEED TO CHOOSE AND DOCUMENT THIS VALUE
    };

    struct DischargeTimePowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(ULONGLONG value);

        void OnCallback(ULONGLONG dischargeTimeOut);

        using unique_registration = wil::unique_any<DischargeTimeRegistration, decltype(::UnregisterDischargeTimeChangedListener), ::UnregisterDischargeTimeChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<Windows::Foundation::TimeSpan> m_event{ *this };
    };
    
    struct PowerSourcePowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(DWORD value);

        void OnCallback(DWORD powerCondition);

        using unique_registration = wil::unique_any<PowerConditionRegistration, decltype(::UnregisterPowerConditionChangedListener), ::UnregisterPowerConditionChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::PowerSourceStatus> m_event{ *this };
    };

    struct DisplayStatusPowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(DWORD value);

        void OnCallback(DWORD displayStatus);

        using unique_registration = wil::unique_any<DisplayStatusRegistration, decltype(::UnregisterDisplayStatusChangedListener), ::UnregisterDisplayStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::DisplayStatus> m_event{ *this };
    };

    struct SystemIdleStatusPowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;

        void OnCallback();

        using unique_registration = wil::unique_any<SystemIdleStatusRegistration, decltype(::UnregisterSystemIdleStatusChangedListener), ::UnregisterSystemIdleStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::SystemIdleStatus> m_event{ *this };
    };

    struct PowerSchemePersonalityPowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(GUID const& value);

        void OnCallback(GUID const& value);

        using unique_registration = wil::unique_any<PowerSchemePersonalityRegistration, decltype(::UnregisterPowerSchemePersonalityChangedListener), ::UnregisterPowerSchemePersonalityChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::PowerSchemePersonality> m_event{ *this };
    };

    struct UserPresenceStatusPowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(DWORD value);

        void OnCallback(DWORD value);

        using unique_registration = wil::unique_any<UserPresenceStatusRegistration, decltype(::UnregisterUserPresenceStatusChangedListener), ::UnregisterUserPresenceStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::UserPresenceStatus> m_event{ *this };
    };

    struct SystemAwayModeStatusPowerCallback : PowerCallbackBase
    {
        void Register() override;
        void Unregister() override;
        void RefreshValue() override;
        void UpdateValues(DWORD value);

        void OnCallback(DWORD value);

        using unique_registration = wil::unique_any<SystemAwayModeStatusRegistration, decltype(::UnregisterSystemAwayModeStatusChangedListener), ::UnregisterSystemAwayModeStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<ProjectReunion::SystemAwayModeStatus> m_event{ *this };
    };

    struct PowerManager : PowerManagerT<PowerManager, implementation::PowerManager, static_lifetime>,
        EnergySaverPowerCallback,
        CompositeBatteryPowerCallback,
        DischargeTimePowerCallback,
        PowerSourcePowerCallback,
        DisplayStatusPowerCallback,
        SystemIdleStatusPowerCallback,
        PowerSchemePersonalityPowerCallback,
        UserPresenceStatusPowerCallback,
        SystemAwayModeStatusPowerCallback
    {
        ProjectReunion::EnergySaverStatus EnergySaverStatus();
        event_token EnergySaverStatusChanged(PowerEventHandler const& handler);
        void EnergySaverStatusChanged(event_token const& token);

        ProjectReunion::BatteryStatus BatteryStatus();
        event_token BatteryStatusChanged(PowerEventHandler const& handler);
        void BatteryStatusChanged(event_token const& token);

        ProjectReunion::PowerSupplyStatus PowerSupplyStatus();
        event_token PowerSupplyStatusChanged(PowerEventHandler const& handler);
        void PowerSupplyStatusChanged(event_token const& token);

        int32_t RemainingChargePercent();
        event_token RemainingChargePercentChanged(PowerEventHandler const& handler);
        void RemainingChargePercentChanged(event_token const& token);

        Windows::Foundation::TimeSpan RemainingDischargeTime();
        event_token RemainingDischargeTimeChanged(PowerEventHandler const& handler);
        void RemainingDischargeTimeChanged(event_token const& token);

        ProjectReunion::PowerSourceStatus PowerSourceStatus();
        event_token PowerSourceStatusChanged(PowerEventHandler const& handler);
        void PowerSourceStatusChanged(event_token const& token);

        ProjectReunion::DisplayStatus DisplayStatus();
        event_token DisplayStatusChanged(PowerEventHandler const& handler);
        void DisplayStatusChanged(event_token const& token);

        ProjectReunion::SystemIdleStatus SystemIdleStatus();
        event_token SystemIdleStatusChanged(PowerEventHandler const& handler);
        void SystemIdleStatusChanged(event_token const& token);

        ProjectReunion::PowerSchemePersonality PowerSchemePersonality();
        event_token PowerSchemePersonalityChanged(PowerEventHandler const& handler);
        void PowerSchemePersonalityChanged(event_token const& token);

        ProjectReunion::UserPresenceStatus UserPresenceStatus();
        event_token UserPresenceStatusChanged(PowerEventHandler const& handler);
        void UserPresenceStatusChanged(event_token const& token);

        ProjectReunion::SystemAwayModeStatus SystemAwayModeStatus();
        event_token SystemAwayModeStatusChanged(PowerEventHandler const& handler);
        void SystemAwayModeStatusChanged(event_token const& token);
    };
};

namespace winrt::Microsoft::ProjectReunion::implementation
{
     struct PowerManager
     {
        PowerManager() = default;

        //Get function forwards
        static ProjectReunion::EnergySaverStatus EnergySaverStatus()
        {
            return make_self<factory_implementation::PowerManager>()->EnergySaverStatus();
        }

        static ProjectReunion::BatteryStatus BatteryStatus()
        {
            return make_self<factory_implementation::PowerManager>()->BatteryStatus();
        }

        static ProjectReunion::PowerSupplyStatus PowerSupplyStatus()
        {
            return make_self<factory_implementation::PowerManager>()->PowerSupplyStatus();
        }

        static int32_t RemainingChargePercent()
        {
            return make_self<factory_implementation::PowerManager>()->RemainingChargePercent();
        }

        static Windows::Foundation::TimeSpan RemainingDischargeTime()
        {
            return make_self<factory_implementation::PowerManager>()->RemainingDischargeTime();
        }

        static ProjectReunion::PowerSourceStatus PowerSourceStatus()
        {
            return make_self<factory_implementation::PowerManager>()->PowerSourceStatus();
        }

        static ProjectReunion::DisplayStatus DisplayStatus()
        {
            return make_self<factory_implementation::PowerManager>()->DisplayStatus();
        }

        static ProjectReunion::SystemIdleStatus SystemIdleStatus()
        {
            return make_self<factory_implementation::PowerManager>()->SystemIdleStatus();
        }

        static ProjectReunion::PowerSchemePersonality PowerSchemePersonality()
        {
            return make_self<factory_implementation::PowerManager>()->PowerSchemePersonality();
        }

        static ProjectReunion::UserPresenceStatus UserPresenceStatus()
        {
            return make_self<factory_implementation::PowerManager>()->UserPresenceStatus();
        }

        static ProjectReunion::SystemAwayModeStatus SystemAwayModeStatus()
        {
            return make_self<factory_implementation::PowerManager>()->SystemAwayModeStatus();
        }

#if 0
        //Callback forwards
        static void EnergySaverStatusChanged_Callback(::EnergySaverStatus energySaverStatus)
        {
            return make_self<factory_implementation::PowerManager>()->EnergySaverStatusChanged_Callback(energySaverStatus);
        }
#endif

#if 0
        static void RemainingDischargeTimeChanged_Callback(ULONGLONG remainingDischargeTime)
        {
            return make_self<factory_implementation::PowerManager>()->RemainingDischargeTimeChanged_Callback(remainingDischargeTime);
        }

        static void PowerSourceStatusChanged_Callback(DWORD powerCondition)
        {
            return make_self<factory_implementation::PowerManager>()->PowerSourceStatusChanged_Callback(powerCondition);
        }

        static void DisplayStatusChanged_Callback(DWORD displayStatus)
        {
            return make_self<factory_implementation::PowerManager>()->DisplayStatusChanged_Callback(displayStatus);
        }

        static void SystemIdleStatusChanged_Callback()
        {
            return make_self<factory_implementation::PowerManager>()->SystemIdleStatusChanged_Callback();
        }

        static void PowerSchemePersonalityChanged_Callback(GUID powerSchemePersonality)
        {
            return make_self<factory_implementation::PowerManager>()->PowerSchemePersonalityChanged_Callback(powerSchemePersonality);
        }

        static void UserPresenceStatusChanged_Callback(DWORD userPresenceStatus)
        {
            return make_self<factory_implementation::PowerManager>()->UserPresenceStatusChanged_Callback(userPresenceStatus);
        }

        static void SystemAwayModeStatusChanged_Callback(DWORD systemAwayModeStatus)
        {
            return make_self<factory_implementation::PowerManager>()->SystemAwayModeStatusChanged_Callback(systemAwayModeStatus);
        }
#endif
    };
}
#if 0
struct PowerManager;

struct PowerFunctionDetails
{
    PowerEvent& (*event)();
    void (*registerListener)();
    void (*unregisterListener)();
    void (*getStatus)();
};

$


PowerEvent& EnergySaverStatus_Event();
void EnergySaverStatus_Register();
void EnergySaverStatus_Unregister();
void EnergySaverStatus_Update();

PowerEvent& BatteryStatus_Event();
void BatteryStatus_Register();
void BatteryStatus_Unregister();
void BatteryStatus_Update();

PowerEvent& PowerSupplyStatus_Event();
void PowerSupplyStatus_Register();
void PowerSupplyStatus_Unregister();
void PowerSupplyStatus_Update();

PowerEvent& RemainingChargePercent_Event();
void RemainingChargePercent_Register();
void RemainingChargePercent_Unregister();
void RemainingChargePercent_Update();

PowerEvent& RemainingDischargeTime_Event();
void RemainingDischargeTime_Register();
void RemainingDischargeTime_Unregister();
void RemainingDischargeTime_Update();

PowerEvent& PowerSourceStatus_Event();
void PowerSourceStatus_Register();
void PowerSourceStatus_Unregister();
void PowerSourceStatus_Update();

PowerEvent& DisplayStatus_Event();
void DisplayStatus_Register();
void DisplayStatus_Unregister();
void DisplayStatus_Update();

PowerEvent& SystemIdleStatus_Event();
void SystemIdleStatus_Register();
void SystemIdleStatus_Unregister();

PowerEvent& PowerSchemePersonality_Event();
void PowerSchemePersonality_Register();
void PowerSchemePersonality_Unregister();
void PowerSchemePersonality_Update();

PowerEvent& UserPresenceStatus_Event();
void UserPresenceStatus_Register();
void UserPresenceStatus_Unregister();
void UserPresenceStatus_Update();

PowerEvent& SystemAwayModeStatus_Event();
void SystemAwayModeStatus_Register();
void SystemAwayModeStatus_Unregister();
void SystemAwayModeStatus_Update();

// A place holder for an empty function, since not all events have every function defined
void NoOperation() {}
}

#endif
