// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include <TestDef.h>
#include "MockBackgroundTaskInstance.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Background;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Management::Deployment;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::System;

#include <windows.h> 
#include <MddBootstrap.h> 
#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/Microsoft.Windows.PushNotifications.h>
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Windows::AppLifecycle;
using namespace winrt::Microsoft::Windows::PushNotifications;

winrt::guid remoteId1(L"a2e4a323-b518-4799-9e80-0b37aeb0d225"); // Generated from ms.portal.azure.com
winrt::guid remoteId2(L"CA1A4AB2-AC1D-4EFC-A132-E5A191CA285A"); // Dummy guid from visual studio guid tool generator

PushNotificationRegistrationToken g_appToken = nullptr;

constexpr auto timeout{ std::chrono::seconds(300) };

HRESULT ChannelRequestHelper(IAsyncOperationWithProgress<PushNotificationCreateChannelResult, PushNotificationCreateChannelStatus> const& channelOperation)
{
    if (channelOperation.wait_for(timeout) != winrt::Windows::Foundation::AsyncStatus::Completed)
    {
        channelOperation.Cancel();
        return HRESULT_FROM_WIN32(ERROR_TIMEOUT); // timed out or failed
    }

    auto result = channelOperation.GetResults();
    auto status = result.Status();
    if (status != PushNotificationChannelStatus::CompletedSuccess)
    {
        return result.ExtendedError(); // did not produce a channel
    }

    result.Channel().Close();
    return S_OK;
}

namespace Test::PushNotifications
{
    class APITests
    {
    private:
        wil::unique_event m_failed;
        wil::unique_process_handle m_processHandle;
        winrt::com_ptr<IApplicationActivationManager> m_testAppLauncher;

    public:
        BEGIN_TEST_CLASS(APITests)
            TEST_CLASS_PROPERTY(L"Description", L"Project Reunion Push Notifications test")
            TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
            TEST_CLASS_PROPERTY(L"RunAs:Class", L"RestrictedUser")
        END_TEST_CLASS()

        static const int testWaitTime()
        {
            return 3000;
        }

        static const int channelTestWaitTime()
        {
            return 303000; // Need to wait 300000ms for channel request + 3000ms for application overhead
        }

        static PCWSTR GetTestPackageFile()
        {
            return L"PushNotificationsTestAppPackage";
        }

        static PCWSTR GetTestPackageFullName()
        {
            return L"PushNotificationsTestAppPackage_1.0.0.0_" PROJECTREUNION_TEST_PACKAGE_DDLM_ARCHITECTURE L"__8wekyb3d8bbwe";
        }

        TEST_CLASS_SETUP(ClassInit)
        {
            try
            {
                TP::AddPackage_ProjectReunionFramework(); // Installs PRfwk
                TP::AddPackage_DynamicDependencyLifetimeManager();
                TP::AddPackage_PushNotificationsLongRunningTask(); // Installs the PushNotifications long running task.
                TP::WapProj::AddPackage(TAEF::GetDeploymentDir(), GetTestPackageFile(), L".msix"); // Installs PushNotificationsTestApp.msix
            }
            catch (...)
            {
                return false;
            }

            m_testAppLauncher = winrt::create_instance<IApplicationActivationManager>(CLSID_ApplicationActivationManager, CLSCTX_ALL);
            return true;
        }

        TEST_CLASS_CLEANUP(ClassUninit)
        {
            try
            {
                TP::RemovePackage(GetTestPackageFullName());
                TP::RemovePackage_PushNotificationsLongRunningTask();
                TP::RemovePackage_DynamicDependencyLifetimeManager();
                TP::RemovePackage_ProjectReunionFramework();
            }
            catch (...)
            {
                return false;
            }
            return true;
        }

        TEST_METHOD_SETUP(MethodInit)
        {
            VERIFY_IS_TRUE(TP::IsPackageRegistered_ProjectReunionFramework());
            VERIFY_IS_TRUE(TP::IsPackageRegistered_PushNotificationsLongRunningTask());
            return true;
        }

        TEST_METHOD_CLEANUP(MethodUninit)
        {
            VERIFY_IS_TRUE(TP::IsPackageRegistered_ProjectReunionFramework());
            VERIFY_IS_TRUE(TP::IsPackageRegistered_PushNotificationsLongRunningTask());

            m_processHandle.reset();
            return true;
        }

