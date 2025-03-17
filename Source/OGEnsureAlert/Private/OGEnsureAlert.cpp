/// Copyright Occam's Gamekit contributors 2025

#include "OGEnsureAlert.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "FOGEnsureAlertModule"

class FEnsureNotifierOutputDevice : public FOutputDevice
{
    
public:
    FEnsureNotifierOutputDevice()
        : FOutputDevice()
    {
        GLog->AddOutputDevice(this);
    }

    virtual ~FEnsureNotifierOutputDevice()
    {
        if (GLog)
        {
            GLog->RemoveOutputDevice(this);
        }
    }

    virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        // Only process ensure failures in editor
        if (!GIsEditor || Verbosity != ELogVerbosity::Error)
        {
            return;
        }

        FRegexMatcher Matcher(EnsureMessagePattern, Message);
        if (!Matcher.FindNext())
            return;

        const FString EnsureMessage = Matcher.GetCaptureGroup(1);

        // Post notification to the game thread
        AsyncTask(ENamedThreads::GameThread, [this, EnsureMessage]()
        {
            FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Ensure Failed\n%s"), *EnsureMessage)));
            
            // Configure notification for manual dismissal
            Info.bUseLargeFont = false;
            Info.bUseSuccessFailIcons = true;
            Info.bUseThrobber = true;
            Info.bFireAndForget = false;
            
            // Set durations to 0 to prevent auto-fadeout
            Info.FadeOutDuration = 1.0f;
            Info.ExpireDuration = 0.0f;
            
            Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));

            const int NotificationKey = NextNotificationKey++;

            // Add "Open in Log" button
            Info.ButtonDetails.Add(FNotificationButtonInfo(
                LOCTEXT("OpenInLogButton", "Open in Log"),
                LOCTEXT("OpenInLogButtonTooltip", "Open the Output Log to see the full error"),
                FSimpleDelegate::CreateLambda([this, NotificationKey]()
                {
                    FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog"));

                    if (!ActiveNotifications.Contains(NotificationKey))
                        return;
                    const TSharedPtr<SNotificationItem> Notification = ActiveNotifications.FindAndRemoveChecked(NotificationKey);
                    if (!Notification.IsValid())
                        return;
                    Notification->Fadeout();
                }),
                SNotificationItem::ECompletionState::CS_Fail
            ));

            // Add dismiss button
            Info.ButtonDetails.Add(FNotificationButtonInfo(
                LOCTEXT("DismissButton", "Dismiss"),
                LOCTEXT("DismissButtonTooltip", "Dismiss this notification"),
                FSimpleDelegate::CreateLambda([this, NotificationKey]()
                {
                    if (!ActiveNotifications.Contains(NotificationKey))
                        return;
                    const TSharedPtr<SNotificationItem> Notification = ActiveNotifications.FindAndRemoveChecked(NotificationKey);
                    if (!Notification.IsValid())
                        return;
                    Notification->Fadeout();
                }),
                SNotificationItem::ECompletionState::CS_Fail)
            );
            const TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
            if (NotificationItem.IsValid())
            {
                NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
            }
            ActiveNotifications.Add(NotificationKey, NotificationItem);
        });
    }

    FRegexPattern EnsureMessagePattern = FRegexPattern(TEXT("Ensure condition failed:\\s+(.*?)\\s+\\[File:[\\w:\\\\.]*\\] \\[Line: \\d+\\]"));
    int NextNotificationKey = 0;
    TMap<int, TSharedPtr<SNotificationItem>> ActiveNotifications;
};

void FOGEnsureAlertModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogTemp, Error, TEXT("OGEnsureAlert Starting up!"))
    if (GIsEditor)
    {
        EnsureNotifierDevice = MakeUnique<FEnsureNotifierOutputDevice>();
    }
}

void FOGEnsureAlertModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
    EnsureNotifierDevice.Reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOGEnsureAlertModule, OGEnsureAlert)