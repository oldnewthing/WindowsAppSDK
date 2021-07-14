// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include <pch.h>
#include <PowerNotifications.h>
#include <PowerManager.g.cpp>

namespace winrt::Microsoft::ProjectReunion::factory_implementation
{
    fire_and_forget PowerManagerEventBase::NotifyListeners(PowerManager const* sender)
    {
        if (sender && m_event)
        {
            Windows::Foundation::IInspectable lifetime = *sender; // extend lifetime into background thread
            co_await resume_background();
            m_event(nullptr, nullptr);
        }
    }

    template<typename TValue>
    fire_and_forget PowerManagerEvent<TValue>::UpdateValue(TValue value, PowerManager const* sender)
    {
        if (m_value != value)
        {
            m_value = value;
            NotifyListeners(sender);
        }
    };

    event_token PowerCallbackBase::AddCallback(PowerManagerEventBase& e, PowerEventHandler const& handler)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        e.m_callback.Register();
        return e.m_event.add(handler);
    }

    void PowerCallbackBase::RemoveCallback(PowerManagerEventBase& e, event_token const& token)
    {
        e.m_event.remove(token);
        std::scoped_lock<std::mutex> lock(m_mutex);
        e.m_callback.Unregister();
    }

    // Checks if an event is already registered. If none are, then gets the status
    void PowerCallbackBase::UpdateValueIfNecessary(PowerManagerEventBase& e)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        if (!e.m_event)
        {
            e.m_callback.RefreshValue();
        }
    }

#pragma region Energy Saver
    ProjectReunion::EnergySaverStatus PowerManager::EnergySaverStatus()
    {
        return EnergySaverPowerCallback::GetLatestValue(EnergySaverPowerCallback::m_event);
    }

    event_token PowerManager::EnergySaverStatusChanged(PowerEventHandler const& handler)
    {
        return EnergySaverPowerCallback::AddCallback(EnergySaverPowerCallback::m_event, handler);
    }

    void PowerManager::EnergySaverStatusChanged(event_token const& token)
    {
        EnergySaverPowerCallback::RemoveCallback(EnergySaverPowerCallback::m_event, token);
    }

    void EnergySaverPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterEnergySaverStatusChangedListener(PowerManager::MakeStaticCallback<&EnergySaverPowerCallback::OnCallback>(), &m_registration));
        }
    }

    void EnergySaverPowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void EnergySaverPowerCallback::RefreshValue()
    {
        ::EnergySaverStatus status;
        check_hresult(::GetEnergySaverStatus(&status));
        UpdateValues(status);
    }

    void EnergySaverPowerCallback::OnCallback(::EnergySaverStatus status)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(status);
    }

    void EnergySaverPowerCallback::UpdateValues(::EnergySaverStatus status)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::EnergySaverStatus>(status), static_cast<PowerManager*>(this));
    }
#pragma endregion

#pragma region Composite battery events
#pragma region public APIs
    ProjectReunion::BatteryStatus PowerManager::BatteryStatus()
    {
        return CompositeBatteryPowerCallback::GetLatestValue(CompositeBatteryPowerCallback::m_batteryStatusEvent);
    }

    event_token PowerManager::BatteryStatusChanged(PowerEventHandler const& handler)
    {
        return CompositeBatteryPowerCallback::AddCallback(CompositeBatteryPowerCallback::m_batteryStatusEvent, handler);
    }

    void PowerManager::BatteryStatusChanged(event_token const& token)
    {
        return CompositeBatteryPowerCallback::RemoveCallback(CompositeBatteryPowerCallback::m_batteryStatusEvent, token);
    }

    ProjectReunion::PowerSupplyStatus PowerManager::PowerSupplyStatus()
    {
        return CompositeBatteryPowerCallback::GetLatestValue(CompositeBatteryPowerCallback::m_powerSupplyStatusEvent);
    }

    event_token PowerManager::PowerSupplyStatusChanged(PowerEventHandler const& handler)
    {
        return CompositeBatteryPowerCallback::AddCallback(CompositeBatteryPowerCallback::m_powerSupplyStatusEvent, handler);
    }

    void PowerManager::PowerSupplyStatusChanged(event_token const& token)
    {
        return CompositeBatteryPowerCallback::RemoveCallback(CompositeBatteryPowerCallback::m_powerSupplyStatusEvent, token);
    }

    int32_t PowerManager::RemainingChargePercent()
    {
        return CompositeBatteryPowerCallback::GetLatestValue(CompositeBatteryPowerCallback::m_remainingChargePercentEvent);
    }

    event_token PowerManager::RemainingChargePercentChanged(PowerEventHandler const& handler)
    {
        return CompositeBatteryPowerCallback::AddCallback(CompositeBatteryPowerCallback::m_remainingChargePercentEvent, handler);
    }

    void PowerManager::RemainingChargePercentChanged(event_token const& token)
    {
        return CompositeBatteryPowerCallback::RemoveCallback(CompositeBatteryPowerCallback::m_remainingChargePercentEvent, token);
    }
