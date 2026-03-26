#include "DesignerRuntimeConsoleWidget.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "DesignerControlPanelViewModel.h"
#include "Dom/JsonObject.h"
#include "Logging/LogMacros.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Widgets/Colors/SColorPicker.h"
#endif

void UDesignerRuntimeConsoleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UE_LOG(LogTemp, Log, TEXT("DesignerRuntimeConsoleWidget::NativeOnInitialized"));

	BuildLayout();

	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		UE_LOG(LogTemp, Log, TEXT("DesignerRuntimeConsoleWidget acquired ViewModel."));
		CurrentViewModel->OnStateChanged.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleStateChanged);
		CurrentViewModel->OnMessage.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleMessage);

		if (ServerUrlTextBox)
		{
			CurrentViewModel->SetServerBaseUrl(ServerUrlTextBox->GetText().ToString());
		}

		if (!CurrentViewModel->IsAutoPolling())
		{
			UE_LOG(LogTemp, Log, TEXT("DesignerRuntimeConsoleWidget starting auto poll."));
			CurrentViewModel->StartAutoPoll(0.05f, TEXT("/control/next"));
		}

		UE_LOG(LogTemp, Log, TEXT("DesignerRuntimeConsoleWidget pushing initial runtime state."));
		CurrentViewModel->PushRuntimeState(TEXT("/control/state"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DesignerRuntimeConsoleWidget has no ViewModel."));
	}

	RefreshView();
}

void UDesignerRuntimeConsoleWidget::HandleStateChanged()
{
	RefreshView();
}

void UDesignerRuntimeConsoleWidget::HandleTemplateSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bIsRefreshingTemplateSelection)
	{
		return;
	}

	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		CurrentViewModel->SetSelectedTemplateCode(SelectedItem);
	}
}

void UDesignerRuntimeConsoleWidget::HandleMessage(const FString& Message)
{
	SetStatusMessage(Message);
	RefreshView();
}

void UDesignerRuntimeConsoleWidget::HandleRefreshClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		CurrentViewModel->Refresh();
	}
}

void UDesignerRuntimeConsoleWidget::HandleActivateClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		CurrentViewModel->SelectTemplate(GetSelectedTemplateCodeFromUi());
	}
}

void UDesignerRuntimeConsoleWidget::HandleApplyClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		const FString FieldsJson = BuildFieldsJsonFromEditors();
		const FString SelectedTemplateCode = GetSelectedTemplateCodeFromUi();
		if (!SelectedTemplateCode.IsEmpty())
		{
			CurrentViewModel->SelectTemplate(SelectedTemplateCode);
		}

		CurrentViewModel->ApplyFieldsFromJson(FieldsJson);
	}
}

void UDesignerRuntimeConsoleWidget::HandleTakeInClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		CurrentViewModel->TakeIn();
	}
}

void UDesignerRuntimeConsoleWidget::HandleTakeOutClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		CurrentViewModel->TakeOut();
	}
}

void UDesignerRuntimeConsoleWidget::HandleClearClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		CurrentViewModel->ClearActiveTemplate();
	}
}

void UDesignerRuntimeConsoleWidget::HandlePollClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		if (ServerUrlTextBox)
		{
			CurrentViewModel->SetServerBaseUrl(ServerUrlTextBox->GetText().ToString());
		}

		CurrentViewModel->PollRemoteCommand(TEXT("/control/next"));
	}
}

void UDesignerRuntimeConsoleWidget::HandleToggleAutoPollClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		if (ServerUrlTextBox)
		{
			CurrentViewModel->SetServerBaseUrl(ServerUrlTextBox->GetText().ToString());
		}

		if (CurrentViewModel->IsAutoPolling())
		{
			CurrentViewModel->StopAutoPoll();
			SetStatusMessage(TEXT("Auto polling stopped."));
		}
		else
		{
			CurrentViewModel->StartAutoPoll(0.05f, TEXT("/control/next"));
			SetStatusMessage(TEXT("Auto polling started."));
		}

		UpdateAutoPollButtonLabel();
	}
}

