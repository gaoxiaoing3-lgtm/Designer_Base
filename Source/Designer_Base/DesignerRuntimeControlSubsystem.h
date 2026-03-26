#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DesignerRuntimeControlTypes.h"
#include "DesignerRuntimeControlSubsystem.generated.h"

class UDesignerTemplateRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDesignerControlExecutedSignature, const FDesignerControlResult&, Result);

UCLASS()
class DESIGNER_BASE_API UDesignerRuntimeControlSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Designer|Control")
	FDesignerControlExecutedSignature OnControlExecuted;

	UFUNCTION(BlueprintCallable, Category = "Designer|Control")
	FDesignerControlResult ExecuteControlCommand(const FDesignerControlCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "Designer|Control")
	FDesignerControlResult ExecuteControlJson(const FString& JsonPayload);

	UFUNCTION(BlueprintCallable, Category = "Designer|Control")
	FString ExecuteControlJsonAndGetResponse(const FString& JsonPayload);

	UFUNCTION(BlueprintPure, Category = "Designer|Control")
	FDesignerRuntimeState GetRuntimeState() const;

	UFUNCTION(BlueprintPure, Category = "Designer|Control")
	FString GetRuntimeStateJson() const;

	UFUNCTION(BlueprintPure, Category = "Designer|Control")
	FString GetLastControlResultJson() const;

private:
	FDesignerControlResult BuildResult(bool bSuccess, const FDesignerControlCommand& Command, const FString& Message) const;
	bool TryParseCommandJson(const FString& JsonPayload, FDesignerControlCommand& OutCommand, FString& OutError) const;
	FString SerializeStateToJson(const FDesignerRuntimeState& State) const;
	FString SerializeResultToJson(const FDesignerControlResult& Result) const;
	UDesignerTemplateRegistrySubsystem* GetRegistry() const;

	UPROPERTY(Transient)
	FString LastControlResultJson;
};