        void RunTest(const PCWSTR& testName, const int& waitTime)
        {
            DWORD processId {};
            VERIFY_SUCCEEDED(m_testAppLauncher->ActivateApplication(L"PushNotificationsTestAppPackage_8wekyb3d8bbwe!App", testName, AO_NONE, &processId));

            m_processHandle.reset(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
            VERIFY_IS_TRUE(m_processHandle.is_valid());

            VERIFY_IS_TRUE(wil::handle_wait(m_processHandle.get(), waitTime));

            DWORD exitCode {};
            VERIFY_WIN32_BOOL_SUCCEEDED(GetExitCodeProcess(m_processHandle.get(), &exitCode));
            VERIFY_ARE_EQUAL(exitCode, 0);
        }

        TEST_METHOD(BackgroundActivation)
        {
            RunTest(L"BackgroundActivationTest", testWaitTime()); // Need to launch one time to enable background activation.

            auto LocalBackgroundTask = winrt::create_instance<winrt::Windows::ApplicationModel::Background::IBackgroundTask>(c_comServerId, CLSCTX_ALL);
            auto mockBackgroundTaskInstance = winrt::make<MockBackgroundTaskInstance>();
            VERIFY_NO_THROW(LocalBackgroundTask.Run(mockBackgroundTaskInstance));
        }

        TEST_METHOD(MultipleBackgroundActivation)
        {
            RunTest(L"BackgroundActivationTest", testWaitTime()); // Need to launch one time to enable background activation.

            auto LocalBackgroundTask1 = winrt::create_instance<winrt::Windows::ApplicationModel::Background::IBackgroundTask>(c_comServerId, CLSCTX_ALL);
            auto mockBackgroundTaskInstance1 = winrt::make<MockBackgroundTaskInstance>();

            auto LocalBackgroundTask2 = winrt::create_instance<winrt::Windows::ApplicationModel::Background::IBackgroundTask>(c_comServerId, CLSCTX_ALL);
            auto mockBackgroundTaskInstance2 = winrt::make<MockBackgroundTaskInstance>();

            VERIFY_NO_THROW(LocalBackgroundTask1.Run(mockBackgroundTaskInstance1));
            VERIFY_NO_THROW(LocalBackgroundTask2.Run(mockBackgroundTaskInstance2));

        }

        TEST_METHOD(ChannelRequestUsingNullRemoteId)
        {
            RunTest(L"ChannelRequestUsingNullRemoteId", testWaitTime());
        }

        TEST_METHOD(ChannelRequestUsingRemoteId)
        {
            RunTest(L"ChannelRequestUsingRemoteId", channelTestWaitTime());
        }

        TEST_METHOD(UnpackagedChannelRequestUsingRemoteId)
        {
            PACKAGE_VERSION version{};
            version.Major = 4;
            version.Minor = 1;
            version.Build = 1967;
            version.Revision = 333;

            const UINT32 majorMinorVersion = static_cast<UINT32>((version.Major << 16) | version.Minor);
            //const UINT32 majorMinorVersion{ 0x00040001 };
            //const UINT32 majorMinorVersion{ 0x00000008 };
            PCWSTR versionTag{};
            const PACKAGE_VERSION minVersion{};
            const HRESULT hr{ MddBootstrapInitialize(majorMinorVersion, nullptr, minVersion) };

            // Check the return code for errors. If there is an error, display the result.
            if (FAILED(hr))
            {
                wprintf(L"Error 0x%X in MddBootstrapInitialize(0x%08X, %s, %hu.%hu.%hu.%hu)\n",
                    hr, majorMinorVersion, versionTag, minVersion.Major, minVersion.Minor, minVersion.Build, minVersion.Revision);
                //return hr;
            }

            //RunTest(L"BackgroundActivationTest", testWaitTime()); // Need to launch one time to enable background activation.

            //auto LocalBackgroundTask = winrt::create_instance<winrt::Windows::ApplicationModel::Background::IBackgroundTask>(c_comServerId, CLSCTX_ALL);
            //auto mockBackgroundTaskInstance = winrt::make<MockBackgroundTaskInstance>();
            //VERIFY_NO_THROW(LocalBackgroundTask.Run(mockBackgroundTaskInstance));

            //auto args = AppInstance::GetCurrent().GetActivatedEventArgs();
            //auto kind = args.Kind();

            //PushNotificationActivationInfo info(
            //    PushNotificationRegistrationOptions::PushTrigger | PushNotificationRegistrationOptions::ComActivator,
            //    winrt::guid(c_comServerId)); // same clsid as app manifest

            //g_appToken = PushNotificationManager::RegisterActivator(info);

            //RunTest(L"UnpackagedChannelRequestUsingRemoteId", channelTestWaitTime());
            auto channelOperation = PushNotificationManager::CreateChannelAsync(remoteId1);
            auto channelOperationResult = ChannelRequestHelper(channelOperation);

            // Release the DDLM and clean up.
            MddBootstrapShutdown();

            //return channelOperationResult == S_OK;
            VERIFY_ARE_EQUAL(channelOperationResult, S_OK);
        }

        TEST_METHOD(MultipleChannelClose)
        {
            RunTest(L"MultipleChannelClose", channelTestWaitTime());
        }

        TEST_METHOD(MultipleChannelRequestUsingSameRemoteId)
        {
            RunTest(L"MultipleChannelRequestUsingSameRemoteId", channelTestWaitTime());
        }

        TEST_METHOD(MultipleChannelRequestUsingMultipleRemoteId)
        {
            RunTest(L"MultipleChannelRequestUsingMultipleRemoteId", channelTestWaitTime());
        }

        TEST_METHOD(ActivatorTest)
        {
            RunTest(L"ActivatorTest", testWaitTime());
        }

        TEST_METHOD(RegisterActivatorNullDetails)
        {
            RunTest(L"RegisterActivatorNullDetails", testWaitTime());
        }

        TEST_METHOD(RegisterActivatorNullClsid)
        {
            RunTest(L"RegisterActivatorNullClsid", testWaitTime());
        }

        TEST_METHOD(UnregisterActivatorNullToken)
        {
            RunTest(L"UnregisterActivatorNullToken", testWaitTime());
        }

        TEST_METHOD(UnregisterActivatorNullBackgroundRegistration)
        {
            RunTest(L"UnregisterActivatorNullBackgroundRegistration", testWaitTime());
        }

        TEST_METHOD(MultipleRegisterActivatorTest)
        {
            RunTest(L"MultipleRegisterActivatorTest", testWaitTime());
        }

    };
}