void UDesignerRuntimeConsoleWidget::HandlePushStateClicked()
{
	if (UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel())
	{
		if (ServerUrlTextBox)
		{
			CurrentViewModel->SetServerBaseUrl(ServerUrlTextBox->GetText().ToString());
		}

		CurrentViewModel->PushRuntimeState(TEXT("/control/state"));
	}
}

void UDesignerRuntimeConsoleWidget::HandleBrowseImageClicked()
{
	if (ImageFieldKeys.Num() == 0)
	{
		SetStatusMessage(TEXT("No image field is available."));
		return;
	}

	const FName FieldKey = ImageFieldKeys[0];
	UEditableTextBox* const* TextBoxPtr = FieldTextEditors.Find(FieldKey);
	if (!TextBoxPtr || !*TextBoxPtr)
	{
		SetStatusMessage(TEXT("Image field input is unavailable."));
		return;
	}

#if WITH_EDITOR
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		SetStatusMessage(TEXT("Desktop file picker is unavailable."));
		return;
	}

	const void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	}

	TArray<FString> SelectedFiles;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Choose Image File"),
		FString(),
		FString(),
		TEXT("Image Files|*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.exr;*.webp|All Files|*.*"),
		EFileDialogFlags::None,
		SelectedFiles);

	if (bOpened && SelectedFiles.Num() > 0)
	{
		(*TextBoxPtr)->SetText(FText::FromString(SelectedFiles[0]));
		SetStatusMessage(FString::Printf(TEXT("Selected image: %s"), *SelectedFiles[0]));
	}
	else
	{
		SetStatusMessage(TEXT("Image selection canceled."));
	}
#else
	SetStatusMessage(TEXT("File picker is only available in editor builds right now."));
#endif
}

void UDesignerRuntimeConsoleWidget::HandleBrowseColorClicked()
{
	if (ColorFieldKeys.Num() == 0)
	{
		SetStatusMessage(TEXT("No color field is available."));
		return;
	}

	const FName FieldKey = ColorFieldKeys[0];
	UEditableTextBox* const* TextBoxPtr = FieldTextEditors.Find(FieldKey);
	if (!TextBoxPtr || !*TextBoxPtr)
	{
		SetStatusMessage(TEXT("Color field input is unavailable."));
		return;
	}

#if WITH_EDITOR
	FLinearColor InitialColor = FLinearColor(1.0f, 0.666f, 0.0f, 1.0f);
	const FString CurrentValue = (*TextBoxPtr)->GetText().ToString();
	if (!CurrentValue.IsEmpty())
	{
		const FColor ParsedHexColor = FColor::FromHex(CurrentValue);
		if (ParsedHexColor.A != 0 || CurrentValue.Equals(TEXT("#00000000")))
		{
			InitialColor = FLinearColor(ParsedHexColor);
		}
		else
		{
			FLinearColor ParsedLinearColor;
			if (ParsedLinearColor.InitFromString(CurrentValue))
			{
				InitialColor = ParsedLinearColor;
			}
		}
	}

	FColorPickerArgs PickerArgs;
	PickerArgs.InitialColor = InitialColor;
	PickerArgs.bUseAlpha = true;
	PickerArgs.bOnlyRefreshOnOk = false;
	PickerArgs.bOnlyRefreshOnMouseUp = false;
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateWeakLambda(this, [this, TextBox = *TextBoxPtr](FLinearColor NewColor)
	{
		if (!TextBox)
		{
			return;
		}

		const FColor SrgbColor = NewColor.ToFColor(true);
		const FString HexValue = FString::Printf(TEXT("#%02X%02X%02X%02X"), SrgbColor.R, SrgbColor.G, SrgbColor.B, SrgbColor.A);
		TextBox->SetText(FText::FromString(HexValue));
		SetStatusMessage(FString::Printf(TEXT("Selected color: %s"), *HexValue));
	});

	if (!OpenColorPicker(PickerArgs))
	{
		SetStatusMessage(TEXT("Color picker could not be opened."));
	}
#else
	SetStatusMessage(TEXT("Color picker is only available in editor builds right now."));
#endif
}

void UDesignerRuntimeConsoleWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopBar"));
	Root->AddChildToVerticalBox(TopBar);

	ServerUrlTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ServerUrlTextBox"));
	ApplyReadableTextBoxStyle(ServerUrlTextBox);
	ServerUrlTextBox->SetText(FText::FromString(TEXT("http://127.0.0.1:8080")));
	TopBar->AddChildToHorizontalBox(ServerUrlTextBox);

	if (UHorizontalBoxSlot* UrlSlot = Cast<UHorizontalBoxSlot>(ServerUrlTextBox->Slot))
	{
		UrlSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UrlSlot->SetPadding(FMargin(4.0f));
	}

	UTextBlock* IgnoreLabel = nullptr;
	UButton* PollButton = CreateLabeledButton(TEXT("Poll"), IgnoreLabel);
	PollButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandlePollClicked);
	TopBar->AddChildToHorizontalBox(PollButton);

	UButton* PushStateButton = CreateLabeledButton(TEXT("Push State"), IgnoreLabel);
	PushStateButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandlePushStateClicked);
	TopBar->AddChildToHorizontalBox(PushStateButton);

	UTextBlock* AutoPollLabelRaw = nullptr;
	AutoPollButton = CreateLabeledButton(TEXT("Start Auto Poll"), AutoPollLabelRaw);
	AutoPollButtonLabel = AutoPollLabelRaw;
	AutoPollButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleToggleAutoPollClicked);
	TopBar->AddChildToHorizontalBox(AutoPollButton);

	StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusTextBlock"));
	StatusTextBlock->SetText(FText::FromString(TEXT("Ready.")));
	Root->AddChildToVerticalBox(StatusTextBlock);

	UHorizontalBox* MainContent = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainContent"));
	Root->AddChildToVerticalBox(MainContent);
	if (UVerticalBoxSlot* MainContentSlot = Cast<UVerticalBoxSlot>(MainContent->Slot))
	{
		MainContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* LeftPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftPanel"));
	MainContent->AddChildToHorizontalBox(LeftPanel);
	if (UHorizontalBoxSlot* LeftSlot = Cast<UHorizontalBoxSlot>(LeftPanel->Slot))
	{
		LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LeftSlot->SetPadding(FMargin(4.0f));
	}

	LeftPanel->AddChildToVerticalBox(CreateSectionLabel(TEXT("Template")));
	TemplateComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("TemplateComboBox"));
	TemplateComboBox->OnSelectionChanged.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleTemplateSelectionChanged);
	LeftPanel->AddChildToVerticalBox(TemplateComboBox);

	UButton* RefreshButton = CreateLabeledButton(TEXT("Refresh Catalog"), IgnoreLabel);
	RefreshButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleRefreshClicked);
	LeftPanel->AddChildToVerticalBox(RefreshButton);

	UButton* ActivateButton = CreateLabeledButton(TEXT("Activate Template"), IgnoreLabel);
	ActivateButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleActivateClicked);
	LeftPanel->AddChildToVerticalBox(ActivateButton);

	UButton* TakeInButton = CreateLabeledButton(TEXT("Take In"), IgnoreLabel);
	TakeInButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleTakeInClicked);
	LeftPanel->AddChildToVerticalBox(TakeInButton);

	UButton* TakeOutButton = CreateLabeledButton(TEXT("Take Out"), IgnoreLabel);
	TakeOutButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleTakeOutClicked);
	LeftPanel->AddChildToVerticalBox(TakeOutButton);

	UButton* ClearButton = CreateLabeledButton(TEXT("Clear"), IgnoreLabel);
	ClearButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleClearClicked);
	LeftPanel->AddChildToVerticalBox(ClearButton);

	UVerticalBox* CenterPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterPanel"));
	MainContent->AddChildToHorizontalBox(CenterPanel);
	if (UHorizontalBoxSlot* CenterSlot = Cast<UHorizontalBoxSlot>(CenterPanel->Slot))
	{
		CenterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CenterSlot->SetPadding(FMargin(4.0f));
	}

	CenterPanel->AddChildToVerticalBox(CreateSectionLabel(TEXT("Preview")));

	USizeBox* PreviewCard = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviewCard"));
	PreviewCard->SetHeightOverride(300.0f);
	CenterPanel->AddChildToVerticalBox(PreviewCard);
	if (UVerticalBoxSlot* PreviewCardSlot = Cast<UVerticalBoxSlot>(PreviewCard->Slot))
	{
		PreviewCardSlot->SetPadding(FMargin(4.0f));
	}

	UBorder* PreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewBorder"));
	FSlateBrush PreviewBrush;
	PreviewBrush.TintColor = FSlateColor(FLinearColor(0.07f, 0.08f, 0.10f, 1.0f));
	PreviewBorder->SetBrush(PreviewBrush);
	PreviewBorder->SetPadding(FMargin(18.0f));
	PreviewCard->SetContent(PreviewBorder);

	UVerticalBox* PreviewInfoBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PreviewInfoBox"));
	PreviewBorder->SetContent(PreviewInfoBox);

	PreviewHintTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PreviewHintTextBlock"));
	PreviewHintTextBlock->SetText(FText::FromString(TEXT("Viewport lower-third preview is pinned to the screen during Play.")));
	PreviewInfoBox->AddChildToVerticalBox(PreviewHintTextBlock);

	ActiveTemplateTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActiveTemplateTextBlock"));
	ActiveTemplateTextBlock->SetText(FText::FromString(TEXT("Active Template: None")));
	PreviewInfoBox->AddChildToVerticalBox(ActiveTemplateTextBlock);

	PlaybackStateTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlaybackStateTextBlock"));
	PlaybackStateTextBlock->SetText(FText::FromString(TEXT("Playback: Standby")));
	PreviewInfoBox->AddChildToVerticalBox(PlaybackStateTextBlock);

	ConnectionStateTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConnectionStateTextBlock"));
	ConnectionStateTextBlock->SetText(FText::FromString(TEXT("Connection: Local Only")));
	PreviewInfoBox->AddChildToVerticalBox(ConnectionStateTextBlock);

	UVerticalBox* RightPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightPanel"));
	MainContent->AddChildToHorizontalBox(RightPanel);
	if (UHorizontalBoxSlot* RightSlot = Cast<UHorizontalBoxSlot>(RightPanel->Slot))
	{
		RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RightSlot->SetPadding(FMargin(4.0f));
	}

	RightPanel->AddChildToVerticalBox(CreateSectionLabel(TEXT("Parameters")));
	FieldEditorContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FieldEditorContainer"));
	RightPanel->AddChildToVerticalBox(FieldEditorContainer);
	if (UVerticalBoxSlot* EditorSlot = Cast<UVerticalBoxSlot>(FieldEditorContainer->Slot))
	{
		EditorSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		EditorSlot->SetPadding(FMargin(4.0f));
	}

	UHorizontalBox* DebugRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DebugRow"));
	Root->AddChildToVerticalBox(DebugRow);
	if (UVerticalBoxSlot* DebugRowSlot = Cast<UVerticalBoxSlot>(DebugRow->Slot))
	{
		DebugRowSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}

	UVerticalBox* DebugLeftPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugLeftPanel"));
	DebugRow->AddChildToHorizontalBox(DebugLeftPanel);
	if (UHorizontalBoxSlot* DebugLeftSlot = Cast<UHorizontalBoxSlot>(DebugLeftPanel->Slot))
	{
		DebugLeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DebugLeftSlot->SetPadding(FMargin(4.0f));
	}

	DebugLeftPanel->AddChildToVerticalBox(CreateSectionLabel(TEXT("Fields JSON Preview")));
	USizeBox* FieldsJsonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FieldsJsonSizeBox"));
	FieldsJsonSizeBox->SetHeightOverride(120.0f);
	DebugLeftPanel->AddChildToVerticalBox(FieldsJsonSizeBox);
	FieldsJsonTextBox = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(UMultiLineEditableTextBox::StaticClass(), TEXT("FieldsJsonTextBox"));
	ApplyReadableMultiLineStyle(FieldsJsonTextBox);
	FieldsJsonTextBox->SetIsReadOnly(true);
	FieldsJsonSizeBox->SetContent(FieldsJsonTextBox);
	if (UVerticalBoxSlot* FieldsSlot = Cast<UVerticalBoxSlot>(FieldsJsonSizeBox->Slot))
	{
		FieldsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		FieldsSlot->SetPadding(FMargin(4.0f));
	}

	UButton* ApplyButton = CreateLabeledButton(TEXT("Apply Fields"), IgnoreLabel);
	ApplyButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleApplyClicked);
	RightPanel->AddChildToVerticalBox(ApplyButton);
	if (UVerticalBoxSlot* ApplySlot = Cast<UVerticalBoxSlot>(ApplyButton->Slot))
	{
		ApplySlot->SetPadding(FMargin(4.0f, 8.0f, 4.0f, 8.0f));
	}

	UVerticalBox* DebugRightPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugRightPanel"));
	DebugRow->AddChildToHorizontalBox(DebugRightPanel);
	if (UHorizontalBoxSlot* DebugRightSlot = Cast<UHorizontalBoxSlot>(DebugRightPanel->Slot))
	{
		DebugRightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DebugRightSlot->SetPadding(FMargin(4.0f));
	}

	DebugRightPanel->AddChildToVerticalBox(CreateSectionLabel(TEXT("Runtime State JSON")));
	USizeBox* RuntimeStateSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RuntimeStateSizeBox"));
	RuntimeStateSizeBox->SetHeightOverride(180.0f);
	DebugRightPanel->AddChildToVerticalBox(RuntimeStateSizeBox);
	RuntimeStateTextBox = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(UMultiLineEditableTextBox::StaticClass(), TEXT("RuntimeStateTextBox"));
	ApplyReadableMultiLineStyle(RuntimeStateTextBox);
	RuntimeStateTextBox->SetIsReadOnly(true);
	RuntimeStateSizeBox->SetContent(RuntimeStateTextBox);
	if (UVerticalBoxSlot* RuntimeSlot = Cast<UVerticalBoxSlot>(RuntimeStateSizeBox->Slot))
	{
		RuntimeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		RuntimeSlot->SetPadding(FMargin(4.0f));
	}
}

