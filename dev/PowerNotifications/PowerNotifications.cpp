// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include <pch.h>
#include <PowerNotifications.h>
#include <PowerManager.g.cpp>

namespace winrt::Microsoft::ProjectReunion::factory_implementation
{
    fire_and_forget PowerManagerEventBase::NotifyListeners()
    {
        if (m_event)
        {
            Windows::Foundation::IInspectable lifetime = *m_callback.m_powerManager; // extend lifetime into background thread
            co_await resume_background();
            m_event(nullptr, nullptr);
        }
    }

    template<typename TValue>
    fire_and_forget PowerManagerEvent<TValue>::UpdateValue(TValue value)
    {
        if (m_value != value)
        {
            m_value = value;
            NotifyListeners();
        }
    };

#pragma region Energy Saver
#pragma region public APIs
    ProjectReunion::EnergySaverStatus PowerManager::EnergySaverStatus()
    {
        return GetLatestValue(m_energySaverStatusEvent);
    }

    event_token PowerManager::EnergySaverStatusChanged(PowerEventHandler const& handler)
    {
        return AddCallback(m_energySaverStatusEvent, handler);
    }

    void PowerManager::EnergySaverStatusChanged(event_token const& token)
    {
        RemoveCallback(m_energySaverStatusEvent, token);
    }

#pragma endregion

    void PowerManager::EnergySaverPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterEnergySaverStatusChangedListener(PowerManager::MakeStaticCallback<&PowerManager::EnergySaver_Callback>(), &m_registration));
        }
    }

    void PowerManager::EnergySaverPowerCallback::Unregister()
    {
        if (!m_powerManager->m_energySaverStatusEvent)
        {
            m_registration.reset();
        }
    }

    void PowerManager::EnergySaverPowerCallback::RefreshValue()
    {
        ::EnergySaverStatus status;
        check_hresult(::GetEnergySaverStatus(&status));
        UpdateValues(status);
    }

    void PowerManager::EnergySaver_Callback(::EnergySaverStatus status)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_energySaverPowerCallback.UpdateValues(status);
    }

    void PowerManager::EnergySaverPowerCallback::UpdateValues(::EnergySaverStatus status)
    {
        m_powerManager->m_energySaverStatusEvent.UpdateValue(static_cast<ProjectReunion::EnergySaverStatus>(status));
    }
#pragma endregion

#pragma region Composite battery events
#pragma region public APIs
    ProjectReunion::BatteryStatus PowerManager::BatteryStatus()
    {
        return GetLatestValue(m_batteryStatusEvent);
    }

    event_token PowerManager::BatteryStatusChanged(PowerEventHandler const& handler)
    {
        return AddCallback(m_batteryStatusEvent, handler);
    }

    void PowerManager::BatteryStatusChanged(event_token const& token)
    {
        return RemoveCallback(m_batteryStatusEvent, token);
    }

    ProjectReunion::PowerSupplyStatus PowerManager::PowerSupplyStatus()
    {
        return GetLatestValue(m_powerSupplyStatusEvent);
    }

    event_token PowerManager::PowerSupplyStatusChanged(PowerEventHandler const& handler)
    {
        return AddCallback(m_powerSupplyStatusEvent, handler);
    }

    void PowerManager::PowerSupplyStatusChanged(event_token const& token)
    {
        return RemoveCallback(m_powerSupplyStatusEvent, token);
    }

    int32_t PowerManager::RemainingChargePercent()
    {
        return GetLatestValue(m_remainingChargePercentEvent);
    }

    event_token PowerManager::RemainingChargePercentChanged(PowerEventHandler const& handler)
    {
        return AddCallback(m_remainingChargePercentEvent, handler);
    }

    void PowerManager::RemainingChargePercentChanged(event_token const& token)
    {
        return RemoveCallback(m_remainingChargePercentEvent, token);
    }
