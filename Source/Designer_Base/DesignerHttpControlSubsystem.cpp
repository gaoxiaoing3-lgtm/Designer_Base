#include "DesignerHttpControlSubsystem.h"

#include "Common/TcpSocketBuilder.h"
#include "DesignerRuntimeControlSubsystem.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "IPAddress.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Logging/LogMacros.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

void UDesignerHttpControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureRealtimeConnection();
}

void UDesignerHttpControlSubsystem::Deinitialize()
{
	if (GetGameInstance() && GetGameInstance()->GetWorld())
	{
		GetGameInstance()->GetWorld()->GetTimerManager().ClearTimer(RealtimeReconnectTimerHandle);
		GetGameInstance()->GetWorld()->GetTimerManager().ClearTimer(RealtimePumpTimerHandle);
	}

	CloseRealtimeSocket();
	Super::Deinitialize();
}

void UDesignerHttpControlSubsystem::SetServerBaseUrl(const FString& InBaseUrl)
{
	ServerBaseUrl = InBaseUrl;
	RealtimeReconnectDelaySeconds = 0.5f;
	EnsureRealtimeConnection();
}

FString UDesignerHttpControlSubsystem::GetServerBaseUrl() const
{
	return ServerBaseUrl;
}

bool UDesignerHttpControlSubsystem::PollCommandEndpoint(const FString& EndpointPath)
{
	EnsureRealtimeConnection();

	if (IsRealtimeConnected())
	{
		return false;
	}

	if (ServerBaseUrl.IsEmpty())
	{
		OnHttpRequestFailed.Broadcast(TEXT("ServerBaseUrl is empty."));
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(BuildUrl(EndpointPath));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetTimeout(30.0f);
	Request->SetActivityTimeout(35.0f);
	Request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (!Response.IsValid())
		{
			OnHttpRequestFailed.Broadcast(TEXT("HTTP poll failed."));
			return;
		}

		const int32 ResponseCode = Response->GetResponseCode();
		const FString Payload = Response->GetContentAsString().TrimStartAndEnd();
		if (ResponseCode == EHttpResponseCodes::NoContent || Payload.IsEmpty() || Payload.Contains(TEXT("\"empty\":true")))
		{
			OnRemoteCommandExecuted.Broadcast(FDesignerControlResult());
			return;
		}

		if (!bWasSuccessful && !EHttpResponseCodes::IsOk(ResponseCode))
		{
			OnHttpRequestFailed.Broadcast(FString::Printf(TEXT("HTTP poll returned %d."), ResponseCode));
			return;
		}

		UDesignerRuntimeControlSubsystem* RuntimeControl = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDesignerRuntimeControlSubsystem>() : nullptr;
		if (!RuntimeControl)
		{
			OnHttpRequestFailed.Broadcast(TEXT("Runtime control subsystem is unavailable."));
			return;
		}

		OnRemoteCommandExecuted.Broadcast(RuntimeControl->ExecuteControlJson(Payload));
	});

	return Request->ProcessRequest();
}

bool UDesignerHttpControlSubsystem::PushRuntimeState(const FString& EndpointPath)
{
	EnsureRealtimeConnection();

	if (ServerBaseUrl.IsEmpty())
	{
		OnHttpRequestFailed.Broadcast(TEXT("ServerBaseUrl is empty."));
		return false;
	}

	UDesignerRuntimeControlSubsystem* RuntimeControl = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDesignerRuntimeControlSubsystem>() : nullptr;
	if (!RuntimeControl)
	{
		OnHttpRequestFailed.Broadcast(TEXT("Runtime control subsystem is unavailable."));
		return false;
	}

	if (IsRealtimeConnected())
	{
		SendRuntimeStateOverRealtime();
		return true;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(BuildUrl(EndpointPath));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RuntimeControl->GetRuntimeStateJson());
	Request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (!bWasSuccessful || !Response.IsValid())
		{
			OnHttpRequestFailed.Broadcast(TEXT("HTTP state push failed."));
			return;
		}

		if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			OnHttpRequestFailed.Broadcast(FString::Printf(TEXT("HTTP state push returned %d."), Response->GetResponseCode()));
			return;
		}

		OnHttpStatePushed.Broadcast(Response->GetContentAsString());
	});

	return Request->ProcessRequest();
}

