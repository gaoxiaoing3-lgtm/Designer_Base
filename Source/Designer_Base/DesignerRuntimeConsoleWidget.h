#pragma once

#include "CoreMinimal.h"
#include "DesignerControlPanelWidgetBase.h"
#include "DesignerRuntimeConsoleWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS(Blueprintable)
class DESIGNER_BASE_API UDesignerRuntimeConsoleWidget : public UDesignerControlPanelWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

protected:
	UFUNCTION()
	void HandleStateChanged();

	UFUNCTION()
	void HandleTemplateSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleMessage(const FString& Message);

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleActivateClicked();

	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleTakeInClicked();

	UFUNCTION()
	void HandleTakeOutClicked();

	UFUNCTION()
	void HandleClearClicked();

	UFUNCTION()
	void HandlePollClicked();

	UFUNCTION()
	void HandleToggleAutoPollClicked();

	UFUNCTION()
	void HandlePushStateClicked();

	UFUNCTION()
	void HandleBrowseImageClicked();

	UFUNCTION()
	void HandleBrowseColorClicked();

private:
	void BuildLayout();
	void RefreshView();
	void RebuildFieldEditors();
	FString BuildFieldsJsonFromEditors() const;
	UButton* CreateLabeledButton(const FString& Label, UTextBlock*& OutLabelWidget);
	UWidget* CreateSectionLabel(const FString& Label);
	FString GetSelectedTemplateCodeFromUi() const;
	void UpdateAutoPollButtonLabel() const;
	void SetStatusMessage(const FString& Message) const;
	void ApplyReadableTextBoxStyle(UEditableTextBox* TextBox) const;
	void ApplyReadableMultiLineStyle(UMultiLineEditableTextBox* TextBox) const;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> TemplateComboBox;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> ServerUrlTextBox;

	UPROPERTY(Transient)
	TObjectPtr<UMultiLineEditableTextBox> FieldsJsonTextBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FieldEditorContainer;

	UPROPERTY(Transient)
	TObjectPtr<UMultiLineEditableTextBox> RuntimeStateTextBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActiveTemplateTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlaybackStateTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConnectionStateTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviewHintTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AutoPollButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AutoPollButtonLabel;

	UPROPERTY(Transient)
	bool bIsRefreshingTemplateSelection = false;

	TMap<FName, UEditableTextBox*> FieldTextEditors;
	TMap<FName, UCheckBox*> FieldCheckEditors;
	TArray<FName> ImageFieldKeys;
	TArray<FName> ColorFieldKeys;
};
