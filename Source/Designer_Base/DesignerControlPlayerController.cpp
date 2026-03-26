#include "DesignerControlPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "DesignerBroadcastOutputWidget.h"
#include "DesignerLowerThirdPreviewWidget.h"
#include "DesignerLowerThirdTemplateActor.h"
#include "DesignerTemplateRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Logging/LogMacros.h"
#include "Widgets/SWindow.h"

ADesignerControlPlayerController::ADesignerControlPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	ViewportPreviewWidgetClass = UDesignerLowerThirdPreviewWidget::StaticClass();
	BroadcastOutputWidgetClass = UDesignerBroadcastOutputWidget::StaticClass();
}

void ADesignerControlPlayerController::BeginPlay()
{
	Super::BeginPlay();

	EnsureBroadcastOutputWindow();
	RefreshViewportPreview();

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			PreviewRefreshTimerHandle,
			this,
			&ADesignerControlPlayerController::RefreshViewportPreview,
			0.1f,
			true,
			0.0f);
	}
}

void ADesignerControlPlayerController::SetControlPanelWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
	ControlPanelWidgetClass = InWidgetClass;
}

UUserWidget* ADesignerControlPlayerController::EnsureControlPanelWidget()
{
	return nullptr;

	/*
	if (ControlPanelWidget)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Designer control panel widget already exists: %s"), *GetNameSafe(ControlPanelWidget));
		return ControlPanelWidget;
	}

	if (!ControlPanelWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Designer control panel widget class is null."));
		return nullptr;
	}

	ControlPanelWidget = CreateWidget<UUserWidget>(this, ControlPanelWidgetClass);
	if (ControlPanelWidget)
	{
		UE_LOG(LogTemp, Log, TEXT("Created designer control panel widget: %s"), *GetNameSafe(ControlPanelWidget));
		ControlPanelWidget->AddToViewport(0);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create designer control panel widget from class %s"), *GetNameSafe(ControlPanelWidgetClass));
	}

	return ControlPanelWidget;
	*/
}

void ADesignerControlPlayerController::RemoveControlPanelWidget()
{
	if (ControlPanelWidget)
	{
		ControlPanelWidget->RemoveFromParent();
		ControlPanelWidget = nullptr;
	}
}

void ADesignerControlPlayerController::EnsureViewportPreviewWidget()
{
	if (BroadcastOutputWidget || !BroadcastOutputWidgetClass)
	{
		return;
	}

	BroadcastOutputWidget = CreateWidget<UDesignerBroadcastOutputWidget>(this, BroadcastOutputWidgetClass);
	if (BroadcastOutputWidget)
	{
		BroadcastOutputWidget->AddToViewport(0);
	}
}

void ADesignerControlPlayerController::EnsureBroadcastOutputWindow()
{
	if (!bOpenBroadcastOutputWindow)
	{
		EnsureViewportPreviewWidget();
		return;
	}

	if (BroadcastOutputWindow.IsValid() || !BroadcastOutputWidgetClass)
	{
		return;
	}

	BroadcastOutputWidget = CreateWidget<UDesignerBroadcastOutputWidget>(this, BroadcastOutputWidgetClass);
	if (!BroadcastOutputWidget || !FSlateApplication::IsInitialized())
	{
		return;
	}

	BroadcastOutputWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Designer Broadcast Output")))
		.ClientSize(FVector2D(1920.0f, 1080.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(true)
		.IsInitiallyMaximized(false)
		.SizingRule(ESizingRule::FixedSize);

	BroadcastOutputWindow->SetContent(BroadcastOutputWidget->TakeWidget());
	FSlateApplication::Get().AddWindow(BroadcastOutputWindow.ToSharedRef());
}

void ADesignerControlPlayerController::RefreshViewportPreview()
{
	UDesignerLowerThirdPreviewWidget* TargetViewportPreview = nullptr;
	UDesignerBroadcastOutputWidget* TargetBroadcastOutput = BroadcastOutputWidget;

	if (!TargetBroadcastOutput)
	{
		return;
	}

	UDesignerTemplateRegistrySubsystem* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDesignerTemplateRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		if (TargetBroadcastOutput)
		{
			TargetBroadcastOutput->SetOutputData(TEXT(""), TEXT(""), FString(), FLinearColor::White, false, false);
		}
		return;
	}

	if (!IsValid(Registry->GetActiveTemplateActor()))
	{
		Registry->ActivateTemplateByCode(TEXT("lowerthird_basic"));
	}

	ADesignerLowerThirdTemplateActor* LowerThirdActor = Cast<ADesignerLowerThirdTemplateActor>(Registry->GetActiveTemplateActor());
	if (!LowerThirdActor)
	{
		if (TargetBroadcastOutput)
		{
			TargetBroadcastOutput->SetOutputData(TEXT(""), TEXT(""), FString(), FLinearColor::White, false, false);
		}
		return;
	}

	if (TargetBroadcastOutput)
	{
		TargetBroadcastOutput->SetOutputData(
			LowerThirdActor->CurrentTitle,
			LowerThirdActor->CurrentSubtitle,
			LowerThirdActor->CurrentLogoPath,
			LowerThirdActor->CurrentThemeColor,
			LowerThirdActor->bShowLogo,
			LowerThirdActor->bIsOnAir);
	}
}
