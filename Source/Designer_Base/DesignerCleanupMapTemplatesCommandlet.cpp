#include "DesignerCleanupMapTemplatesCommandlet.h"

#if WITH_EDITOR

#include "DesignerRuntimePreviewBootstrapper.h"
#include "DesignerTemplateActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/WidgetComponent.h"
#include "FileHelpers.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#endif

UDesignerCleanupMapTemplatesCommandlet::UDesignerCleanupMapTemplatesCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UDesignerCleanupMapTemplatesCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	return 1;
#else
	const FString MapPath = TEXT("/Game/AA");
	if (!UEditorLoadingAndSavingUtils::LoadMap(MapPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load map %s"), *MapPath);
		return 2;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Editor world is unavailable after loading map."));
		return 3;
	}

	TArray<AActor*> ActorsToRemove;
	int32 ActorCount = 0;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		++ActorCount;
		UE_LOG(LogTemp, Display, TEXT("Map actor: %s (%s)"), *Actor->GetActorLabel(), *Actor->GetClass()->GetPathName());
		if (ShouldRemoveActor(Actor))
		{
			ActorsToRemove.Add(Actor);
		}
	}

	int32 RemovedCount = 0;
	for (AActor* Actor : ActorsToRemove)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		UE_LOG(LogTemp, Display, TEXT("Removing actor: %s (%s)"), *Actor->GetActorLabel(), *Actor->GetClass()->GetPathName());
		Actor->Modify();
		World->EditorDestroyActor(Actor, true);
		++RemovedCount;
	}

	int32 RemovedLevelScriptNodes = 0;
	if (ULevelScriptBlueprint* LevelScriptBlueprint = World->PersistentLevel ? World->PersistentLevel->GetLevelScriptBlueprint(true) : nullptr)
	{
		TSet<UEdGraphNode*> NodesToRemove;
		for (UEdGraph* Graph : LevelScriptBlueprint->UbergraphPages)
		{
			if (!IsValid(Graph))
			{
				continue;
			}

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!IsValid(Node))
				{
					continue;
				}

				if (Node->GetClass()->GetName() != TEXT("K2Node_CreateWidget"))
				{
					continue;
				}

				bool bTargetsLowerThirdWidget = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin == nullptr)
					{
						continue;
					}

					if (Pin->DefaultObject && Pin->DefaultObject->GetPathName().Contains(TEXT("WBP_LowerThirdBasic")))
					{
						bTargetsLowerThirdWidget = true;
						break;
					}

					if (Pin->DefaultValue.Contains(TEXT("WBP_LowerThirdBasic")) || Pin->PinFriendlyName.ToString().Contains(TEXT("Class")))
					{
						bTargetsLowerThirdWidget = bTargetsLowerThirdWidget || Pin->DefaultValue.Contains(TEXT("LowerThird"));
					}
				}

				if (!bTargetsLowerThirdWidget)
				{
					continue;
				}

				TArray<UEdGraphNode*> Pending;
				Pending.Add(Node);
				while (Pending.Num() > 0)
				{
					UEdGraphNode* CurrentNode = Pending.Pop(EAllowShrinking::No);
					if (!IsValid(CurrentNode) || NodesToRemove.Contains(CurrentNode))
					{
						continue;
					}

					NodesToRemove.Add(CurrentNode);
					for (UEdGraphPin* CurrentPin : CurrentNode->Pins)
					{
						if (CurrentPin == nullptr)
						{
							continue;
						}

						for (UEdGraphPin* LinkedPin : CurrentPin->LinkedTo)
						{
							if (LinkedPin != nullptr && IsValid(LinkedPin->GetOwningNode()))
							{
								Pending.Add(LinkedPin->GetOwningNode());
							}
						}
					}
				}
			}
		}

		if (NodesToRemove.Num() > 0)
		{
			for (UEdGraphNode* Node : NodesToRemove)
			{
				if (!IsValid(Node))
				{
					continue;
				}

				UE_LOG(LogTemp, Display, TEXT("Removing Level Blueprint node: %s (%s)"), *Node->GetName(), *Node->GetClass()->GetName());
				FBlueprintEditorUtils::RemoveNode(LevelScriptBlueprint, Node, true);
				++RemovedLevelScriptNodes;
			}

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LevelScriptBlueprint);
			FKismetEditorUtilities::CompileBlueprint(LevelScriptBlueprint);
		}
	}

	bool bHasBootstrapper = false;
	for (TActorIterator<ADesignerRuntimePreviewBootstrapper> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			bHasBootstrapper = true;
			break;
		}
	}

	if (!bHasBootstrapper)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ADesignerRuntimePreviewBootstrapper>(
			ADesignerRuntimePreviewBootstrapper::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	}

	World->MarkPackageDirty();
	World->PersistentLevel->MarkPackageDirty();

	const bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(World, MapPath);
	UE_LOG(LogTemp, Display, TEXT("Scanned %d actor(s). Removed %d actor(s) and %d Level Blueprint node(s) from %s. Save=%s"), ActorCount, RemovedCount, RemovedLevelScriptNodes, *MapPath, bSaved ? TEXT("true") : TEXT("false"));
	return bSaved ? 0 : 4;
#endif
}

bool UDesignerCleanupMapTemplatesCommandlet::ShouldRemoveActor(const AActor* Actor) const
{
#if !WITH_EDITOR
	return false;
#else
	if (!IsValid(Actor))
	{
		return false;
	}

	if (Actor->IsA(ADesignerRuntimePreviewBootstrapper::StaticClass()))
	{
		return false;
	}

	if (Actor->IsA(ADesignerTemplateActor::StaticClass()))
	{
		return true;
	}

	const FString ClassPath = Actor->GetClass()->GetPathName();
	const FString ActorLabel = Actor->GetActorLabel();
	const FString ActorName = Actor->GetName();

	if (ClassPath.Contains(TEXT("LowerThird")) || ClassPath.Contains(TEXT("Template")) ||
		ActorLabel.Contains(TEXT("LowerThird")) || ActorLabel.Contains(TEXT("Template")) ||
		ActorName.Contains(TEXT("LowerThird")) || ActorName.Contains(TEXT("Template")))
	{
		return true;
	}

	TInlineComponentArray<UWidgetComponent*> WidgetComponents;
	Actor->GetComponents(WidgetComponents);
	for (const UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!IsValid(WidgetComponent))
		{
			continue;
		}

		const UClass* WidgetClass = WidgetComponent->GetWidgetClass();
		if (!IsValid(WidgetClass))
		{
			continue;
		}

		const FString WidgetClassPath = WidgetClass->GetPathName();
		if (WidgetClassPath.Contains(TEXT("WBP_LowerThirdBasic")) || WidgetClassPath.Contains(TEXT("LowerThird")))
		{
			return true;
		}
	}

	return false;
#endif
}