#pragma endregion public APIs

    void CompositeBatteryPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterCompositeBatteryStatusChangedListener(PowerManager::MakeStaticCallback<&CompositeBatteryPowerCallback::OnCallback>(), &m_registration));
        }
    }

    void CompositeBatteryPowerCallback::Unregister()
    {
        bool isCallbackNeeded = m_batteryStatusEvent || m_powerSupplyStatusEvent || m_remainingChargePercentEvent;
        if (!isCallbackNeeded)
        {
            m_registration.reset();
        }
    }

    void CompositeBatteryPowerCallback::RefreshValue()
    {
        CompositeBatteryStatus* compositeBatteryStatus;
        check_hresult(GetCompositeBatteryStatus(&compositeBatteryStatus));
        auto cleanup = wil::scope_exit([&] {}); // TODO: how to free the memory?
        UpdateValues(*compositeBatteryStatus);
    }

    void CompositeBatteryPowerCallback::OnCallback(CompositeBatteryStatus* compositeBatteryStatus)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(*compositeBatteryStatus);
    }

    namespace
    {
        int32_t GetBatteryChargePercent(CompositeBatteryStatus const& compositeBatteryStatus)
        {
            // Calculate the remaining charge capacity based on the maximum charge
           // as an integer percentage value from 0 to 100.
           //
           // If the battery does not report enough information to calculate the remaining charge,
           // then report a remaining charge of UNKNOWN_BATTERY_PERCENT.
            auto fullChargedCapacity = compositeBatteryStatus.Information.FullChargedCapacity;
            auto remainingCapacity = compositeBatteryStatus.Status.Capacity;
            if (fullChargedCapacity == BATTERY_UNKNOWN_CAPACITY ||
                fullChargedCapacity == 0 ||
                remainingCapacity == BATTERY_UNKNOWN_CAPACITY)
            {
                return CompositeBatteryPowerCallback::UNKNOWN_BATTERY_PERCENT;
            }
            else if (remainingCapacity > fullChargedCapacity)
            {
                return 100;
            }
            else
            {
                // round to nearest percent
                return static_cast<int>((static_cast<uint64_t>(remainingCapacity) * 200 + 1) / fullChargedCapacity) / 2;
            }
        }

        ProjectReunion::BatteryStatus GetBatteryStatus(CompositeBatteryStatus const& compositeBatteryStatus)
        {
            auto powerState = compositeBatteryStatus.Status.PowerState;
            if (compositeBatteryStatus.ActiveBatteryCount == 0)
            {
                return BatteryStatus::NotPresent;
            }
            else if (WI_IsFlagSet(powerState, BATTERY_DISCHARGING))
            {
                return BatteryStatus::Discharging;
            }
            else if (WI_IsFlagSet(powerState, BATTERY_CHARGING))
            {
                return BatteryStatus::Charging;
            }
            else
            {
                return BatteryStatus::Idle;
            }
        }

        ProjectReunion::PowerSupplyStatus GetPowerSupplyStatus(CompositeBatteryStatus const& compositeBatteryStatus)
        {
            auto powerState = compositeBatteryStatus.Status.PowerState;
            if (WI_IsFlagClear(powerState, BATTERY_POWER_ON_LINE))
            {
                return PowerSupplyStatus::NotPresent;
            }
            else if (WI_IsFlagSet(powerState, BATTERY_DISCHARGING))
            {
                return PowerSupplyStatus::Inadequate;
            }
            else
            {
                return PowerSupplyStatus::Adequate;
            }
        }
    }

    void CompositeBatteryPowerCallback::UpdateValues(CompositeBatteryStatus const& compositeBatteryStatus)
    {
        m_remainingChargePercentEvent.UpdateValue(GetBatteryChargePercent(compositeBatteryStatus), static_cast<PowerManager*>(this));
        m_batteryStatusEvent.UpdateValue(GetBatteryStatus(compositeBatteryStatus), static_cast<PowerManager*>(this));
        m_powerSupplyStatusEvent.UpdateValue(GetPowerSupplyStatus(compositeBatteryStatus), static_cast<PowerManager*>(this));
    }
#pragma endregion

