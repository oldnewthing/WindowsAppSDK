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

    struct PowerCallbackBase
    {
        PowerCallbackBase(PowerManager* powerManager) : m_powerManager(powerManager) {}
        virtual void Register() = 0;
        virtual void Unregister() = 0;
        virtual void RefreshValue() = 0;

        PowerManager* m_powerManager;
    };

    struct PowerManagerEventBase
    {
        PowerManagerEventBase(PowerCallbackBase& callback) : m_callback(callback) {}
        fire_and_forget NotifyListeners();
        operator bool() const { return static_cast<bool>(m_event); }

        event<PowerEventHandler> m_event;
        PowerCallbackBase& m_callback;
    };

    template<typename TValue>
    struct PowerManagerEvent : PowerManagerEventBase
    {
        using PowerManagerEventBase::PowerManagerEventBase;
        auto Value() const { return m_value; }
        fire_and_forget UpdateValue(TValue value);
    private:
        TValue m_value;
    };

    struct PowerManager : PowerManagerT<PowerManager, implementation::PowerManager, static_lifetime>
    {
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

    private:
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
            return [](auto value) -> void
            {
                return (make_self<PowerManager>().get()->*Callback)(value);
            };
        }

        std::mutex m_mutex;

#pragma region Energy saver events
    public:
        ProjectReunion::EnergySaverStatus EnergySaverStatus();
        event_token EnergySaverStatusChanged(PowerEventHandler const& handler);
        void EnergySaverStatusChanged(event_token const& token);

    private:
        struct EnergySaverPowerCallback final : PowerCallbackBase
        {
            using PowerCallbackBase::PowerCallbackBase;
            void Register() override;
            void Unregister() override;
            void RefreshValue() override;
            void UpdateValues(::EnergySaverStatus value);

            using unique_registration = wil::unique_any<EnergySaverStatusRegistration, decltype(::UnregisterEnergySaverStatusChangedListener), ::UnregisterEnergySaverStatusChangedListener>;
            unique_registration m_registration;
        };

        EnergySaverPowerCallback m_energySaverPowerCallback{ this };
        void EnergySaver_Callback(::EnergySaverStatus status);

        PowerManagerEvent<ProjectReunion::EnergySaverStatus> m_energySaverStatusEvent{ m_energySaverPowerCallback };
#pragma endregion

#pragma region Composite battery events
    public:
        ProjectReunion::BatteryStatus BatteryStatus();
        event_token BatteryStatusChanged(PowerEventHandler const& handler);
        void BatteryStatusChanged(event_token const& token);

        ProjectReunion::PowerSupplyStatus PowerSupplyStatus();
        event_token PowerSupplyStatusChanged(PowerEventHandler const& handler);
        void PowerSupplyStatusChanged(event_token const& token);

        int32_t RemainingChargePercent();
        event_token RemainingChargePercentChanged(PowerEventHandler const& handler);
        void RemainingChargePercentChanged(event_token const& token);

        static const auto UNKNOWN_BATTERY_PERCENT = 99999; // NEED TO CHOOSE AND DOCUMENT THIS VALUE

    private:
        struct CompositeBatteryPowerCallback final : PowerCallbackBase
        {
            using PowerCallbackBase::PowerCallbackBase;
            void Register() override;
            void Unregister() override;
            void RefreshValue() override;
            void UpdateValues(CompositeBatteryStatus const& status);

            using unique_registration = wil::unique_any<CompositeBatteryStatusRegistration, decltype(::UnregisterCompositeBatteryStatusChangedListener), ::UnregisterCompositeBatteryStatusChangedListener>;
            unique_registration m_registration;
        };

        CompositeBatteryPowerCallback m_compositeBatteryPowerCallback{ this };
        void CompositeBatteryStatusChanged_Callback(CompositeBatteryStatus* compositeBatteryStatus);

        PowerManagerEvent<ProjectReunion::BatteryStatus> m_batteryStatusEvent{ m_compositeBatteryPowerCallback };
        PowerManagerEvent<ProjectReunion::PowerSupplyStatus> m_powerSupplyStatusEvent{ m_compositeBatteryPowerCallback };
        PowerManagerEvent<int32_t> m_remainingChargePercentEvent{ m_compositeBatteryPowerCallback };
#pragma endregion

#pragma region Remaining discharge time events
    public:
        Windows::Foundation::TimeSpan RemainingDischargeTime();
        event_token RemainingDischargeTimeChanged(PowerEventHandler const& handler);
        void RemainingDischargeTimeChanged(event_token const& token);

    private:
        struct DischargeTimePowerCallback final : PowerCallbackBase
        {
            using PowerCallbackBase::PowerCallbackBase;
            void Register() override;
            void Unregister() override;
            void RefreshValue() override;
            void UpdateValues(ULONGLONG value);

            using unique_registration = wil::unique_any<DischargeTimeRegistration, decltype(::UnregisterDischargeTimeChangedListener), ::UnregisterDischargeTimeChangedListener>;
            unique_registration m_registration;
        };

        DischargeTimePowerCallback m_dischargeTimePowerCallback{ this };
        void DischargeTime_Callback(ULONGLONG dischargeTimeOut);

        PowerManagerEvent<Windows::Foundation::TimeSpan> m_remainingDischargeTimeEvent{ m_dischargeTimePowerCallback };