void UDesignerRuntimeConsoleWidget::RefreshView()
{
	UDesignerControlPanelViewModel* CurrentViewModel = GetViewModel();
	if (!CurrentViewModel)
	{
		return;
	}

	if (ServerUrlTextBox)
	{
		ServerUrlTextBox->SetText(FText::FromString(CurrentViewModel->GetServerBaseUrl()));
	}

	if (TemplateComboBox)
	{
		bIsRefreshingTemplateSelection = true;
		TemplateComboBox->ClearOptions();
		for (const FDesignerTemplateDescriptor& Template : CurrentViewModel->GetTemplates())
		{
			TemplateComboBox->AddOption(Template.TemplateCode);
		}

		const FString SelectedCode = CurrentViewModel->GetSelectedTemplateCode();
		if (!SelectedCode.IsEmpty())
		{
			TemplateComboBox->SetSelectedOption(SelectedCode);
		}
		else if (TemplateComboBox->GetOptionCount() > 0)
		{
			TemplateComboBox->SetSelectedIndex(0);
		}
		bIsRefreshingTemplateSelection = false;
	}

	RebuildFieldEditors();

	if (FieldsJsonTextBox)
	{
		FieldsJsonTextBox->SetText(FText::FromString(CurrentViewModel->GetEditableFieldsJson()));
	}

	if (RuntimeStateTextBox)
	{
		RuntimeStateTextBox->SetText(FText::FromString(CurrentViewModel->GetRuntimeStateJson()));
	}

	const FDesignerRuntimeState RuntimeState = CurrentViewModel->GetRuntimeState();
	if (ActiveTemplateTextBlock)
	{
		const FString ActiveTemplateLine = RuntimeState.bHasActiveTemplate
			? FString::Printf(TEXT("Active Template: %s"), *RuntimeState.ActiveTemplateCode)
			: TEXT("Active Template: None");
		ActiveTemplateTextBlock->SetText(FText::FromString(ActiveTemplateLine));
	}

	if (PlaybackStateTextBlock)
	{
		PlaybackStateTextBlock->SetText(FText::FromString(RuntimeState.bIsOnAir ? TEXT("Playback: On Air") : TEXT("Playback: Standby")));
	}

	if (ConnectionStateTextBlock)
	{
		const bool bPolling = CurrentViewModel->IsAutoPolling();
		ConnectionStateTextBlock->SetText(FText::FromString(bPolling ? TEXT("Connection: Auto Polling Enabled") : TEXT("Connection: Local / Manual Poll")));
	}

	if (PreviewHintTextBlock)
	{
		PreviewHintTextBlock->SetText(FText::FromString(TEXT("Viewport lower-third preview is pinned on screen while this panel controls the active template.")));
	}

	UpdateAutoPollButtonLabel();
}

