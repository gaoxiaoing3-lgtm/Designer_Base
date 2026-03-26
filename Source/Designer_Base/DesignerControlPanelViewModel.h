#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UObject/Object.h"
#include "DesignerRuntimeControlTypes.h"
#include "DesignerControlPanelViewModel.generated.h"

class UDesignerHttpControlSubsystem;
class UDesignerRuntimeControlSubsystem;
class UDesignerTemplateRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDesignerPanelStateChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDesignerPanelMessageSignature, const FString&, Message);

UCLASS(BlueprintType)
class DESIGNER_BASE_API UDesignerControlPanelViewModel : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Designer|UI", meta = (WorldContext = "WorldContextObject"))
	static UDesignerControlPanelViewModel* CreateDesignerControlPanelViewModel(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool SelectTemplate(const FString& TemplateCode);

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void SetSelectedTemplateCode(const FString& TemplateCode);

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void SetFieldValue(FName FieldKey, const FString& InValue);

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool ApplyCurrentFields();

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool ApplyFieldsFromJson(const FString& FieldsJson);

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool TakeIn();

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool TakeOut();

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool ClearActiveTemplate();

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool PollRemoteCommand(const FString& EndpointPath = TEXT("/control/next"));

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	bool PushRuntimeState(const FString& EndpointPath = TEXT("/control/state"));

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void StartAutoPoll(float IntervalSeconds = 1.0f, const FString& EndpointPath = TEXT("/control/next"));

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void StopAutoPoll();

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	bool IsAutoPolling() const;

	UFUNCTION(BlueprintCallable, Category = "Designer|UI")
	void SetServerBaseUrl(const FString& InBaseUrl);

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	FString GetServerBaseUrl() const;

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	TArray<FDesignerTemplateDescriptor> GetTemplates() const;

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	FDesignerRuntimeState GetRuntimeState() const;

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	FString GetRuntimeStateJson() const;

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	TArray<FDesignerEditableFieldValue> GetEditableFields() const;

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	FString GetEditableFieldsJson() const;

	UFUNCTION(BlueprintPure, Category = "Designer|UI")
	FString GetSelectedTemplateCode() const;

	UPROPERTY(BlueprintAssignable, Category = "Designer|UI")
	FDesignerPanelStateChangedSignature OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Designer|UI")
	FDesignerPanelMessageSignature OnMessage;

protected:
	virtual UWorld* GetWorld() const override;

private:
	void Initialize(UObject* WorldContextObject);
	void RebuildEditableFields();
	TMap<FName, FString> BuildFieldMap() const;
	UDesignerTemplateRegistrySubsystem* GetRegistry() const;
	UDesignerRuntimeControlSubsystem* GetRuntimeControl() const;
	UDesignerHttpControlSubsystem* GetHttpControl() const;
	void ScheduleNextAutoPoll(float DelaySeconds = 0.0f);

	UFUNCTION()
	void HandleRemoteResult(const FDesignerControlResult& Result);

	UFUNCTION()
	void HandleHttpFailure(const FString& Message);

	UFUNCTION()
	void HandleHttpStatePushed(const FString& Message);

	UFUNCTION()
	void HandleAutoPollTick();

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	UPROPERTY(Transient)
	FDesignerRuntimeState CachedState;

	UPROPERTY(Transient)
	TArray<FDesignerTemplateDescriptor> CachedTemplates;

	UPROPERTY(Transient)
	TArray<FDesignerEditableFieldValue> EditableFields;

	UPROPERTY(Transient)
	FString SelectedTemplateCode;

	UPROPERTY(Transient)
	bool bHttpHandlersBound = false;

	UPROPERTY(Transient)
	FTimerHandle AutoPollTimerHandle;

	UPROPERTY(Transient)
	FString AutoPollEndpointPath = TEXT("/control/next");

	UPROPERTY(Transient)
	bool bAutoPollingEnabled = false;

	UPROPERTY(Transient)
	bool bAutoPollRequestInFlight = false;

	UPROPERTY(Transient)
	float AutoPollRetryDelaySeconds = 0.05f;
};
