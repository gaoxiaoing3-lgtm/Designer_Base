#include "DesignerBroadcastOutputWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "DesignerLowerThirdPreviewWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"

void UDesignerBroadcastOutputWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuildLayout();
	SetOutputData(TEXT(""), TEXT(""), FString(), FLinearColor(1.0f, 0.666f, 0.0f, 1.0f), false, false);
}

void UDesignerBroadcastOutputWidget::SetOutputData(
	const FString& InTitle,
	const FString& InSubtitle,
	const FString& InLogoPath,
	const FLinearColor& InThemeColor,
	bool bInShowLogo,
	bool bInIsOnAir)
{
	if (!PreviewWidget)
	{
		return;
	}

	PreviewWidget->SetPreviewData(InTitle, InSubtitle, InLogoPath, InThemeColor, bInShowLogo, bInIsOnAir);
}

void UDesignerBroadcastOutputWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BackgroundBorder"));
	FSlateBrush BackgroundBrush;
	BackgroundBrush.TintColor = FSlateColor(FLinearColor::Black);
	BackgroundBorder->SetBrush(BackgroundBrush);
	RootCanvas->AddChildToCanvas(BackgroundBorder);
	if (UCanvasPanelSlot* BackgroundSlot = Cast<UCanvasPanelSlot>(BackgroundBorder->Slot))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	PreviewWidget = WidgetTree->ConstructWidget<UDesignerLowerThirdPreviewWidget>(UDesignerLowerThirdPreviewWidget::StaticClass(), TEXT("BroadcastPreviewWidget"));
	PreviewWidget->SetBroadcastMode(true);
	RootCanvas->AddChildToCanvas(PreviewWidget);
	if (UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(PreviewWidget->Slot))
	{
		PreviewSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		PreviewSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		PreviewSlot->SetPosition(FVector2D(48.0f, -48.0f));
		PreviewSlot->SetSize(FVector2D(720.0f, 160.0f));
		PreviewSlot->SetAutoSize(true);
	}
}