UButton* UDesignerRuntimeConsoleWidget::CreateLabeledButton(const FString& Label, UTextBlock*& OutLabelWidget)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelBlock->SetText(FText::FromString(Label));
	Button->AddChild(LabelBlock);
	OutLabelWidget = LabelBlock;

	return Button;
}

void UDesignerRuntimeConsoleWidget::RebuildFieldEditors()
{
	if (!FieldEditorContainer || !GetViewModel())
	{
		return;
	}

	FieldTextEditors.Reset();
	FieldCheckEditors.Reset();
	ImageFieldKeys.Reset();
	ColorFieldKeys.Reset();
	FieldEditorContainer->ClearChildren();

	for (const FDesignerEditableFieldValue& Field : GetViewModel()->GetEditableFields())
	{
		UVerticalBox* FieldRow = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		FieldEditorContainer->AddChildToVerticalBox(FieldRow);
		if (UVerticalBoxSlot* FieldRowSlot = Cast<UVerticalBoxSlot>(FieldRow->Slot))
		{
			FieldRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}

		FString LabelText = Field.Label.IsEmpty() ? Field.Key.ToString() : Field.Label.ToString();
		if (Field.bRequired)
		{
			LabelText += TEXT(" *");
		}

		UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelBlock->SetText(FText::FromString(LabelText));
		FieldRow->AddChildToVerticalBox(LabelBlock);

		if (Field.Type == EDesignerTemplateParamType::Boolean)
		{
			UCheckBox* CheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
			CheckBox->SetIsChecked(Field.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase));
			FieldRow->AddChildToVerticalBox(CheckBox);
			FieldCheckEditors.Add(Field.Key, CheckBox);
			continue;
		}

		UEditableTextBox* TextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		ApplyReadableTextBoxStyle(TextBox);
		TextBox->SetText(FText::FromString(Field.Value));

		switch (Field.Type)
		{
		case EDesignerTemplateParamType::Color:
			TextBox->SetHintText(FText::FromString(TEXT("#RRGGBBAA")));
			{
				UHorizontalBox* ColorRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				FieldRow->AddChildToVerticalBox(ColorRow);

				ColorRow->AddChildToHorizontalBox(TextBox);
				if (UHorizontalBoxSlot* TextSlot = Cast<UHorizontalBoxSlot>(TextBox->Slot))
				{
					TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					TextSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
				}

				UTextBlock* BrowseLabel = nullptr;
				UButton* BrowseButton = CreateLabeledButton(TEXT("Choose Color"), BrowseLabel);
				BrowseButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleBrowseColorClicked);
				ColorRow->AddChildToHorizontalBox(BrowseButton);
				ColorFieldKeys.Add(Field.Key);
			}
			break;
		case EDesignerTemplateParamType::Image:
			TextBox->SetHintText(FText::FromString(TEXT("Image Path Or URL")));
			{
				UHorizontalBox* ImageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				FieldRow->AddChildToVerticalBox(ImageRow);

				ImageRow->AddChildToHorizontalBox(TextBox);
				if (UHorizontalBoxSlot* TextSlot = Cast<UHorizontalBoxSlot>(TextBox->Slot))
				{
					TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					TextSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
				}

				UTextBlock* BrowseLabel = nullptr;
				UButton* BrowseButton = CreateLabeledButton(TEXT("Choose File"), BrowseLabel);
				BrowseButton->OnClicked.AddDynamic(this, &UDesignerRuntimeConsoleWidget::HandleBrowseImageClicked);
				ImageRow->AddChildToHorizontalBox(BrowseButton);
				ImageFieldKeys.Add(Field.Key);
			}
			break;
		default:
			TextBox->SetHintText(FText::FromString(Field.DefaultValue));
			FieldRow->AddChildToVerticalBox(TextBox);
			break;
		}
		FieldTextEditors.Add(Field.Key, TextBox);
	}
}

