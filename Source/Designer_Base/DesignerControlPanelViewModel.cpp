#include "DesignerControlPanelViewModel.h"

#include "DesignerHttpControlSubsystem.h"
#include "DesignerRuntimeControlSubsystem.h"
#include "DesignerTemplateRegistrySubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UDesignerControlPanelViewModel* UDesignerControlPanelViewModel::CreateDesignerControlPanelViewModel(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UDesignerControlPanelViewModel* ViewModel = NewObject<UDesignerControlPanelViewModel>(WorldContextObject);
	if (ViewModel)
	{
		ViewModel->Initialize(WorldContextObject);
	}

	return ViewModel;
}

void UDesignerControlPanelViewModel::Refresh()
{
	if (UDesignerTemplateRegistrySubsystem* Registry = GetRegistry())
	{
		Registry->RefreshTemplateCatalog();
		CachedTemplates = Registry->GetAvailableTemplates();
	}
	else
	{
		CachedTemplates.Reset();
	}

	if (UDesignerRuntimeControlSubsystem* RuntimeControl = GetRuntimeControl())
	{
		CachedState = RuntimeControl->GetRuntimeState();
		CachedTemplates = CachedState.AvailableTemplates;
	}
	else
	{
		CachedState = FDesignerRuntimeState();
	}

	if (CachedState.bHasActiveTemplate)
	{
		SelectedTemplateCode = CachedState.ActiveTemplateCode;
	}

	RebuildEditableFields();
	OnStateChanged.Broadcast();
}

bool UDesignerControlPanelViewModel::SelectTemplate(const FString& TemplateCode)
{
	UDesignerTemplateRegistrySubsystem* Registry = GetRegistry();
	if (!Registry)
	{
		OnMessage.Broadcast(TEXT("Template registry subsystem is unavailable."));
		return false;
	}

	const bool bActivated = Registry->ActivateTemplateByCode(TemplateCode);
	if (!bActivated)
	{
		OnMessage.Broadcast(FString::Printf(TEXT("Failed to activate template: %s"), *TemplateCode));
		return false;
	}

	SelectedTemplateCode = TemplateCode;
	Refresh();
	return true;
}

void UDesignerControlPanelViewModel::SetSelectedTemplateCode(const FString& TemplateCode)
{
	SelectedTemplateCode = TemplateCode;
	RebuildEditableFields();
	OnStateChanged.Broadcast();
}

void UDesignerControlPanelViewModel::SetFieldValue(FName FieldKey, const FString& InValue)
{
	for (FDesignerEditableFieldValue& Field : EditableFields)
	{
		if (Field.Key == FieldKey)
		{
			Field.Value = InValue;
			break;
		}
	}

	OnStateChanged.Broadcast();
}

bool UDesignerControlPanelViewModel::ApplyCurrentFields()
{
	UDesignerTemplateRegistrySubsystem* Registry = GetRegistry();
	if (!Registry)
	{
		OnMessage.Broadcast(TEXT("Template registry subsystem is unavailable."));
		return false;
	}

	if (!Registry->GetActiveTemplateActor())
	{
		if (SelectedTemplateCode.IsEmpty() || !Registry->ActivateTemplateByCode(SelectedTemplateCode))
		{
			OnMessage.Broadcast(TEXT("No active template is selected."));
			return false;
		}
	}

	Registry->ApplyFieldsToActiveTemplate(BuildFieldMap());
	OnMessage.Broadcast(TEXT("Fields applied."));
	Refresh();
	return true;
}

bool UDesignerControlPanelViewModel::ApplyFieldsFromJson(const FString& FieldsJson)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FieldsJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OnMessage.Broadcast(TEXT("Fields JSON is invalid."));
		return false;
	}

	for (FDesignerEditableFieldValue& Field : EditableFields)
	{
		Field.Value = Field.DefaultValue;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : RootObject->Values)
	{
		FString StringValue;
		if (Entry.Value.IsValid())
		{
			switch (Entry.Value->Type)
			{
			case EJson::String:
				StringValue = Entry.Value->AsString();
				break;
			case EJson::Boolean:
				StringValue = Entry.Value->AsBool() ? TEXT("true") : TEXT("false");
				break;
			case EJson::Number:
				StringValue = FString::SanitizeFloat(Entry.Value->AsNumber());
				break;
			default:
				StringValue = Entry.Value->AsString();
				break;
			}
		}

		SetFieldValue(FName(*Entry.Key), StringValue);
	}

	return ApplyCurrentFields();
}

bool UDesignerControlPanelViewModel::TakeIn()
{
	UDesignerTemplateRegistrySubsystem* Registry = GetRegistry();
	if (!Registry || !Registry->GetActiveTemplateActor())
	{
		OnMessage.Broadcast(TEXT("No active template is selected."));
		return false;
	}

	Registry->TakeActiveTemplateIn();
	Refresh();
	return true;
}

