#include "Notification/Private/Win10/NativeListenerWin10.h"

#if defined(__DAVAENGINE_COREV2__)
#if defined(__DAVAENGINE_WIN_UAP__)

#include "Notification/LocalNotificationController.h"

#include "Engine/Engine.h"
#include "Logger/Logger.h"
#include "Utils/UTF8Utils.h"
namespace DAVA
{
namespace Private
{
NativeListener::NativeListener(LocalNotificationController& controller)
    : localNotificationController(controller)
{
    PlatformApi::Win10::RegisterXamlApplicationListener(this);
}

NativeListener::~NativeListener()
{
    PlatformApi::Win10::UnregisterXamlApplicationListener(this);
}

void NativeListener::OnLaunched(::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^ launchArgs)
{
    using namespace DAVA;
    String arguments = UTF8Utils::EncodeToUTF8(launchArgs->Arguments->Data());
    if (launchArgs->Kind == Windows::ApplicationModel::Activation::ActivationKind::Launch)
    {
        Platform::String ^ launchString = launchArgs->Arguments;
        if (!arguments.empty())
        {
            String uidStr = UTF8Utils::EncodeToUTF8(launchString->Data());
            auto function = [this, arguments]()
            {
                localNotificationController.OnNotificationPressed(arguments);
            };
            RunOnMainThread(function);
        }
    }
}
} // namespace Private
} // namespace DAVA
#endif // __DAVAENGINE_WIN_UAP__
#endif // __DAVAENGINE_COREV2__