FString UDesignerRuntimeConsoleWidget::BuildFieldsJsonFromEditors() const
{
	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();

	for (const TPair<FName, UEditableTextBox*>& Entry : FieldTextEditors)
	{
		if (Entry.Value)
		{
			RootObject->SetStringField(Entry.Key.ToString(), Entry.Value->GetText().ToString());
		}
	}

	for (const TPair<FName, UCheckBox*>& Entry : FieldCheckEditors)
	{
		if (Entry.Value)
		{
			RootObject->SetBoolField(Entry.Key.ToString(), Entry.Value->IsChecked());
		}
	}

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	return Output;
}

UWidget* UDesignerRuntimeConsoleWidget::CreateSectionLabel(const FString& Label)
{
	UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelBlock->SetText(FText::FromString(Label));
	return LabelBlock;
}

FString UDesignerRuntimeConsoleWidget::GetSelectedTemplateCodeFromUi() const
{
	return TemplateComboBox ? TemplateComboBox->GetSelectedOption() : FString();
}

void UDesignerRuntimeConsoleWidget::UpdateAutoPollButtonLabel() const
{
	if (AutoPollButtonLabel)
	{
		const bool bIsAutoPolling = GetViewModel() && GetViewModel()->IsAutoPolling();
		AutoPollButtonLabel->SetText(FText::FromString(bIsAutoPolling ? TEXT("Stop Auto Poll") : TEXT("Start Auto Poll")));
	}
}