bool UDesignerControlPanelViewModel::TakeOut()
{
	UDesignerTemplateRegistrySubsystem* Registry = GetRegistry();
	if (!Registry || !Registry->GetActiveTemplateActor())
	{
		OnMessage.Broadcast(TEXT("No active template is selected."));
		return false;
	}

	Registry->TakeActiveTemplateOut();
	Refresh();
	return true;
}

bool UDesignerControlPanelViewModel::ClearActiveTemplate()
{
	UDesignerTemplateRegistrySubsystem* Registry = GetRegistry();
	if (!Registry)
	{
		OnMessage.Broadcast(TEXT("Template registry subsystem is unavailable."));
		return false;
	}

	Registry->ClearActiveTemplate();
	Refresh();
	return true;
}

bool UDesignerControlPanelViewModel::PollRemoteCommand(const FString& EndpointPath)
{
	UDesignerHttpControlSubsystem* HttpControl = GetHttpControl();
	if (!HttpControl)
	{
		OnMessage.Broadcast(TEXT("HTTP control subsystem is unavailable."));
		return false;
	}

	UE_LOG(LogTemp, Verbose, TEXT("ViewModel polling remote command endpoint: %s"), *EndpointPath);
	return HttpControl->PollCommandEndpoint(EndpointPath);
}

bool UDesignerControlPanelViewModel::PushRuntimeState(const FString& EndpointPath)
{
	UDesignerHttpControlSubsystem* HttpControl = GetHttpControl();
	if (!HttpControl)
	{
		OnMessage.Broadcast(TEXT("HTTP control subsystem is unavailable."));
		return false;
	}

	UE_LOG(LogTemp, Verbose, TEXT("ViewModel pushing runtime state endpoint: %s"), *EndpointPath);
	return HttpControl->PushRuntimeState(EndpointPath);
}

void UDesignerControlPanelViewModel::StartAutoPoll(float IntervalSeconds, const FString& EndpointPath)
{
	AutoPollEndpointPath = EndpointPath;
	AutoPollRetryDelaySeconds = FMath::Max(IntervalSeconds, 0.05f);
	bAutoPollingEnabled = true;
	ScheduleNextAutoPoll();
}

void UDesignerControlPanelViewModel::StopAutoPoll()
{
	bAutoPollingEnabled = false;
	bAutoPollRequestInFlight = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoPollTimerHandle);
	}
}

bool UDesignerControlPanelViewModel::IsAutoPolling() const
{
	return bAutoPollingEnabled;
}

void UDesignerControlPanelViewModel::SetServerBaseUrl(const FString& InBaseUrl)
{
	if (UDesignerHttpControlSubsystem* HttpControl = GetHttpControl())
	{
		HttpControl->SetServerBaseUrl(InBaseUrl);
		OnStateChanged.Broadcast();
	}
}

FString UDesignerControlPanelViewModel::GetServerBaseUrl() const
{
	if (const UDesignerHttpControlSubsystem* HttpControl = GetHttpControl())
	{
		return HttpControl->GetServerBaseUrl();
	}

	return FString();
}

TArray<FDesignerTemplateDescriptor> UDesignerControlPanelViewModel::GetTemplates() const
{
	return CachedTemplates;
}

FDesignerRuntimeState UDesignerControlPanelViewModel::GetRuntimeState() const
{
	return CachedState;
}

FString UDesignerControlPanelViewModel::GetRuntimeStateJson() const
{
	if (const UDesignerRuntimeControlSubsystem* RuntimeControl = GetRuntimeControl())
	{
		return RuntimeControl->GetRuntimeStateJson();
	}

	return FString();
}

TArray<FDesignerEditableFieldValue> UDesignerControlPanelViewModel::GetEditableFields() const
{
	return EditableFields;
}

FString UDesignerControlPanelViewModel::GetEditableFieldsJson() const
{
	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	for (const FDesignerEditableFieldValue& Field : EditableFields)
	{
		if (!Field.Key.IsNone())
		{
			RootObject->SetStringField(Field.Key.ToString(), Field.Value);
		}
	}

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	return Output;
}

FString UDesignerControlPanelViewModel::GetSelectedTemplateCode() const
{
	return SelectedTemplateCode;
}

UWorld* UDesignerControlPanelViewModel::GetWorld() const
{
	return WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
}

void UDesignerControlPanelViewModel::Initialize(UObject* InWorldContextObject)
{
	WorldContextObject = InWorldContextObject;

	UDesignerHttpControlSubsystem* HttpControl = GetHttpControl();
	if (HttpControl && !bHttpHandlersBound)
	{
		HttpControl->OnRemoteCommandExecuted.AddDynamic(this, &UDesignerControlPanelViewModel::HandleRemoteResult);
		HttpControl->OnHttpRequestFailed.AddDynamic(this, &UDesignerControlPanelViewModel::HandleHttpFailure);
		HttpControl->OnHttpStatePushed.AddDynamic(this, &UDesignerControlPanelViewModel::HandleHttpStatePushed);
		bHttpHandlersBound = true;
	}

	Refresh();
}

