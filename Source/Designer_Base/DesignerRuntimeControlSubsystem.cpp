#include "DesignerRuntimeControlSubsystem.h"

#include "DesignerTemplateActor.h"
#include "DesignerTemplateRegistrySubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Logging/LogMacros.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UDesignerRuntimeControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

FDesignerControlResult UDesignerRuntimeControlSubsystem::ExecuteControlCommand(const FDesignerControlCommand& Command)
{
	UE_LOG(LogTemp, Log, TEXT("ExecuteControlCommand requestId=%s command=%s templateCode=%s fields=%d"),
		*Command.RequestId,
		*Command.Command,
		*Command.TemplateCode,
		Command.Fields.Num());

	UDesignerTemplateRegistrySubsystem* Registry = GetRegistry();
	if (!Registry)
	{
		const FDesignerControlResult Result = BuildResult(false, Command, TEXT("Template registry subsystem is unavailable."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	const FString NormalizedCommand = Command.Command.ToLower();

	if (NormalizedCommand == TEXT("activate"))
	{
		if (Command.TemplateCode.IsEmpty())
		{
			const FDesignerControlResult Result = BuildResult(false, Command, TEXT("activate requires templateCode."));
			LastControlResultJson = SerializeResultToJson(Result);
			OnControlExecuted.Broadcast(Result);
			return Result;
		}

		const FDesignerControlResult Result = BuildResult(
			Registry->ActivateTemplateByCode(Command.TemplateCode),
			Command,
			Registry->GetActiveTemplateActor() ? TEXT("Template activated.") : TEXT("Template activation failed."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	if (NormalizedCommand == TEXT("apply"))
	{
		if (!Registry->GetActiveTemplateActor())
		{
			const FDesignerControlResult Result = BuildResult(false, Command, TEXT("No active template to apply fields to."));
			LastControlResultJson = SerializeResultToJson(Result);
			OnControlExecuted.Broadcast(Result);
			return Result;
		}

		Registry->ApplyFieldsToActiveTemplate(Command.Fields);
		const FDesignerControlResult Result = BuildResult(true, Command, TEXT("Fields applied."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	if (NormalizedCommand == TEXT("takein"))
	{
		if (!Registry->GetActiveTemplateActor())
		{
			const FDesignerControlResult Result = BuildResult(false, Command, TEXT("No active template to take in."));
			LastControlResultJson = SerializeResultToJson(Result);
			OnControlExecuted.Broadcast(Result);
			return Result;
		}

		Registry->TakeActiveTemplateIn();
		const FDesignerControlResult Result = BuildResult(true, Command, TEXT("TakeIn executed."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	if (NormalizedCommand == TEXT("takeout"))
	{
		if (!Registry->GetActiveTemplateActor())
		{
			const FDesignerControlResult Result = BuildResult(false, Command, TEXT("No active template to take out."));
			LastControlResultJson = SerializeResultToJson(Result);
			OnControlExecuted.Broadcast(Result);
			return Result;
		}

		Registry->TakeActiveTemplateOut();
		const FDesignerControlResult Result = BuildResult(true, Command, TEXT("TakeOut executed."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	if (NormalizedCommand == TEXT("clear"))
	{
		Registry->ClearActiveTemplate();
		const FDesignerControlResult Result = BuildResult(true, Command, TEXT("Active template cleared."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	if (NormalizedCommand == TEXT("refresh"))
	{
		Registry->RefreshTemplateCatalog();
		const FDesignerControlResult Result = BuildResult(true, Command, TEXT("Template catalog refreshed."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	if (NormalizedCommand == TEXT("status"))
	{
		const FDesignerControlResult Result = BuildResult(true, Command, TEXT("Runtime state collected."));
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	const FDesignerControlResult Result = BuildResult(false, Command, FString::Printf(TEXT("Unsupported command: %s"), *Command.Command));
	LastControlResultJson = SerializeResultToJson(Result);
	OnControlExecuted.Broadcast(Result);
	return Result;
}

FDesignerControlResult UDesignerRuntimeControlSubsystem::ExecuteControlJson(const FString& JsonPayload)
{
	FDesignerControlCommand Command;
	FString Error;
	if (!TryParseCommandJson(JsonPayload, Command, Error))
	{
		const FDesignerControlResult Result = BuildResult(false, Command, Error);
		LastControlResultJson = SerializeResultToJson(Result);
		OnControlExecuted.Broadcast(Result);
		return Result;
	}

	return ExecuteControlCommand(Command);
}

FString UDesignerRuntimeControlSubsystem::ExecuteControlJsonAndGetResponse(const FString& JsonPayload)
{
	const FDesignerControlResult Result = ExecuteControlJson(JsonPayload);
	return SerializeResultToJson(Result);
}

FDesignerRuntimeState UDesignerRuntimeControlSubsystem::GetRuntimeState() const
{
	FDesignerRuntimeState State;

	if (const UDesignerTemplateRegistrySubsystem* Registry = GetRegistry())
	{
		State.AvailableTemplates = Registry->GetAvailableTemplates();

		if (ADesignerTemplateActor* ActiveTemplate = Registry->GetActiveTemplateActor())
		{
			State.bHasActiveTemplate = true;
			State.bIsOnAir = ActiveTemplate->IsOnAir();
			State.ActiveTemplateCode = ActiveTemplate->TemplateCode;
			State.ActiveTemplateName = ActiveTemplate->TemplateName;
			ActiveTemplate->CollectCurrentFields(State.ActiveFields);
		}
	}

	return State;
}

FString UDesignerRuntimeControlSubsystem::GetRuntimeStateJson() const
{
	return SerializeStateToJson(GetRuntimeState());
}

FString UDesignerRuntimeControlSubsystem::GetLastControlResultJson() const
{
	return LastControlResultJson;
}

FDesignerControlResult UDesignerRuntimeControlSubsystem::BuildResult(bool bSuccess, const FDesignerControlCommand& Command, const FString& Message) const
{
	FDesignerControlResult Result;
	Result.bSuccess = bSuccess;
	Result.RequestId = Command.RequestId;
	Result.Command = Command.Command;
	Result.Message = Message;
	Result.State = GetRuntimeState();
	return Result;
}

bool UDesignerRuntimeControlSubsystem::TryParseCommandJson(const FString& JsonPayload, FDesignerControlCommand& OutCommand, FString& OutError) const
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Invalid control JSON payload.");
		return false;
	}

	RootObject->TryGetStringField(TEXT("requestId"), OutCommand.RequestId);
	RootObject->TryGetStringField(TEXT("command"), OutCommand.Command);
	RootObject->TryGetStringField(TEXT("templateCode"), OutCommand.TemplateCode);

	const TSharedPtr<FJsonObject>* FieldsObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("fields"), FieldsObject) && FieldsObject && FieldsObject->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*FieldsObject)->Values)
		{
			if (!Entry.Value.IsValid())
			{
				continue;
			}

			FString StringValue;
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

			OutCommand.Fields.Add(FName(*Entry.Key), StringValue);
		}
	}

	if (OutCommand.Command.IsEmpty())
	{
		OutError = TEXT("command is required.");
		return false;
	}

	return true;
}

FString UDesignerRuntimeControlSubsystem::SerializeStateToJson(const FDesignerRuntimeState& State) const
{
	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetBoolField(TEXT("hasActiveTemplate"), State.bHasActiveTemplate);
	RootObject->SetBoolField(TEXT("isOnAir"), State.bIsOnAir);
	RootObject->SetStringField(TEXT("activeTemplateCode"), State.ActiveTemplateCode);
	RootObject->SetStringField(TEXT("activeTemplateName"), State.ActiveTemplateName.ToString());

	const TSharedPtr<FJsonObject> FieldsObject = MakeShared<FJsonObject>();
	for (const TPair<FName, FString>& Entry : State.ActiveFields)
	{
		FieldsObject->SetStringField(Entry.Key.ToString(), Entry.Value);
	}
	RootObject->SetObjectField(TEXT("activeFields"), FieldsObject);

	TArray<TSharedPtr<FJsonValue>> TemplateArray;
	for (const FDesignerTemplateDescriptor& Template : State.AvailableTemplates)
	{
		const TSharedPtr<FJsonObject> TemplateObject = MakeShared<FJsonObject>();
		TemplateObject->SetStringField(TEXT("templateCode"), Template.TemplateCode);
		TemplateObject->SetStringField(TEXT("templateName"), Template.TemplateName.ToString());
		TemplateObject->SetStringField(TEXT("category"), StaticEnum<EDesignerTemplateCategory>()->GetNameStringByValue(static_cast<int64>(Template.Category)).ToLower());
		TemplateObject->SetStringField(TEXT("controllerClassPath"), Template.ControllerClassPath.ToString());

		TArray<TSharedPtr<FJsonValue>> ActionValues;
		for (const FName& Action : Template.SupportedActions)
		{
			ActionValues.Add(MakeShared<FJsonValueString>(Action.ToString()));
		}
		TemplateObject->SetArrayField(TEXT("actions"), ActionValues);

		TArray<TSharedPtr<FJsonValue>> ParameterValues;
		for (const FDesignerTemplateParamDefinition& Parameter : Template.Parameters)
		{
			const TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
			ParameterObject->SetStringField(TEXT("key"), Parameter.Key.ToString());
			ParameterObject->SetStringField(TEXT("label"), Parameter.Label.ToString());
			ParameterObject->SetStringField(TEXT("type"), StaticEnum<EDesignerTemplateParamType>()->GetNameStringByValue(static_cast<int64>(Parameter.Type)).ToLower());
			ParameterObject->SetBoolField(TEXT("required"), Parameter.bRequired);
			ParameterObject->SetStringField(TEXT("default"), Parameter.DefaultValue);
			ParameterValues.Add(MakeShared<FJsonValueObject>(ParameterObject));
		}
		TemplateObject->SetArrayField(TEXT("parameters"), ParameterValues);

		TemplateArray.Add(MakeShared<FJsonValueObject>(TemplateObject));
	}
	RootObject->SetArrayField(TEXT("availableTemplates"), TemplateArray);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	return Output;
}

FString UDesignerRuntimeControlSubsystem::SerializeResultToJson(const FDesignerControlResult& Result) const
{
	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetBoolField(TEXT("success"), Result.bSuccess);
	RootObject->SetStringField(TEXT("requestId"), Result.RequestId);
	RootObject->SetStringField(TEXT("command"), Result.Command);
	RootObject->SetStringField(TEXT("message"), Result.Message);

	TSharedPtr<FJsonObject> StateObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SerializeStateToJson(Result.State));
	if (FJsonSerializer::Deserialize(Reader, StateObject) && StateObject.IsValid())
	{
		RootObject->SetObjectField(TEXT("state"), StateObject);
	}

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	return Output;
}

UDesignerTemplateRegistrySubsystem* UDesignerRuntimeControlSubsystem::GetRegistry() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UDesignerTemplateRegistrySubsystem>() : nullptr;
}