#pragma region Remaining discharge time

    Windows::Foundation::TimeSpan PowerManager::RemainingDischargeTime()
    {
        return DischargeTimePowerCallback::GetLatestValue(DischargeTimePowerCallback::m_event);
    }

    event_token PowerManager::RemainingDischargeTimeChanged(PowerEventHandler const& handler)
    {
        return DischargeTimePowerCallback::AddCallback(DischargeTimePowerCallback::m_event, handler);
    }

    void PowerManager::RemainingDischargeTimeChanged(event_token const& token)
    {
        DischargeTimePowerCallback::RemoveCallback(DischargeTimePowerCallback::m_event, token);
    }


    void DischargeTimePowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterDischargeTimeChangedListener(PowerManager::MakeStaticCallback<&DischargeTimePowerCallback::OnCallback>(), &m_registration));
        }
    }

    void DischargeTimePowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void DischargeTimePowerCallback::RefreshValue()
    {
        ULONGLONG dischargeTime;
        check_hresult(GetDischargeTime(&dischargeTime));
        UpdateValues(dischargeTime);
    }

    void DischargeTimePowerCallback::OnCallback(ULONGLONG dischargeTime)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(dischargeTime);
    }

    void DischargeTimePowerCallback::UpdateValues(ULONGLONG dischargeTime)
    {
        m_event.UpdateValue(std::chrono::seconds(dischargeTime), static_cast<PowerManager*>(this));
    }
#pragma endregion

#pragma region Power source
    ProjectReunion::PowerSourceStatus PowerManager::PowerSourceStatus()
    {
        return PowerSourcePowerCallback::GetLatestValue(PowerSourcePowerCallback::m_event);
    }

    event_token PowerManager::PowerSourceStatusChanged(PowerEventHandler const& handler)
    {
        return PowerSourcePowerCallback::AddCallback(PowerSourcePowerCallback::m_event, handler);
    }

    void PowerManager::PowerSourceStatusChanged(event_token const& token)
    {
        PowerSourcePowerCallback::RemoveCallback(PowerSourcePowerCallback::m_event, token);
    }

    void PowerSourcePowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterPowerConditionChangedListener(PowerManager::MakeStaticCallback<&PowerSourcePowerCallback::OnCallback>(), &m_registration));
        }
    }

    void PowerSourcePowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void PowerSourcePowerCallback::RefreshValue()
    {
        DWORD value;
        check_hresult(::GetPowerCondition(&value));
        UpdateValues(value);
    }

    void PowerSourcePowerCallback::OnCallback(DWORD value)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(value);
    }

    void PowerSourcePowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::PowerSourceStatus>(value), static_cast<PowerManager*>(this));
    }
#pragma endregion

#pragma region Display status
    ProjectReunion::DisplayStatus PowerManager::DisplayStatus()
    {
        return DisplayStatusPowerCallback::GetLatestValue(DisplayStatusPowerCallback::m_event);
    }

    event_token PowerManager::DisplayStatusChanged(PowerEventHandler const& handler)
    {
        return DisplayStatusPowerCallback::AddCallback(DisplayStatusPowerCallback::m_event, handler);
    }

    void PowerManager::DisplayStatusChanged(event_token const& token)
    {
        DisplayStatusPowerCallback::RemoveCallback(DisplayStatusPowerCallback::m_event, token);
    }

    void DisplayStatusPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterDisplayStatusChangedListener(PowerManager::MakeStaticCallback<&DisplayStatusPowerCallback::OnCallback>(), &m_registration));
        }
    }

    void DisplayStatusPowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void DisplayStatusPowerCallback::RefreshValue()
    {
        DWORD value;
        check_hresult(::GetDisplayStatus(&value));
        UpdateValues(value);
    }

    void DisplayStatusPowerCallback::OnCallback(DWORD value)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(value);
    }

    void DisplayStatusPowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::DisplayStatus>(value), static_cast<PowerManager*>(this));
    }
#pragma endregion

#pragma region System idle status
    ProjectReunion::SystemIdleStatus PowerManager::SystemIdleStatus()
    {
        // PReview: Should this be the default value?
        // We expect a persistently-queryable value, but
        // low-level APIs provide an idle->non-idle pulse event

        return SystemIdleStatus::Busy;
    }

    event_token PowerManager::SystemIdleStatusChanged(PowerEventHandler const& handler)
    {
        return SystemIdleStatusPowerCallback::AddCallback(SystemIdleStatusPowerCallback::m_event, handler);
    }

    void PowerManager::SystemIdleStatusChanged(event_token const& token)
    {
        SystemIdleStatusPowerCallback::RemoveCallback(SystemIdleStatusPowerCallback::m_event, token);
    }

    void SystemIdleStatusPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterSystemIdleStatusChangedListener(PowerManager::MakeStaticCallback<&SystemIdleStatusPowerCallback::OnCallback>(), &m_registration));
        }
    }

    void SystemIdleStatusPowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void SystemIdleStatusPowerCallback::RefreshValue()
    {
        // There is no way to query the value.
    }

    void SystemIdleStatusPowerCallback::OnCallback()
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_event.NotifyListeners(static_cast<PowerManager*>(this));
    }
#pragma endregion