void UDesignerControlPanelViewModel::RebuildEditableFields()
{
	EditableFields.Reset();

	FDesignerTemplateDescriptor SelectedTemplate;
	bool bHasSelectedTemplate = false;
	for (const FDesignerTemplateDescriptor& Template : CachedTemplates)
	{
		if (Template.TemplateCode.Equals(SelectedTemplateCode, ESearchCase::IgnoreCase))
		{
			SelectedTemplate = Template;
			bHasSelectedTemplate = true;
			break;
		}
	}

	if (!bHasSelectedTemplate && CachedState.bHasActiveTemplate)
	{
		for (const FDesignerTemplateDescriptor& Template : CachedTemplates)
		{
			if (Template.TemplateCode.Equals(CachedState.ActiveTemplateCode, ESearchCase::IgnoreCase))
			{
				SelectedTemplate = Template;
				bHasSelectedTemplate = true;
				break;
			}
		}
	}

	if (!bHasSelectedTemplate)
	{
		return;
	}

	for (const FDesignerTemplateParamDefinition& Parameter : SelectedTemplate.Parameters)
	{
		FDesignerEditableFieldValue Field;
		Field.Key = Parameter.Key;
		Field.Label = Parameter.Label;
		Field.Type = Parameter.Type;
		Field.bRequired = Parameter.bRequired;
		Field.DefaultValue = Parameter.DefaultValue;
		Field.Value = Parameter.DefaultValue;

		if (const FString* CurrentValue = CachedState.ActiveFields.Find(Parameter.Key))
		{
			Field.Value = *CurrentValue;
		}

		EditableFields.Add(Field);
	}
}

TMap<FName, FString> UDesignerControlPanelViewModel::BuildFieldMap() const
{
	TMap<FName, FString> Fields;
	for (const FDesignerEditableFieldValue& Field : EditableFields)
	{
		if (!Field.Key.IsNone())
		{
			Fields.Add(Field.Key, Field.Value);
		}
	}

	return Fields;
}

UDesignerTemplateRegistrySubsystem* UDesignerControlPanelViewModel::GetRegistry() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UDesignerTemplateRegistrySubsystem>();
		}
	}

	return nullptr;
}

UDesignerRuntimeControlSubsystem* UDesignerControlPanelViewModel::GetRuntimeControl() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UDesignerRuntimeControlSubsystem>();
		}
	}

	return nullptr;
}

UDesignerHttpControlSubsystem* UDesignerControlPanelViewModel::GetHttpControl() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UDesignerHttpControlSubsystem>();
		}
	}

	return nullptr;
}

void UDesignerControlPanelViewModel::HandleRemoteResult(const FDesignerControlResult& Result)
{
	bAutoPollRequestInFlight = false;

	if (Result.Command.IsEmpty())
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("ViewModel received empty remote result."));
		if (bAutoPollingEnabled)
		{
			ScheduleNextAutoPoll();
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ViewModel handled remote result command=%s success=%s message=%s"),
		*Result.Command,
		Result.bSuccess ? TEXT("true") : TEXT("false"),
		*Result.Message);

	CachedState = Result.State;
	CachedTemplates = CachedState.AvailableTemplates;
	if (CachedState.bHasActiveTemplate)
	{
		SelectedTemplateCode = CachedState.ActiveTemplateCode;
	}

	RebuildEditableFields();
	OnMessage.Broadcast(Result.Message);
	OnStateChanged.Broadcast();
	PushRuntimeState(TEXT("/control/state"));

	if (bAutoPollingEnabled)
	{
		ScheduleNextAutoPoll();
	}
}

void UDesignerControlPanelViewModel::HandleHttpFailure(const FString& Message)
{
	bAutoPollRequestInFlight = false;
	OnMessage.Broadcast(Message);

	if (bAutoPollingEnabled)
	{
		ScheduleNextAutoPoll(AutoPollRetryDelaySeconds);
	}
}

void UDesignerControlPanelViewModel::HandleHttpStatePushed(const FString& Message)
{
	OnMessage.Broadcast(Message);
}

void UDesignerControlPanelViewModel::HandleAutoPollTick()
{
	if (!bAutoPollingEnabled || bAutoPollRequestInFlight)
	{
		return;
	}

	bAutoPollRequestInFlight = PollRemoteCommand(AutoPollEndpointPath);
	if (!bAutoPollRequestInFlight)
	{
		ScheduleNextAutoPoll(AutoPollRetryDelaySeconds);
	}
}

void UDesignerControlPanelViewModel::ScheduleNextAutoPoll(float DelaySeconds)
{
	if (!bAutoPollingEnabled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OnMessage.Broadcast(TEXT("World is unavailable for auto polling."));
		return;
	}

	World->GetTimerManager().ClearTimer(AutoPollTimerHandle);
	if (DelaySeconds <= 0.0f)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			HandleAutoPollTick();
		}));
		return;
	}

	World->GetTimerManager().SetTimer(
		AutoPollTimerHandle,
		this,
		&UDesignerControlPanelViewModel::HandleAutoPollTick,
		DelaySeconds,
		false);
}
