// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <mutex>
#include <PowerManager.g.h>
#include <PowerNotificationsPal.h>
#include <wil/resource.h>

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

    template<typename Parent, typename TValue>
    struct PowerManagerEvent
    {
        auto Value() const { return m_value; }
        fire_and_forget NotifyListeners(PowerManager const* sender);
        fire_and_forget UpdateValue(TValue value, PowerManager const* sender);
        operator bool() const { return static_cast<bool>(m_event); }

        event<PowerEventHandler> m_event;
    private:
        TValue m_value;
    };

    template<typename D>
    struct PowerCallbackT
    {
        D* Parent() { return static_cast<D*>(this); }
        PowerManager* Manager() { return static_cast<PowerManager*>(this); }

        template<typename Value>
        event_token EventProjection(PowerManagerEvent<D, Value>& powerEvent, PowerEventHandler const& handler)
        {
            auto lock = LockExclusive();
            if (!Parent()->m_registration)
            {
                Parent()->RefreshValues();
                Parent()->Register();
            }
            return powerEvent.m_event.add(handler);
        }

        template<typename Value>
        void EventProjection(PowerManagerEvent<D, Value>& powerEvent, event_token const& token)
        {
            powerEvent.m_event.remove(token);
            auto lock = LockExclusive();
            if (!Parent()->AreAnyHandlersRegistered())
            {
                Parent()->m_registration.reset();
            }
        }

        void UpdateValuesIfNecessary()
        {
            auto lock = LockExclusive();
            if (!Parent()->AreAnyHandlersRegistered())
            {
                Parent()->RefreshValues();
            }
        }

        template<typename Value>
        Value GetLatestValue(PowerManagerEvent<D, Value> const& powerEvent)
        {
            UpdateValuesIfNecessary();
            return powerEvent.Value();
        }

        // Common case where there is only one event.
        bool AreAnyHandlersRegistered()
        {
            return Parent()->m_event;
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

        auto LockExclusive()
        {
            return std::scoped_lock(m_mutex);
        }
    };

    struct EnergySaverPowerCallback : PowerCallbackT<EnergySaverPowerCallback>
    {
        using unique_registration = wil::unique_any<EnergySaverStatusRegistration, decltype(::UnregisterEnergySaverStatusChangedListener), ::UnregisterEnergySaverStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<EnergySaverPowerCallback, ProjectReunion::EnergySaverStatus> m_event;

        auto EnergySaverStatus() { return GetLatestValue(m_event); }
        template<typename Arg> auto EnergySaverStatusChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(::EnergySaverStatus value);
        void OnCallback(::EnergySaverStatus status);

    };

    struct CompositeBatteryPowerCallback : PowerCallbackT<CompositeBatteryPowerCallback>
    {
        using unique_registration = wil::unique_any<CompositeBatteryStatusRegistration, decltype(::UnregisterCompositeBatteryStatusChangedListener), ::UnregisterCompositeBatteryStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<CompositeBatteryPowerCallback, ProjectReunion::BatteryStatus> m_batteryStatusEvent;
        PowerManagerEvent<CompositeBatteryPowerCallback, ProjectReunion::PowerSupplyStatus> m_powerSupplyStatusEvent;
        PowerManagerEvent<CompositeBatteryPowerCallback, int32_t> m_remainingChargePercentEvent;
        bool AreAnyHandlersRegistered() { return m_batteryStatusEvent || m_powerSupplyStatusEvent || m_remainingChargePercentEvent; }

        static const auto UNKNOWN_BATTERY_PERCENT = 99999; // NEED TO CHOOSE AND DOCUMENT THIS VALUE

        auto BatteryStatus() { return GetLatestValue(m_batteryStatusEvent); }
        template<typename Arg> auto BatteryStatusChanged(Arg const& arg) { return EventProjection(m_batteryStatusEvent, arg); }

        auto PowerSupplyStatus() { return GetLatestValue(m_powerSupplyStatusEvent); }
        template<typename Arg> auto PowerSupplyStatusChanged(Arg const& arg) { return EventProjection(m_powerSupplyStatusEvent, arg); }

        auto RemainingChargePercent() { return GetLatestValue(m_remainingChargePercentEvent); }
        template<typename Arg> auto RemainingChargePercentChanged(Arg const& arg) { return EventProjection(m_remainingChargePercentEvent, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(CompositeBatteryStatus const& status);
        void OnCallback(CompositeBatteryStatus* compositeBatteryStatus);
    };

    struct DischargeTimePowerCallback : PowerCallbackT<DischargeTimePowerCallback>
    {
        using unique_registration = wil::unique_any<DischargeTimeRegistration, decltype(::UnregisterDischargeTimeChangedListener), ::UnregisterDischargeTimeChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<DischargeTimePowerCallback, Windows::Foundation::TimeSpan> m_event;

        auto RemainingDischargeTime() { return GetLatestValue(m_event); }
        template<typename Arg> auto RemainingDischargeTimeChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(ULONGLONG value);
        void OnCallback(ULONGLONG dischargeTimeOut);
    };
    
    struct PowerSourcePowerCallback : PowerCallbackT<PowerSourcePowerCallback>
    {
        using unique_registration = wil::unique_any<PowerConditionRegistration, decltype(::UnregisterPowerConditionChangedListener), ::UnregisterPowerConditionChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<PowerSourcePowerCallback, ProjectReunion::PowerSourceStatus> m_event;

        auto PowerSourceStatus() { return GetLatestValue(m_event); }
        template<typename Arg> auto PowerSourceStatusChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(DWORD value);
        void OnCallback(DWORD powerCondition);
    };

    struct DisplayStatusPowerCallback : PowerCallbackT<DisplayStatusPowerCallback>
    {
        using unique_registration = wil::unique_any<DisplayStatusRegistration, decltype(::UnregisterDisplayStatusChangedListener), ::UnregisterDisplayStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<DisplayStatusPowerCallback, ProjectReunion::DisplayStatus> m_event;

        auto DisplayStatus() { return GetLatestValue(m_event); }
        template<typename Arg> auto DisplayStatusChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(DWORD value);
        void OnCallback(DWORD displayStatus);
    };

    struct SystemIdleStatusPowerCallback : PowerCallbackT<SystemIdleStatusPowerCallback>
    {
        using unique_registration = wil::unique_any<SystemIdleStatusRegistration, decltype(::UnregisterSystemIdleStatusChangedListener), ::UnregisterSystemIdleStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<SystemIdleStatusPowerCallback, ProjectReunion::SystemIdleStatus> m_event;

        auto SystemIdleStatus() { return GetLatestValue(m_event); }
        template<typename Arg> auto SystemIdleStatusChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void OnCallback();
    };

    struct PowerSchemePersonalityPowerCallback : PowerCallbackT<PowerSchemePersonalityPowerCallback>
    {
        using unique_registration = wil::unique_any<PowerSchemePersonalityRegistration, decltype(::UnregisterPowerSchemePersonalityChangedListener), ::UnregisterPowerSchemePersonalityChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<PowerSchemePersonalityPowerCallback, ProjectReunion::PowerSchemePersonality> m_event;

        auto PowerSchemePersonality() { return GetLatestValue(m_event); }
        template<typename Arg> auto PowerSchemePersonalityChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(GUID const& value);
        void OnCallback(GUID const& value);
    };

    struct UserPresenceStatusPowerCallback : PowerCallbackT<UserPresenceStatusPowerCallback>
    {
        using unique_registration = wil::unique_any<UserPresenceStatusRegistration, decltype(::UnregisterUserPresenceStatusChangedListener), ::UnregisterUserPresenceStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<UserPresenceStatusPowerCallback, ProjectReunion::UserPresenceStatus> m_event;

        auto UserPresenceStatus() { return GetLatestValue(m_event); }
        template<typename Arg> auto UserPresenceStatusChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(DWORD value);
        void OnCallback(DWORD value);
    };

    struct SystemAwayModeStatusPowerCallback : PowerCallbackT<SystemAwayModeStatusPowerCallback>
    {
        using unique_registration = wil::unique_any<SystemAwayModeStatusRegistration, decltype(::UnregisterSystemAwayModeStatusChangedListener), ::UnregisterSystemAwayModeStatusChangedListener>;
        unique_registration m_registration;

        PowerManagerEvent<SystemAwayModeStatusPowerCallback, ProjectReunion::SystemAwayModeStatus> m_event;

        auto SystemAwayModeStatus() { return GetLatestValue(m_event); }
        template<typename Arg> auto SystemAwayModeStatusChanged(Arg const& arg) { return EventProjection(m_event, arg); }

        void Register();
        void RefreshValues();
        void UpdateValues(DWORD value);
        void OnCallback(DWORD value);
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
        using EnergySaverPowerCallback::EnergySaverStatus;
        using EnergySaverPowerCallback::EnergySaverStatusChanged;

        using CompositeBatteryPowerCallback::BatteryStatus;
        using CompositeBatteryPowerCallback::BatteryStatusChanged;
        using CompositeBatteryPowerCallback::PowerSupplyStatus;
        using CompositeBatteryPowerCallback::PowerSupplyStatusChanged;
        using CompositeBatteryPowerCallback::RemainingChargePercent;
        using CompositeBatteryPowerCallback::RemainingChargePercentChanged;

        using DischargeTimePowerCallback::RemainingDischargeTime;
        using DischargeTimePowerCallback::RemainingDischargeTimeChanged;

        using PowerSourcePowerCallback::PowerSourceStatus;
        using PowerSourcePowerCallback::PowerSourceStatusChanged;

        using DisplayStatusPowerCallback::DisplayStatus;
        using DisplayStatusPowerCallback::DisplayStatusChanged;

        using SystemIdleStatusPowerCallback::SystemIdleStatus;
        using SystemIdleStatusPowerCallback::SystemIdleStatusChanged;

        using PowerSchemePersonalityPowerCallback::PowerSchemePersonality;
        using PowerSchemePersonalityPowerCallback::PowerSchemePersonalityChanged;

        using UserPresenceStatusPowerCallback::UserPresenceStatus;
        using UserPresenceStatusPowerCallback::UserPresenceStatusChanged;

        using SystemAwayModeStatusPowerCallback::SystemAwayModeStatus;
        using SystemAwayModeStatusPowerCallback::SystemAwayModeStatusChanged;
    };
};

namespace winrt::Microsoft::ProjectReunion::implementation
{
     struct PowerManager
     {
        PowerManager() = delete;

        static auto Factory()
        {
            return make_self<factory_implementation::PowerManager>();
        }

        //Get function forwards
        static ProjectReunion::EnergySaverStatus EnergySaverStatus()
        {
            return Factory()->EnergySaverStatus();
        }

        static ProjectReunion::BatteryStatus BatteryStatus()
        {
            return Factory()->BatteryStatus();
        }

        static ProjectReunion::PowerSupplyStatus PowerSupplyStatus()
        {
            return Factory()->PowerSupplyStatus();
        }

        static int32_t RemainingChargePercent()
        {
            return Factory()->RemainingChargePercent();
        }

        static Windows::Foundation::TimeSpan RemainingDischargeTime()
        {
            return Factory()->RemainingDischargeTime();
        }

        static ProjectReunion::PowerSourceStatus PowerSourceStatus()
        {
            return Factory()->PowerSourceStatus();
        }

        static ProjectReunion::DisplayStatus DisplayStatus()
        {
            return Factory()->DisplayStatus();
        }

        static ProjectReunion::SystemIdleStatus SystemIdleStatus()
        {
            return Factory()->SystemIdleStatus();
        }

        static ProjectReunion::PowerSchemePersonality PowerSchemePersonality()
        {
            return Factory()->PowerSchemePersonality();
        }

        static ProjectReunion::UserPresenceStatus UserPresenceStatus()
        {
            return Factory()->UserPresenceStatus();
        }

        static ProjectReunion::SystemAwayModeStatus SystemAwayModeStatus()
        {
            return Factory()->SystemAwayModeStatus();
        }
    };
}