#pragma region Power scheme personality
    ProjectReunion::PowerSchemePersonality PowerManager::PowerSchemePersonality()
    {
        return PowerSchemePersonalityPowerCallback::GetLatestValue(PowerSchemePersonalityPowerCallback::m_event);
    }

    event_token PowerManager::PowerSchemePersonalityChanged(PowerEventHandler const& handler)
    {
        return PowerSchemePersonalityPowerCallback::AddCallback(PowerSchemePersonalityPowerCallback::m_event, handler);
    }

    void PowerManager::PowerSchemePersonalityChanged(event_token const& token)
    {
        PowerSchemePersonalityPowerCallback::RemoveCallback(PowerSchemePersonalityPowerCallback::m_event, token);
    }

    void PowerSchemePersonalityPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterPowerSchemePersonalityChangedListener(PowerManager::MakeStaticCallback<&PowerSchemePersonalityPowerCallback::OnCallback>(), &m_registration));
        }
    }

    void PowerSchemePersonalityPowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void PowerSchemePersonalityPowerCallback::RefreshValue()
    {
        GUID value;
        check_hresult(::GetPowerSchemePersonality(&value));
        UpdateValues(value);
    }

    void PowerSchemePersonalityPowerCallback::OnCallback(GUID const& value)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(value);
    }

    void PowerSchemePersonalityPowerCallback::UpdateValues(GUID const& value)
    {
        ProjectReunion::PowerSchemePersonality personality;
        if (value == GUID_MAX_POWER_SAVINGS)
        {
            personality = PowerSchemePersonality::PowerSaver;
        }
        else if (value == GUID_MIN_POWER_SAVINGS)
        {
            personality = PowerSchemePersonality::HighPerformance;
        }
        else
        {
            personality = PowerSchemePersonality::Balanced;
        }
        m_event.UpdateValue(personality, static_cast<PowerManager*>(this));
    }

#pragma endregion

#pragma region User presence status
    ProjectReunion::UserPresenceStatus PowerManager::UserPresenceStatus()
    {
        return UserPresenceStatusPowerCallback::GetLatestValue(UserPresenceStatusPowerCallback::m_event);
    }

    event_token PowerManager::UserPresenceStatusChanged(PowerEventHandler const& handler)
    {
        return UserPresenceStatusPowerCallback::AddCallback(UserPresenceStatusPowerCallback::m_event, handler);
    }

    void PowerManager::UserPresenceStatusChanged(event_token const& token)
    {
        UserPresenceStatusPowerCallback::RemoveCallback(UserPresenceStatusPowerCallback::m_event, token);
    }

    void UserPresenceStatusPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterUserPresenceStatusChangedListener(PowerManager::MakeStaticCallback<&UserPresenceStatusPowerCallback::OnCallback>(), &m_registration));
        }
    }

    void UserPresenceStatusPowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void UserPresenceStatusPowerCallback::RefreshValue()
    {
        DWORD value;
        check_hresult(::GetUserPresenceStatus(&value));
        UpdateValues(value);
    }

    void UserPresenceStatusPowerCallback::OnCallback(DWORD value)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(value);
    }

    void UserPresenceStatusPowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::UserPresenceStatus>(value), static_cast<PowerManager*>(this));
    }

#pragma endregion

#pragma region System away mode status
    ProjectReunion::SystemAwayModeStatus PowerManager::SystemAwayModeStatus()
    {
        return SystemAwayModeStatusPowerCallback::GetLatestValue(SystemAwayModeStatusPowerCallback::m_event);
    }

    event_token PowerManager::SystemAwayModeStatusChanged(PowerEventHandler const& handler)
    {
        return SystemAwayModeStatusPowerCallback::AddCallback(SystemAwayModeStatusPowerCallback::m_event, handler);
    }

    void PowerManager::SystemAwayModeStatusChanged(event_token const& token)
    {
        SystemAwayModeStatusPowerCallback::RemoveCallback(SystemAwayModeStatusPowerCallback::m_event, token);
    }

    void SystemAwayModeStatusPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterSystemAwayModeStatusChangedListener(PowerManager::MakeStaticCallback<&SystemAwayModeStatusPowerCallback::OnCallback>(), &m_registration));
        }
    }

    void SystemAwayModeStatusPowerCallback::Unregister()
    {
        if (!m_event)
        {
            m_registration.reset();
        }
    }

    void SystemAwayModeStatusPowerCallback::RefreshValue()
    {
        DWORD value;
        check_hresult(::GetSystemAwayModeStatus(&value));
        UpdateValues(value);
    }

    void SystemAwayModeStatusPowerCallback::OnCallback(DWORD value)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        UpdateValues(value);
    }

    void SystemAwayModeStatusPowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::SystemAwayModeStatus>(value), static_cast<PowerManager*>(this));
    }

#pragma endregion

}