void UDesignerRuntimeConsoleWidget::SetStatusMessage(const FString& Message) const
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(Message));
	}
}

void UDesignerRuntimeConsoleWidget::ApplyReadableTextBoxStyle(UEditableTextBox* TextBox) const
{
	if (!TextBox)
	{
		return;
	}

	TextBox->SetForegroundColor(FLinearColor(0.96f, 0.96f, 0.96f, 1.0f));

	FEditableTextBoxStyle Style = TextBox->GetWidgetStyle();
	Style.SetForegroundColor(FSlateColor(FLinearColor(0.96f, 0.96f, 0.96f, 1.0f)));
	Style.SetFocusedForegroundColor(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.SetReadOnlyForegroundColor(FSlateColor(FLinearColor(0.80f, 0.80f, 0.80f, 1.0f)));
	Style.SetBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
	Style.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.96f, 0.96f, 1.0f)));
	Style.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f));
	Style.BackgroundImageHovered.TintColor = FSlateColor(FLinearColor(0.16f, 0.16f, 0.16f, 1.0f));
	Style.BackgroundImageFocused.TintColor = FSlateColor(FLinearColor(0.18f, 0.18f, 0.18f, 1.0f));
	Style.BackgroundImageReadOnly.TintColor = FSlateColor(FLinearColor(0.10f, 0.10f, 0.10f, 1.0f));
	TextBox->SetWidgetStyle(Style);
}

void UDesignerRuntimeConsoleWidget::ApplyReadableMultiLineStyle(UMultiLineEditableTextBox* TextBox) const
{
	if (!TextBox)
	{
		return;
	}

	TextBox->SetForegroundColor(FLinearColor(0.96f, 0.96f, 0.96f, 1.0f));

	FEditableTextBoxStyle Style = TextBox->WidgetStyle;
	Style.SetForegroundColor(FSlateColor(FLinearColor(0.96f, 0.96f, 0.96f, 1.0f)));
	Style.SetFocusedForegroundColor(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
	Style.SetReadOnlyForegroundColor(FSlateColor(FLinearColor(0.80f, 0.80f, 0.80f, 1.0f)));
	Style.SetBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
	Style.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.96f, 0.96f, 1.0f)));
	Style.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f));
	Style.BackgroundImageHovered.TintColor = FSlateColor(FLinearColor(0.16f, 0.16f, 0.16f, 1.0f));
	Style.BackgroundImageFocused.TintColor = FSlateColor(FLinearColor(0.18f, 0.18f, 0.18f, 1.0f));
	Style.BackgroundImageReadOnly.TintColor = FSlateColor(FLinearColor(0.10f, 0.10f, 0.10f, 1.0f));
	TextBox->WidgetStyle = Style;
	TextBox->SetTextStyle(Style.TextStyle);
}


