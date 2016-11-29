#include "Engine/Private/UWP/PlatformCoreUWP.h"

#if defined(__DAVAENGINE_COREV2__)
#if defined(__DAVAENGINE_WIN_UAP__)

#include "Base/Exception.h"
#include "Engine/Window.h"
#include "Engine/Private/EngineBackend.h"
#include "Engine/Private/Dispatcher/MainDispatcherEvent.h"
#include "Engine/Private/UWP/Window/WindowBackendUWP.h"
#include "Engine/Private/UWP/DllImportUWP.h"

#include "Debug/Backtrace.h"
#include "Platform/SystemTimer.h"
#include "Concurrency/LockGuard.h"
#include "Concurrency/Thread.h"
#include "Logger/Logger.h"
#include "Utils/Utils.h"
#include "Platform/DeviceInfo.h"

extern int DAVAMain(DAVA::Vector<DAVA::String> cmdline);

namespace DAVA
{
namespace Private
{
bool PlatformCore::isPhoneContractPresent = false;

PlatformCore::PlatformCore(EngineBackend* engineBackend_)
    : engineBackend(engineBackend_)
    , dispatcher(engineBackend->GetDispatcher())
{
    using ::Windows::Foundation::Metadata::ApiInformation;
    isPhoneContractPresent = ApiInformation::IsApiContractPresent("Windows.Phone.PhoneContract", 1);

    if (!isPhoneContractPresent)
    {
        DllImportUWP::Initialize();
    }
}

PlatformCore::~PlatformCore() = default;

void PlatformCore::Init()
{
}

void PlatformCore::Run()
{
    if (savedActivatedEventArgs != nullptr)
    {
        // Here notify listeners about OnLaunched or OnActivated occured before entering game loop.
        // Notification will come on first frame
        engineBackend->GetPrimaryWindow()->RunOnUIThreadAsync([ this, args = savedActivatedEventArgs ]() {
            using ::Windows::ApplicationModel::Activation::ActivationKind;
            NotifyListeners(args->Kind == ActivationKind::Launch ? ON_LAUNCHED : ON_ACTIVATED, args);
        });
        savedActivatedEventArgs = nullptr;
    }

    engineBackend->OnGameLoopStarted();

    while (!quitGameThread)
    {
        uint64 frameBeginTime = SystemTimer::Instance()->AbsoluteMS();

        int32 fps = engineBackend->OnFrame();

        uint64 frameEndTime = SystemTimer::Instance()->AbsoluteMS();
        uint32 frameDuration = static_cast<uint32>(frameEndTime - frameBeginTime);

        int32 sleep = 1;
        if (fps > 0)
        {
            sleep = 1000 / fps - frameDuration;
            if (sleep < 1)
                sleep = 1;
        }
        Sleep(sleep);
    }

    engineBackend->OnGameLoopStopped();
    engineBackend->OnEngineCleanup();
}

void PlatformCore::PrepareToQuit()
{
    engineBackend->PostAppTerminate(true);
}

void PlatformCore::Quit()
{
    quitGameThread = true;
}

void PlatformCore::OnLaunchedOrActivated(::Windows::ApplicationModel::Activation::IActivatedEventArgs ^ args)
{
    using namespace ::Windows::ApplicationModel::Activation;

    ApplicationExecutionState prevExecState = args->PreviousExecutionState;
    if (prevExecState != ApplicationExecutionState::Running && prevExecState != ApplicationExecutionState::Suspended)
    {
        Thread* gameThread = Thread::Create(MakeFunction(this, &PlatformCore::GameThread));
        gameThread->Start();
        gameThread->BindToProcessor(0);
        // TODO: make Thread detachable
        //gameThread->Detach();
        //gameThread->Release();

        // Save activated event arguments to notify listeners later just before entering game loop to ensure
        // that dava.engine and game have intialized and listeners have had chance to register.
        savedActivatedEventArgs = args;
    }
    else
    {
        NotifyListeners(args->Kind == ActivationKind::Launch ? ON_LAUNCHED : ON_ACTIVATED, args);
    }
}

void PlatformCore::OnWindowCreated(::Windows::UI::Xaml::Window ^ xamlWindow)
{
    // TODO: think about binding XAML window to prior created Window instance
    Window* primaryWindow = engineBackend->GetPrimaryWindow();
    if (primaryWindow == nullptr)
    {
        primaryWindow = engineBackend->InitializePrimaryWindow();
    }
    WindowBackend* windowBackend = primaryWindow->GetBackend();
    windowBackend->BindXamlWindow(xamlWindow);
}

void PlatformCore::OnSuspending()
{
    dispatcher->SendEvent(MainDispatcherEvent(MainDispatcherEvent::APP_SUSPENDED)); // Blocking call !!!
}

void PlatformCore::OnResuming()
{
    dispatcher->PostEvent(MainDispatcherEvent(MainDispatcherEvent::APP_RESUMED));
}

void PlatformCore::OnUnhandledException(::Windows::UI::Xaml::UnhandledExceptionEventArgs ^ arg)
{
    Logger::Error("Unhandled exception: hresult=0x%08X, message=%s", arg->Exception, WStringToString(arg->Message->Data()).c_str());
}

void PlatformCore::OnBackPressed()
{
    dispatcher->PostEvent(MainDispatcherEvent(MainDispatcherEvent::BACK_NAVIGATION));
}

void PlatformCore::OnGamepadAdded(::Windows::Gaming::Input::Gamepad ^ /*gamepad*/)
{
    dispatcher->PostEvent(MainDispatcherEvent::CreateGamepadAddedEvent(0));
}

void PlatformCore::OnGamepadRemoved(::Windows::Gaming::Input::Gamepad ^ /*gamepad*/)
{
    dispatcher->PostEvent(MainDispatcherEvent::CreateGamepadRemovedEvent(0));
}

void PlatformCore::OnDpiChanged()
{
    engineBackend->UpdateDisplayConfig();
}

void PlatformCore::RegisterXamlApplicationListener(PlatformApi::Win10::XamlApplicationListener* listener)
{
    DVASSERT(listener != nullptr);

    using std::begin;
    using std::end;

    LockGuard<Mutex> lock(listenersMutex);
    auto it = std::find(begin(xamlApplicationListeners), end(xamlApplicationListeners), listener);
    if (it == end(xamlApplicationListeners))
    {
        xamlApplicationListeners.push_back(listener);
    }
}

void PlatformCore::UnregisterXamlApplicationListener(PlatformApi::Win10::XamlApplicationListener* listener)
{
    using std::begin;
    using std::end;

    LockGuard<Mutex> lock(listenersMutex);
    auto it = std::find(begin(xamlApplicationListeners), end(xamlApplicationListeners), listener);
    if (it != end(xamlApplicationListeners))
    {
        xamlApplicationListeners.erase(it);
    }
}

void PlatformCore::GameThread()
{
    try
    {
        DAVAMain(engineBackend->GetCommandLine());
    }
    catch (const Exception& e)
    {
        StringStream ss;
        ss << "!!! Unhandled DAVA::Exception at `" << e.file << "`: " << e.line << std::endl;
        ss << Debug::GetBacktraceString(e.callstack) << std::endl;
        Logger::PlatformLog(Logger::LEVEL_ERROR, ss.str().c_str());
        throw;
    }
    catch (const std::exception& e)
    {
        StringStream ss;
        ss << "!!! Unhandled std::exception in DAVAMain: " << e.what() << std::endl;
        Logger::PlatformLog(Logger::LEVEL_ERROR, ss.str().c_str());
        throw;
    }

    using namespace ::Windows::UI::Xaml;
    Application::Current->Exit();
}

void PlatformCore::NotifyListeners(eNotificationType type, ::Platform::Object ^ arg1)
{
    using ::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs;
    using ::Windows::ApplicationModel::Activation::IActivatedEventArgs;

    Vector<PlatformApi::Win10::XamlApplicationListener*> listenersCopy;
    {
        // Make copy to allow listeners unregistering inside a callback
        LockGuard<Mutex> lock(listenersMutex);
        listenersCopy.resize(xamlApplicationListeners.size());
        std::copy(xamlApplicationListeners.begin(), xamlApplicationListeners.end(), listenersCopy.begin());
    }
    for (PlatformApi::Win10::XamlApplicationListener* l : listenersCopy)
    {
        switch (type)
        {
        case ON_LAUNCHED:
            l->OnLaunched(static_cast<LaunchActivatedEventArgs ^>(arg1));
            break;
        case ON_ACTIVATED:
            l->OnActivated(static_cast<IActivatedEventArgs ^>(arg1));
            break;
        default:
            break;
        }
    }
}

} // namespace Private
} // namespace DAVA

#endif // __DAVAENGINE_WIN_UAP__
#endif // __DAVAENGINE_COREV2__
