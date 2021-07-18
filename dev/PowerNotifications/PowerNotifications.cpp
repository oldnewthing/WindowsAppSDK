// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include <pch.h>
#include <PowerNotifications.h>
#include <PowerManager.g.cpp>

namespace winrt::Microsoft::ProjectReunion::factory_implementation
{
    template<typename Parent, typename TValue>
    fire_and_forget PowerManagerEvent<Parent, TValue>::NotifyListeners(PowerManager const* sender)
    {
        if (sender && m_event)
        {
            Windows::Foundation::IInspectable lifetime = *sender; // extend lifetime into background thread
            co_await resume_background();
            m_event(nullptr, nullptr);
        }
    }

    template<typename Parent, typename TValue>
    fire_and_forget PowerManagerEvent<Parent, TValue>::UpdateValue(TValue value, PowerManager const* sender)
    {
        if (m_value != value)
        {
            m_value = value;
            this->NotifyListeners(sender);
        }
    };

#pragma region Energy Saver
    void EnergySaverPowerCallback::Register()
    {
        check_hresult(RegisterEnergySaverStatusChangedListener(MakeStaticCallback<&EnergySaverPowerCallback::OnCallback>(), &m_registration));
    }

    void EnergySaverPowerCallback::RefreshValues()
    {
        ::EnergySaverStatus status;
        check_hresult(::GetEnergySaverStatus(&status));
        UpdateValues(status);
    }

    void EnergySaverPowerCallback::OnCallback(::EnergySaverStatus status)
    {
        auto lock = LockExclusive();
        UpdateValues(status);
    }

    void EnergySaverPowerCallback::UpdateValues(::EnergySaverStatus status)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::EnergySaverStatus>(status), Manager());
    }
#pragma endregion

#pragma region Composite battery events
    void CompositeBatteryPowerCallback::Register()
    {
            check_hresult(RegisterCompositeBatteryStatusChangedListener(MakeStaticCallback<&CompositeBatteryPowerCallback::OnCallback>(), &m_registration));
    }

    void CompositeBatteryPowerCallback::RefreshValues()
    {
        CompositeBatteryStatus* compositeBatteryStatus;
        check_hresult(GetCompositeBatteryStatus(&compositeBatteryStatus));
        auto cleanup = wil::scope_exit([&] {}); // TODO: how to free the memory?
        UpdateValues(*compositeBatteryStatus);
    }

    void CompositeBatteryPowerCallback::OnCallback(CompositeBatteryStatus* compositeBatteryStatus)
    {
        auto lock = LockExclusive();
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
        m_remainingChargePercentEvent.UpdateValue(GetBatteryChargePercent(compositeBatteryStatus), Manager());
        m_batteryStatusEvent.UpdateValue(GetBatteryStatus(compositeBatteryStatus), Manager());
        m_powerSupplyStatusEvent.UpdateValue(GetPowerSupplyStatus(compositeBatteryStatus), Manager());
    }
#pragma endregion

#pragma region Remaining discharge time
    void DischargeTimePowerCallback::Register()
    {
        check_hresult(RegisterDischargeTimeChangedListener(MakeStaticCallback<&DischargeTimePowerCallback::OnCallback>(), &m_registration));
    }

    void DischargeTimePowerCallback::RefreshValues()
    {
        ULONGLONG dischargeTime;
        check_hresult(GetDischargeTime(&dischargeTime));
        UpdateValues(dischargeTime);
    }

    void DischargeTimePowerCallback::OnCallback(ULONGLONG dischargeTime)
    {
        auto lock = LockExclusive();
        UpdateValues(dischargeTime);
    }

    void DischargeTimePowerCallback::UpdateValues(ULONGLONG dischargeTime)
    {
        m_event.UpdateValue(std::chrono::seconds(dischargeTime), Manager());
    }
#pragma endregion