#pragma endregion

#if 0


        // BatteryStatus


        // PowerSourceStatus Functions
        ProjectReunion::PowerSourceStatus PowerSourceStatus()
        {
            CheckRegistrationAndOrUpdateValue(powerSourceStatusFunc);
            return static_cast<ProjectReunion::PowerSourceStatus>(m_cachedPowerSourceStatus);
        }

        event_token PowerSourceStatusChanged(PowerEventHandler const& handler)
        {
            return AddCallback(powerSourceStatusFunc, handler);
        }

        void PowerSourceStatusChanged(event_token const& token)
        {
            RemoveCallback(powerSourceStatusFunc, token);
        }

        void PowerSourceStatusChanged_Callback(DWORD powerCondition)
        {
            m_cachedPowerSourceStatus = powerCondition;
            RaiseEvent(powerSourceStatusFunc);
        }


        // DisplayStatus Functions
        ProjectReunion::DisplayStatus DisplayStatus()
        {
            CheckRegistrationAndOrUpdateValue(displayStatusFunc);
            return static_cast<ProjectReunion::DisplayStatus>(m_cachedDisplayStatus);
        }

        event_token DisplayStatusChanged(PowerEventHandler const& handler)
        {
            return AddCallback(displayStatusFunc, handler);
        }

        void DisplayStatusChanged(event_token const& token)
        {
            RemoveCallback(displayStatusFunc, token);
        }

        void DisplayStatusChanged_Callback(DWORD displayStatus)
        {
            m_cachedDisplayStatus = displayStatus;
            RaiseEvent(displayStatusFunc);
        }


        // SystemIdleStatus Functions
        ProjectReunion::SystemIdleStatus SystemIdleStatus()
        {
            // PReview: Should this be the default value?
            // We expect a persistently-queryable value, but
            // low-level APIs provide an idle->non-idle pulse event
 
            return SystemIdleStatus::Busy;
        }

        event_token SystemIdleStatusChanged(PowerEventHandler const& handler)
        {
            return AddCallback(systemIdleStatusFunc, handler);
        }

        void SystemIdleStatusChanged(event_token const& token)
        {
            RemoveCallback(systemIdleStatusFunc, token);
        }

        void SystemIdleStatusChanged_Callback()
        {
            RaiseEvent(systemIdleStatusFunc);
        }


        // PowerSchemePersonality Functions
        ProjectReunion::PowerSchemePersonality PowerSchemePersonality()
        {
            CheckRegistrationAndOrUpdateValue(powerSchemePersonalityFunc);
            if (m_cachedPowerSchemePersonality == GUID_MAX_POWER_SAVINGS)
            {
                return PowerSchemePersonality::PowerSaver;
            }
            else if (m_cachedPowerSchemePersonality == GUID_MIN_POWER_SAVINGS)
            {
                return PowerSchemePersonality::HighPerformance;
            }
            else
            {
                return PowerSchemePersonality::Balanced;
            }

        }

        event_token PowerSchemePersonalityChanged(PowerEventHandler const& handler)
        {
            return AddCallback(powerSchemePersonalityFunc, handler);
        }

        void PowerSchemePersonalityChanged(event_token const& token)
        {
            RemoveCallback(powerSchemePersonalityFunc, token);
        }

        void PowerSchemePersonalityChanged_Callback(GUID powerSchemePersonality)
        {
            m_cachedPowerSchemePersonality = powerSchemePersonality;
            RaiseEvent(powerSchemePersonalityFunc);
        }


        // UserPresenceStatus Functions
        ProjectReunion::UserPresenceStatus UserPresenceStatus()
        {
            CheckRegistrationAndOrUpdateValue(userPresenceStatusFunc);
            return static_cast<ProjectReunion::UserPresenceStatus>(m_cachedUserPresenceStatus);
        }

        event_token UserPresenceStatusChanged(PowerEventHandler const& handler)
        {
            return AddCallback(userPresenceStatusFunc, handler);
        }

        void UserPresenceStatusChanged(event_token const& token)
        {
            RemoveCallback(userPresenceStatusFunc, token);
        }

        void UserPresenceStatusChanged_Callback(DWORD userPresenceStatus)
        {
            m_cachedUserPresenceStatus = userPresenceStatus;
            RaiseEvent(userPresenceStatusFunc);
        }


        // SystemAwayModeStatus Functions
        ProjectReunion::SystemAwayModeStatus SystemAwayModeStatus()
        {
            CheckRegistrationAndOrUpdateValue(systemAwayModeStatusFunc);
            return static_cast<ProjectReunion::SystemAwayModeStatus>(m_cachedSystemAwayModeStatus);
        }

        event_token SystemAwayModeStatusChanged(PowerEventHandler const& handler)
        {
            return AddCallback(systemAwayModeStatusFunc, handler);
        }

        void SystemAwayModeStatusChanged(event_token const& token)
        {
            RemoveCallback(systemAwayModeStatusFunc, token);
        }

        void SystemAwayModeStatusChanged_Callback(DWORD systemAwayModeStatus)
        {
            m_cachedSystemAwayModeStatus = systemAwayModeStatus;
            RaiseEvent(systemAwayModeStatusFunc);
        }
#endif
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