void UDesignerHttpControlSubsystem::EnsureRealtimeConnection()
{
	if (ServerBaseUrl.IsEmpty() || IsRealtimeConnected() || bRealtimeConnecting || bReconnectTimerActive)
	{
		return;
	}

	bRealtimeConnecting = true;
	if (!ConnectRealtimeSocket())
	{
		bRealtimeConnecting = false;
		ScheduleRealtimeReconnect(RealtimeReconnectDelaySeconds);
		return;
	}

	bRealtimeConnecting = false;
	bReconnectTimerActive = false;
	RealtimeReconnectDelaySeconds = 0.5f;
	UE_LOG(LogTemp, Log, TEXT("Realtime TCP connected -> %s:%d"), *BuildRealtimeHost(), BuildRealtimePort());
	ScheduleRealtimePump();
	SendRuntimeStateOverRealtime();
}

bool UDesignerHttpControlSubsystem::IsRealtimeConnected() const
{
	return RealtimeSocket != nullptr;
}

bool UDesignerHttpControlSubsystem::IsRealtimeConnecting() const
{
	return bRealtimeConnecting || bReconnectTimerActive;
}

FString UDesignerHttpControlSubsystem::BuildUrl(const FString& EndpointPath) const
{
	const bool bBaseEndsWithSlash = ServerBaseUrl.EndsWith(TEXT("/"));
	const bool bPathStartsWithSlash = EndpointPath.StartsWith(TEXT("/"));

	if (bBaseEndsWithSlash && bPathStartsWithSlash)
	{
		return ServerBaseUrl.LeftChop(1) + EndpointPath;
	}

	if (!bBaseEndsWithSlash && !bPathStartsWithSlash)
	{
		return ServerBaseUrl + TEXT("/") + EndpointPath;
	}

	return ServerBaseUrl + EndpointPath;
}

FString UDesignerHttpControlSubsystem::BuildRealtimeHost() const
{
	FString Working = ServerBaseUrl;
	Working.RemoveFromStart(TEXT("http://"));
	Working.RemoveFromStart(TEXT("https://"));
	FString HostPortPath;
	if (!Working.Split(TEXT("/"), &HostPortPath, nullptr))
	{
		HostPortPath = Working;
	}

	FString Host;
	if (!HostPortPath.Split(TEXT(":"), &Host, nullptr))
	{
		Host = HostPortPath;
	}

	return Host.IsEmpty() ? TEXT("127.0.0.1") : Host;
}

int32 UDesignerHttpControlSubsystem::BuildRealtimePort() const
{
	return 18081;
}

bool UDesignerHttpControlSubsystem::ConnectRealtimeSocket()
{
	CloseRealtimeSocket();

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	bool bIsValid = false;
	TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
	Address->SetIp(*BuildRealtimeHost(), bIsValid);
	Address->SetPort(BuildRealtimePort());
	if (!bIsValid)
	{
		OnHttpRequestFailed.Broadcast(TEXT("Realtime host address is invalid."));
		return false;
	}

	RealtimeSocket = FTcpSocketBuilder(TEXT("DesignerRealtimeTcp"))
		.AsReusable()
		.WithSendBufferSize(1024 * 64)
		.WithReceiveBufferSize(1024 * 64);
	if (!RealtimeSocket)
	{
		return false;
	}

	RealtimeSocket->SetNonBlocking(true);
	RealtimeSocket->SetNoDelay(true);

	if (!RealtimeSocket->Connect(*Address))
	{
		ESocketErrors LastError = SocketSubsystem->GetLastErrorCode();
		const bool bWouldBlock = LastError == SE_EWOULDBLOCK || LastError == SE_NO_ERROR;
		if (!bWouldBlock)
		{
			UE_LOG(LogTemp, Warning, TEXT("Realtime TCP connect failed: %s"), SocketSubsystem->GetSocketError(LastError));
			CloseRealtimeSocket();
			return false;
		}
	}

	return true;
}

void UDesignerHttpControlSubsystem::CloseRealtimeSocket()
{
	if (RealtimeSocket)
	{
		RealtimeSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(RealtimeSocket);
		RealtimeSocket = nullptr;
	}
	RealtimeReceiveBuffer.Reset();
}

void UDesignerHttpControlSubsystem::ScheduleRealtimeReconnect(float DelaySeconds)
{
	if (!GetGameInstance() || !GetGameInstance()->GetWorld() || ServerBaseUrl.IsEmpty())
	{
		return;
	}

	if (bRealtimeConnecting || IsRealtimeConnected() || bReconnectTimerActive)
	{
		return;
	}

	bReconnectTimerActive = true;
	const float RetryDelay = FMath::Clamp(DelaySeconds, 0.25f, 5.0f);
	RealtimeReconnectDelaySeconds = FMath::Min(RetryDelay * 2.0f, 5.0f);
	GetGameInstance()->GetWorld()->GetTimerManager().SetTimer(RealtimeReconnectTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bReconnectTimerActive = false;
		EnsureRealtimeConnection();
	}), RetryDelay, false);
}