#pragma endregion public APIs

    void PowerManager::CompositeBatteryPowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterCompositeBatteryStatusChangedListener(PowerManager::MakeStaticCallback<&PowerManager::CompositeBatteryStatusChanged_Callback>(), &m_registration));
        }
    }

    void PowerManager::CompositeBatteryPowerCallback::Unregister()
    {
        bool isCallbackNeeded = m_powerManager->m_batteryStatusEvent || m_powerManager->m_powerSupplyStatusEvent || m_powerManager->m_remainingChargePercentEvent;
        if (!isCallbackNeeded)
        {
            m_registration.reset();
        }
    }

    void PowerManager::CompositeBatteryPowerCallback::RefreshValue()
    {
        CompositeBatteryStatus* compositeBatteryStatus;
        check_hresult(GetCompositeBatteryStatus(&compositeBatteryStatus));
        auto cleanup = wil::scope_exit([&] {}); // TODO: how to free the memory?
        UpdateValues(*compositeBatteryStatus);
    }

    void PowerManager::CompositeBatteryStatusChanged_Callback(CompositeBatteryStatus* compositeBatteryStatus)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_compositeBatteryPowerCallback.UpdateValues(*compositeBatteryStatus);
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
                return PowerManager::UNKNOWN_BATTERY_PERCENT;
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

    void PowerManager::CompositeBatteryPowerCallback::UpdateValues(CompositeBatteryStatus const& compositeBatteryStatus)
    {
        m_powerManager->m_remainingChargePercentEvent.UpdateValue(GetBatteryChargePercent(compositeBatteryStatus));
        m_powerManager->m_batteryStatusEvent.UpdateValue(GetBatteryStatus(compositeBatteryStatus));
        m_powerManager->m_powerSupplyStatusEvent.UpdateValue(GetPowerSupplyStatus(compositeBatteryStatus));
    }

#pragma endregion

#pragma region Remaining discharge time
#pragma region public APIs
    Windows::Foundation::TimeSpan PowerManager::RemainingDischargeTime()
    {
        return GetLatestValue(m_remainingDischargeTimeEvent);
    }

    event_token PowerManager::RemainingDischargeTimeChanged(PowerEventHandler const& handler)
    {
        return AddCallback(m_remainingDischargeTimeEvent, handler);
    }

    void PowerManager::RemainingDischargeTimeChanged(event_token const& token)
    {
        RemoveCallback(m_remainingDischargeTimeEvent, token);
    }

#pragma endregion

    void PowerManager::DischargeTimePowerCallback::Register()
    {
        if (!m_registration)
        {
            RefreshValue();
            check_hresult(RegisterDischargeTimeChangedListener(PowerManager::MakeStaticCallback<&PowerManager::DischargeTime_Callback>(), &m_registration));
        }
    }

    void PowerManager::DischargeTimePowerCallback::Unregister()
    {
        if (!m_powerManager->m_remainingDischargeTimeEvent)
        {
            m_registration.reset();
        }
    }

    void PowerManager::DischargeTimePowerCallback::RefreshValue()
    {
        ULONGLONG dischargeTime;
        check_hresult(GetDischargeTime(&dischargeTime));
        UpdateValues(dischargeTime);
    }

    void PowerManager::DischargeTime_Callback(ULONGLONG dischargeTime)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_dischargeTimePowerCallback.UpdateValues(dischargeTime);
    }

    void PowerManager::DischargeTimePowerCallback::UpdateValues(ULONGLONG dischargeTime)
    {
        m_powerManager->m_remainingDischargeTimeEvent.UpdateValue(std::chrono::seconds(dischargeTime));
    }
#pragma endregion