#pragma region Power source
    void PowerSourcePowerCallback::Register()
    {
        check_hresult(RegisterPowerConditionChangedListener(MakeStaticCallback<&PowerSourcePowerCallback::OnCallback>(), &m_registration));
    }

    void PowerSourcePowerCallback::RefreshValues()
    {
        DWORD value;
        check_hresult(::GetPowerCondition(&value));
        UpdateValues(value);
    }

    void PowerSourcePowerCallback::OnCallback(DWORD value)
    {
        auto lock = LockExclusive();
        UpdateValues(value);
    }

    void PowerSourcePowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::PowerSourceStatus>(value), Manager());
    }
#pragma endregion

#pragma region Display status
    void DisplayStatusPowerCallback::Register()
    {
        check_hresult(RegisterDisplayStatusChangedListener(MakeStaticCallback<&DisplayStatusPowerCallback::OnCallback>(), &m_registration));
    }

    void DisplayStatusPowerCallback::RefreshValues()
    {
        DWORD value;
        check_hresult(::GetDisplayStatus(&value));
        UpdateValues(value);
    }

    void DisplayStatusPowerCallback::OnCallback(DWORD value)
    {
        auto lock = LockExclusive();
        UpdateValues(value);
    }

    void DisplayStatusPowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::DisplayStatus>(value), Manager());
    }
#pragma endregion

#pragma region System idle status
    void SystemIdleStatusPowerCallback::Register()
    {
        check_hresult(RegisterSystemIdleStatusChangedListener(MakeStaticCallback<&SystemIdleStatusPowerCallback::OnCallback>(), &m_registration));
    }

    void SystemIdleStatusPowerCallback::RefreshValues()
    {
        // There is no way to query the value.
        // PReview: Should this be the default value?
        // We expect a persistently-queryable value, but
        // low-level APIs provide an idle->non-idle pulse event
        m_event.UpdateValue(SystemIdleStatus::Busy, Manager());
    }

    void SystemIdleStatusPowerCallback::OnCallback()
    {
        auto lock = LockExclusive();
        m_event.NotifyListeners(Manager());
    }
#pragma endregion

#pragma region Power scheme personality
    void PowerSchemePersonalityPowerCallback::Register()
    {
        check_hresult(RegisterPowerSchemePersonalityChangedListener(MakeStaticCallback<&PowerSchemePersonalityPowerCallback::OnCallback>(), &m_registration));
    }

    void PowerSchemePersonalityPowerCallback::RefreshValues()
    {
        GUID value;
        check_hresult(::GetPowerSchemePersonality(&value));
        UpdateValues(value);
    }

    void PowerSchemePersonalityPowerCallback::OnCallback(GUID const& value)
    {
        auto lock = LockExclusive();
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
        m_event.UpdateValue(personality, Manager());
    }

#pragma endregion

#pragma region User presence status
    void UserPresenceStatusPowerCallback::Register()
    {
        check_hresult(RegisterUserPresenceStatusChangedListener(MakeStaticCallback<&UserPresenceStatusPowerCallback::OnCallback>(), &m_registration));
    }

    void UserPresenceStatusPowerCallback::RefreshValues()
    {
        DWORD value;
        check_hresult(::GetUserPresenceStatus(&value));
        UpdateValues(value);
    }

    void UserPresenceStatusPowerCallback::OnCallback(DWORD value)
    {
        auto lock = LockExclusive();
        UpdateValues(value);
    }

    void UserPresenceStatusPowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::UserPresenceStatus>(value), Manager());
    }

#pragma endregion

#pragma region System away mode status
    void SystemAwayModeStatusPowerCallback::Register()
    {
        check_hresult(RegisterSystemAwayModeStatusChangedListener(MakeStaticCallback<&SystemAwayModeStatusPowerCallback::OnCallback>(), &m_registration));
    }

    void SystemAwayModeStatusPowerCallback::RefreshValues()
    {
        DWORD value;
        check_hresult(::GetSystemAwayModeStatus(&value));
        UpdateValues(value);
    }

    void SystemAwayModeStatusPowerCallback::OnCallback(DWORD value)
    {
        auto lock = LockExclusive();
        UpdateValues(value);
    }

    void SystemAwayModeStatusPowerCallback::UpdateValues(DWORD value)
    {
        m_event.UpdateValue(static_cast<ProjectReunion::SystemAwayModeStatus>(value), Manager());
    }
#pragma endregion

}