void UDesignerHttpControlSubsystem::ScheduleRealtimePump()
{
	if (!GetGameInstance() || !GetGameInstance()->GetWorld())
	{
		return;
	}

	GetGameInstance()->GetWorld()->GetTimerManager().ClearTimer(RealtimePumpTimerHandle);
	GetGameInstance()->GetWorld()->GetTimerManager().SetTimer(RealtimePumpTimerHandle, this, &UDesignerHttpControlSubsystem::PumpRealtimeSocket, 0.02f, true, 0.02f);
}

void UDesignerHttpControlSubsystem::PumpRealtimeSocket()
{
	if (!RealtimeSocket)
	{
		if (GetGameInstance() && GetGameInstance()->GetWorld())
		{
			GetGameInstance()->GetWorld()->GetTimerManager().ClearTimer(RealtimePumpTimerHandle);
		}
		return;
	}

	uint32 PendingSize = 0;
	while (RealtimeSocket->HasPendingData(PendingSize) && PendingSize > 0)
	{
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(FMath::Min<uint32>(PendingSize, 65536));
		int32 BytesRead = 0;
		if (!RealtimeSocket->Recv(Buffer.GetData(), Buffer.Num(), BytesRead) || BytesRead <= 0)
		{
			CloseRealtimeSocket();
			ScheduleRealtimeReconnect(RealtimeReconnectDelaySeconds);
			return;
		}

		const FUTF8ToTCHAR Utf8Chunk(reinterpret_cast<const ANSICHAR*>(Buffer.GetData()), BytesRead); RealtimeReceiveBuffer += FString(Utf8Chunk.Length(), Utf8Chunk.Get());
	}

	FString Line;
	while (RealtimeReceiveBuffer.Split(TEXT("\n"), &Line, &RealtimeReceiveBuffer))
	{
		Line = Line.TrimStartAndEnd();
		if (!Line.IsEmpty())
		{
			HandleRealtimeMessage(Line);
		}
	}
}

bool UDesignerHttpControlSubsystem::SendRealtimePayload(const FString& Payload)
{
	if (!RealtimeSocket)
	{
		return false;
	}

	FString NormalizedPayload = Payload;
	NormalizedPayload.ReplaceInline(TEXT("\r"), TEXT(""));
	NormalizedPayload.ReplaceInline(TEXT("\n"), TEXT(""));
	FTCHARToUTF8 Utf8Payload(*(NormalizedPayload + TEXT("\n")));
	int32 BytesSent = 0;
	if (!RealtimeSocket->Send(reinterpret_cast<const uint8*>(Utf8Payload.Get()), Utf8Payload.Length(), BytesSent) || BytesSent <= 0)
	{
		CloseRealtimeSocket();
		ScheduleRealtimeReconnect(RealtimeReconnectDelaySeconds);
		return false;
	}

	return true;
}

void UDesignerHttpControlSubsystem::SendRuntimeStateOverRealtime()
{
	if (!RealtimeSocket)
	{
		return;
	}

	UDesignerRuntimeControlSubsystem* RuntimeControl = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDesignerRuntimeControlSubsystem>() : nullptr;
	if (!RuntimeControl)
	{
		return;
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("type"), TEXT("runtime_state"));

	TSharedPtr<FJsonObject> StateObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RuntimeControl->GetRuntimeStateJson());
	if (FJsonSerializer::Deserialize(Reader, StateObject) && StateObject.IsValid())
	{
		RootObject->SetObjectField(TEXT("state"), StateObject);
	}

	FString Payload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	SendRealtimePayload(Payload);
}

void UDesignerHttpControlSubsystem::HandleRealtimeMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return;
	}

	FString MessageType;
	RootObject->TryGetStringField(TEXT("type"), MessageType);
	if (!MessageType.Equals(TEXT("command"), ESearchCase::IgnoreCase))
	{
		return;
	}

	const TSharedPtr<FJsonObject>* CommandObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("payload"), CommandObject) || !CommandObject || !CommandObject->IsValid())
	{
		return;
	}

	FString CommandPayload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&CommandPayload);
	FJsonSerializer::Serialize(CommandObject->ToSharedRef(), Writer);

	UDesignerRuntimeControlSubsystem* RuntimeControl = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDesignerRuntimeControlSubsystem>() : nullptr;
	if (!RuntimeControl)
	{
		OnHttpRequestFailed.Broadcast(TEXT("Runtime control subsystem is unavailable."));
		return;
	}

	const FDesignerControlResult Result = RuntimeControl->ExecuteControlJson(CommandPayload);
	OnRemoteCommandExecuted.Broadcast(Result);
	SendRuntimeStateOverRealtime();
}