#if 0
    // PowerSourceStatus Functions
    EventType& PowerSourceStatus_Event()
    {
        return factory->m_powerSourceStatusChangedEvent;
    }

    void PowerSourceStatus_Register()
    {
        check_hresult(RegisterPowerConditionChangedListener(&PowerManager::PowerSourceStatusChanged_Callback, &factory->m_powerSourceStatusHandle));
    }

    void PowerSourceStatus_Unregister()
    {
        check_hresult(UnregisterPowerConditionChangedListener(factory->m_powerSourceStatusHandle));
    }

    void PowerSourceStatus_Update()
    {
        check_hresult(GetPowerCondition(&factory->m_cachedPowerSourceStatus));
    }
   
    // DisplayStatus Functions
    EventType& DisplayStatus_Event()
    {
        return factory->m_displayStatusChangedEvent;
    }

    void DisplayStatus_Register()
    {
        check_hresult(RegisterDisplayStatusChangedListener(&PowerManager::DisplayStatusChanged_Callback, &factory->m_displayStatusHandle));
    }

    void DisplayStatus_Unregister()
    {
        check_hresult(UnregisterDisplayStatusChangedListener(factory->m_displayStatusHandle));
    }

    void DisplayStatus_Update()
    {
        check_hresult(GetDisplayStatus(&factory->m_cachedDisplayStatus));
    }
   
    // SystemIdleStatus Functions
    EventType& SystemIdleStatus_Event()
    {
        return factory->m_systemIdleStatusChangedEvent;
    }

    void SystemIdleStatus_Register()
    {
        check_hresult(RegisterSystemIdleStatusChangedListener(&PowerManager::SystemIdleStatusChanged_Callback, &factory->m_systemIdleStatusHandle));
    }

    void SystemIdleStatus_Unregister()
    {
        check_hresult(UnregisterSystemIdleStatusChangedListener(factory->m_systemIdleStatusHandle));
    }
   
    // PowerSchemePersonality Functions
    EventType& PowerSchemePersonality_Event()
    {
        return factory->m_powerSchemePersonalityChangedEvent;
    }

    void PowerSchemePersonality_Register()
    {
        check_hresult(RegisterPowerSchemePersonalityChangedListener(&PowerManager::PowerSchemePersonalityChanged_Callback, &factory->m_powerSchemePersonalityHandle));
    }

    void PowerSchemePersonality_Unregister()
    {
        check_hresult(UnregisterPowerSchemePersonalityChangedListener(factory->m_powerSchemePersonalityHandle));
    }

    void PowerSchemePersonality_Update()
    {
        check_hresult(GetPowerSchemePersonality(&factory->m_cachedPowerSchemePersonality));
    }
   
    // UserPresenceStatus Functions
    EventType& UserPresenceStatus_Event()
    {
        return factory->m_userPresenceStatusChangedEvent;
    }

    void UserPresenceStatus_Register()
    {
        check_hresult(RegisterUserPresenceStatusChangedListener(&PowerManager::UserPresenceStatusChanged_Callback, &factory->m_userPresenceStatusHandle));
    }

    void UserPresenceStatus_Unregister()
    {
        check_hresult(UnregisterUserPresenceStatusChangedListener(factory->m_userPresenceStatusHandle));
    }

    void UserPresenceStatus_Update()
    {
        check_hresult(GetUserPresenceStatus(&factory->m_cachedUserPresenceStatus));
    }

    // SystemAwayModeStatus Functions
    EventType& SystemAwayModeStatus_Event()
    {
        return factory->m_systemAwayModeStatusChangedEvent;
    }

    void SystemAwayModeStatus_Register()
    {
        check_hresult(RegisterSystemAwayModeStatusChangedListener(&PowerManager::SystemAwayModeStatusChanged_Callback, &factory->m_systemAwayModeStatusHandle));
    }

    void SystemAwayModeStatus_Unregister()
    {
        check_hresult(UnregisterSystemAwayModeStatusChangedListener(factory->m_systemAwayModeStatusHandle));
    }

    void SystemAwayModeStatus_Update()
    {
        check_hresult(GetSystemAwayModeStatus(&factory->m_cachedSystemAwayModeStatus));
    }

}

PowerFunctionDetails powerSupplyStatusFunc{
    &PowerManager::PowerSupplyStatus_Event,
    &PowerManager::PowerSupplyStatus_Register,
    &PowerManager::PowerSupplyStatus_Unregister,
    &PowerManager::PowerSupplyStatus_Update };

PowerFunctionDetails remainingChargePercentFunc{
    &PowerManager::RemainingChargePercent_Event,
    &PowerManager::RemainingChargePercent_Register,
    &PowerManager::RemainingChargePercent_Unregister,
    &PowerManager::RemainingChargePercent_Update };

PowerFunctionDetails remainingDischargeTimeFunc{
    &PowerManager::RemainingDischargeTime_Event,
    &PowerManager::RemainingDischargeTime_Register,
    &PowerManager::RemainingDischargeTime_Unregister,
    &PowerManager::RemainingDischargeTime_Update };

PowerFunctionDetails powerSourceStatusFunc{
    &PowerManager::PowerSourceStatus_Event,
    &PowerManager::PowerSourceStatus_Register,
    &PowerManager::PowerSourceStatus_Unregister,
    &PowerManager::PowerSourceStatus_Update };

PowerFunctionDetails displayStatusFunc{
    &PowerManager::DisplayStatus_Event,
    &PowerManager::DisplayStatus_Register,
    &PowerManager::DisplayStatus_Unregister,
    &PowerManager::DisplayStatus_Update };

PowerFunctionDetails systemIdleStatusFunc{
    &PowerManager::SystemIdleStatus_Event,
    &PowerManager::SystemIdleStatus_Register,
    &PowerManager::SystemIdleStatus_Unregister,
    &PowerManager::NoOperation };

PowerFunctionDetails powerSchemePersonalityFunc{
    &PowerManager::PowerSchemePersonality_Event,
    &PowerManager::PowerSchemePersonality_Register,
    &PowerManager::PowerSchemePersonality_Unregister,
    &PowerManager::PowerSchemePersonality_Update };

