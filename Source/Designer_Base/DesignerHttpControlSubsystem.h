#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DesignerRuntimeControlTypes.h"
#include "DesignerHttpControlSubsystem.generated.h"

class FSocket;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDesignerHttpTextMessageSignature, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDesignerHttpControlResultSignature, const FDesignerControlResult&, Result);

UCLASS()
class DESIGNER_BASE_API UDesignerHttpControlSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Designer|HTTP")
	void SetServerBaseUrl(const FString& InBaseUrl);

	UFUNCTION(BlueprintPure, Category = "Designer|HTTP")
	FString GetServerBaseUrl() const;

	UFUNCTION(BlueprintCallable, Category = "Designer|HTTP")
	bool PollCommandEndpoint(const FString& EndpointPath = TEXT("/control/next"));

	UFUNCTION(BlueprintCallable, Category = "Designer|HTTP")
	bool PushRuntimeState(const FString& EndpointPath = TEXT("/control/state"));

	UFUNCTION(BlueprintCallable, Category = "Designer|HTTP")
	void EnsureRealtimeConnection();

	UFUNCTION(BlueprintPure, Category = "Designer|HTTP")
	bool IsRealtimeConnected() const;

	UFUNCTION(BlueprintPure, Category = "Designer|HTTP")
	bool IsRealtimeConnecting() const;

	UPROPERTY(BlueprintAssignable, Category = "Designer|HTTP")
	FDesignerHttpControlResultSignature OnRemoteCommandExecuted;

	UPROPERTY(BlueprintAssignable, Category = "Designer|HTTP")
	FDesignerHttpTextMessageSignature OnHttpRequestFailed;

	UPROPERTY(BlueprintAssignable, Category = "Designer|HTTP")
	FDesignerHttpTextMessageSignature OnHttpStatePushed;

private:
	FString BuildUrl(const FString& EndpointPath) const;
	FString BuildRealtimeHost() const;
	int32 BuildRealtimePort() const;
	void CloseRealtimeSocket();
	void SendRuntimeStateOverRealtime();
	void HandleRealtimeMessage(const FString& Message);
	void ScheduleRealtimeReconnect(float DelaySeconds);
	void ScheduleRealtimePump();
	void PumpRealtimeSocket();
	bool ConnectRealtimeSocket();
	bool SendRealtimePayload(const FString& Payload);

	UPROPERTY(EditAnywhere, Category = "Designer|HTTP")
	FString ServerBaseUrl = TEXT("http://127.0.0.1:8080");

	FTimerHandle RealtimeReconnectTimerHandle;
	FTimerHandle RealtimePumpTimerHandle;

	FSocket* RealtimeSocket = nullptr;
	FString RealtimeReceiveBuffer;
	bool bRealtimeConnecting = false;
	bool bReconnectTimerActive = false;
	float RealtimeReconnectDelaySeconds = 0.5f;
};