PowerFunctionDetails userPresenceStatusFunc{
    &PowerManager::UserPresenceStatus_Event,
    &PowerManager::UserPresenceStatus_Register,
    &PowerManager::UserPresenceStatus_Unregister,
    &PowerManager::UserPresenceStatus_Update };

PowerFunctionDetails systemAwayModeStatusFunc{
    &PowerManager::SystemAwayModeStatus_Event,
    &PowerManager::SystemAwayModeStatus_Register,
    &PowerManager::SystemAwayModeStatus_Unregister,
    &PowerManager::SystemAwayModeStatus_Update };
namespace winrt::Microsoft::ProjectReunion::factory_implementation
{
#endif

    event_token PowerManager::AddCallback(PowerManagerEvent& e, PowerEventHandler const& handler)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        e.m_details.Register();
        return e.m_event.add(handler);
    }

    void PowerManager::RemoveCallback(PowerManagerEvent& e, event_token const& token)
    {
        e.m_event.remove(token);
        std::scoped_lock<std::mutex> lock(m_mutex);
        if (!e.m_details.HasListeners())
        {
            e.m_details.Unregister();
        }
    }

    // Checks if an event is already registered. If none are, then gets the status
    void PowerManager::UpdateValueIfNecessary(PowerManagerEvent& e)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        if (!e.m_event)
        {
            e.m_details.GetLatestValue();
        }
    }

#pragma region Energy Saver data
    struct PowerManagerEnergySaverData : PowerManagerDetailsBase
    {
        ::EnergySaverStatus m_cachedStatus{ ::EnergySaverStatus::Disabled }; // ??? default value?
        EnergySaverStatusRegistration m_registrationHandle{ nullptr };

        PowerManagerEnergySaverData() = default;
        ~PowerManagerEnergySaverData();

        void Register() final override;
        void Unregister() final override;
        void Update() final override;
        fire_and_forget HandleCallback(std::shared_ptr<PowerManagerData> lifetime, ::EnergySaverStatus energySaverStatus);
        static void StatusChanged_Callback(::EnergySaverStatus energySaverStatus);
    };

    struct PowerManagerBatteryStatusData : PowerManagerDetailsBase
    {
        static const auto UNKNOWN_BATTERY_PERCENT = 99999; // NEED TO CHOOSE AND DOCUMENT THIS VALUE

        int m_batteryChargePercent;
        int m_oldBatteryChargePercent = UNKNOWN_BATTERY_PERCENT;
        ProjectReunion::BatteryStatus m_batteryStatus;
        ProjectReunion::BatteryStatus m_oldBatteryStatus{ ProjectReunion::BatteryStatus::NotPresent };
        ProjectReunion::PowerSupplyStatus m_powerSupplyStatus;
        ProjectReunion::PowerSupplyStatus m_oldPowerSupplyStatus{ ProjectReunion::PowerSupplyStatus::NotPresent };

        CompositeBatteryStatus* m_cachedCompositeBatteryStatus{ nullptr };
        CompositeBatteryStatusRegistration m_registrationHandle{ nullptr };

        PowerManagerBatteryStatusData() = default;
        ~PowerManagerBatteryStatusData();

        void Register() final override;
        void Unregister() final override;
        void Update() final override;
        void ProcessStatus(CompositeBatteryStatus const& compositeBatteryStatus);
        fire_and_forget HandleCallback(std::shared_ptr<PowerManagerData> lifetime, CompositeBatteryStatus* compositeBatteryStatus);
        static void StatusChanged_Callback(CompositeBatteryStatus* compositeBatteryStatus);
    };

    struct PowerManagerData
    {
        PowerManagerEnergySaverData energySaver;
        PowerManagerBatteryStatusData batteryStatus;
    };
#pragma endregion

#pragma region Energy Saver
    ProjectReunion::EnergySaverStatus PowerManager::EnergySaverStatus()
    {
        UpdateValueIfNecessary(m_data->energySaver);
        return static_cast<ProjectReunion::EnergySaverStatus>(m_data->energySaver.m_cachedEnergySaverStatus);
    }

    event_token PowerManager::EnergySaverStatusChanged(PowerEventHandler const& handler)
    {
        return AddCallback(m_data->energySaver, handler);
    }

    void PowerManager::EnergySaverStatusChanged(event_token const& token)
    {
        RemoveCallback(m_data->energySaver, token);
    }

    PowerManagerEnergySaverData::~PowerManagerEnergySaverData()
    {
        if (m_EnergySaverStatusHandle)
        {
            Unregister();
        }
    }

    void PowerManagerEnergySaverData::Register()
    {
        check_hresult(RegisterEnergySaverStatusChangedListener(&PowerManagerEnergySaver::StatusChanged_Callback, &m_registrationHandle));
    }

    void PowerManagerEnergySaverData::Unregister()
    {
        check_hresult(UnregisterEnergySaverStatusChangedListener(std::exchange(m_EnergySaverStatusHandle, {})));
    }

    void PowerManagerEnergySaverData::Update()
    {
        check_hresult(GetEnergySaverStatus(&m_cachedEnergySaverStatus));
    }

    fire_and_forget PowerManagerEnergySaverData::HandleCallback(std::shared_ptr<PowerManagerData> lifetime, ::EnergySaverStatus energySaverStatus)
    {
        // "lifetime" parameter prevents us from being destructed prematurely.
        m_cachedStatus = energySaverStatus;
        co_await resume_background();
        m_event(nullptr, nullptr);
    }

    fire_and_forget PowerManagerEnergySaverData::EnergySaverStatusChanged_Callback(::EnergySaverStatus energySaverStatus)
    {        
        if (auto data = PowerManager::s_weak.lock())
        {
            data->energySaver.HandleCallback(data, energySaverStatus);
        }
    }
#pragma endregion

#pragma region Battery Status
    ProjectReunion::BatteryStatus PowerManager::BatteryStatus()
    {
        UpdateValueIfNecessary(m_data->batteryStatus);
        return m_data->batteryStatus.m_batteryStatus;
    }

    void PowerManagerBatteryStatusData::Update()
    {
        check_hresult(GetCompositeBatteryStatus(&m_cachedCompositeBatteryStatus)); // race condition here?
        ProcessStatus(*m_cachedCompositeBatteryStatus); // race condition here?
    }

    void PowerManagerBatteryStatusData::ProcessStatus(CompositeBatteryStatus const& compositeBatteryStatus)
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
            m_batteryChargePercent = UNKNOWN_BATTERY_PERCENT;
        }
        else if (remainingCapacity > fullChargedCapacity)
        {
            m_batteryChargePercent = 100;
        }
        else
        {
            // round to nearest percent
            m_batteryChargePercent = static_cast<int>((static_cast<uint64_t>(remainingCapacity) * 200 + 1) / fullChargedCapacity) / 2;
        }

        auto powerState = compositeBatteryStatus.Status.PowerState;

        // Set battery status
        if (compositeBatteryStatus.ActiveBatteryCount == 0)
        {
            m_batteryStatus = BatteryStatus::NotPresent;
        }
        else if (WI_IsFlagSet(powerState, BATTERY_DISCHARGING))
        {
            m_batteryStatus = BatteryStatus::Discharging;
        }
        else if (WI_IsFlagSet(powerState, BATTERY_CHARGING))
        {
            m_batteryStatus = BatteryStatus::Charging;
        }
        else
        {
            m_batteryStatus = BatteryStatus::Idle;
        }

        // Set power supply state
        if (WI_IsFlagClear(powerState, BATTERY_POWER_ON_LINE))
        {
            m_powerSupplyStatus = PowerSupplyStatus::NotPresent;
        }
        else if (WI_IsFlagSet(powerState, BATTERY_DISCHARGING))
        {
            m_powerSupplyStatus = PowerSupplyStatus::Inadequate;
        }
        else
        {
            m_powerSupplyStatus = PowerSupplyStatus::Adequate;
        }
    }

    fire_and_forget PowerManagerBatteryStatusData::HandleCallback(std::shared_ptr<PowerManagerData> lifetime, CompositeBatteryStatus* compositeBatteryStatus)
    {
            ProcessCompositeBatteryStatus(*compositeBatteryStatus);
            co_await resume_background();
            // Raise the relevant events
            if (m_oldBatteryChargePercent != m_batteryChargePercent)
            {
                m_oldBatteryChargePercent = m_batteryChargePercent;
                $RaiseEvent(remainingChargePercentFunc);
            }

            if (m_oldBatteryStatus != m_batteryStatus)
            {
                m_oldBatteryStatus = m_batteryStatus;
                RaiseEvent(compositeBatteryStatusFunc);
            }

            if (m_oldPowerSupplyStatus != m_powerSupplyStatus)
            {
                m_oldPowerSupplyStatus = m_powerSupplyStatus;
                RaiseEvent(powerSupplyStatusFunc);
            }
        }
    }

    void PowerManagerBatteryStatusData::StatusChanged_Callback(CompositeBatteryStatus* compositeBatteryStatus)
    {
        if (auto data = PowerManager::s_weak.lock())
        {
            data->batteryStatus.HandleCallback(data, compositeBatteryStatus);
        }
    }

#pragma endregion

}
