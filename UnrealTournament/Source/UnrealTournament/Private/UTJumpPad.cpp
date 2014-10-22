// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTJumpPad.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/NavigationOctree.h"
#include "UTReachSpec_JumpPad.h"

AUTJumpPad::AUTJumpPad(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = PCIP.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComponent"));
	RootComponent = SceneRoot;
	RootComponent->bShouldUpdatePhysicsVolume = true;

	// Setup the mesh
	Mesh = PCIP.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("JumpPadMesh"));
	Mesh->AttachParent = RootComponent;

	TriggerBox = PCIP.CreateDefaultSubobject<UBoxComponent>(this, TEXT("TriggerBox"));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->AttachParent = RootComponent;
	

	JumpSound = nullptr;
	JumpTarget = FVector(100.0f, 0.0f, 0.0f);
	JumpTime = 1.0f;
	bMaintainVelocity = false;

	AreaClass = UNavArea_Default::StaticClass();
#if WITH_EDITORONLY_DATA
	JumpPadComp = PCIP.CreateDefaultSubobject<UUTJumpPadRenderingComponent>(this, TEXT("JumpPadComp"));
	JumpPadComp->PostPhysicsComponentTick.bCanEverTick = false;
	JumpPadComp->AttachParent = RootComponent;
#endif // WITH_EDITORONLY_DATA
}

void AUTJumpPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Launch the pending actors
	if (PendingJumpActors.Num() > 0)
	{
		for (AActor* Actor : PendingJumpActors)
		{
			Launch(Actor);
		}
		PendingJumpActors.Reset();
	}
}

bool AUTJumpPad::CanLaunch_Implementation(AActor* TestActor)
{
	return (Cast<ACharacter>(TestActor) != NULL && TestActor->Role >= ROLE_AutonomousProxy);
}

void AUTJumpPad::Launch_Implementation(AActor* Actor)
{
	// For now just filter for ACharacter. Maybe certain projectiles/vehicles/ragdolls/etc should bounce in the future
	ACharacter* Char = Cast<ACharacter>(Actor);
	if (Char != NULL)
	{
		//Launch the character to the target
		Char->LaunchCharacter(CalculateJumpVelocity(Char), !bMaintainVelocity, true);

		// Play Jump sound if we have one
		UUTGameplayStatics::UTPlaySound(GetWorld(), JumpSound, Char, SRT_AllButOwner, false);

		// if it's a bot, refocus it to its desired endpoint for any air control adjustments
		AUTBot* B = Cast<AUTBot>(Char->Controller);
		if (B != NULL && B->GetMoveTarget().Actor == this)
		{
			bool bFoundNextPath = false;
			for (int32 i = 0; i < B->RouteCache.Num() - 1; i++)
			{
				if (B->RouteCache[i].Actor == this)
				{
					TArray<FComponentBasedPosition> MovePoints;
					new(MovePoints) FComponentBasedPosition(ActorToWorld().TransformPosition(JumpTarget));
					B->SetMoveTarget(B->RouteCache[i + 1], MovePoints);
					B->MoveTimer = FMath::Max<float>(B->MoveTimer, JumpTime);
					bFoundNextPath = true;
					break;
				}
			}
			if (!bFoundNextPath)
			{
				// make sure bot aborts move when it lands
				B->MoveTimer = FMath::Min<float>(B->MoveTimer, JumpTime - 0.1f);
			}
		}
	}
}

void AUTJumpPad::ReceiveActorBeginOverlap(AActor* OtherActor)
{
	Super::ReceiveActorBeginOverlap(OtherActor);

	// Add the actor to be launched if it hasn't already
	if (!PendingJumpActors.Contains(OtherActor) && CanLaunch(OtherActor))
	{
		PendingJumpActors.Add(OtherActor);
	}
}

FVector AUTJumpPad::CalculateJumpVelocity(AActor* JumpActor)
{
	FVector Target = ActorToWorld().TransformPosition(JumpTarget) - JumpActor->GetActorLocation();

	float SizeZ = Target.Z / JumpTime + 0.5f * -GetWorld()->GetGravityZ() * JumpTime;
	float SizeXY = Target.Size2D() / JumpTime;

	FVector Velocity = Target.SafeNormal2D() * SizeXY + FVector(0.0f, 0.0f, SizeZ);

	// Scale the velocity if Character has gravity scaled
	ACharacter* Char = Cast<ACharacter>(JumpActor);
	if (Char != NULL && Char->CharacterMovement != NULL && Char->CharacterMovement->GravityScale != 1.0f)
	{
		Velocity *= FMath::Sqrt(Char->CharacterMovement->GravityScale);
	}
	return Velocity;
}

void AUTJumpPad::AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData)
{
	FVector MyLoc = GetActorLocation();
	NavNodeRef MyPoly = NavData->FindNearestPoly(MyLoc, GetSimpleCollisionCylinderExtent());
	if (MyPoly != INVALID_NAVNODEREF)
	{
		const FCapsuleSize HumanSize = NavData->GetHumanPathSize();
		FVector HumanExtent = FVector(HumanSize.Radius, HumanSize.Radius, HumanSize.Height);
		{
			NavNodeRef TargetPoly = NavData->FindNearestPoly(ActorToWorld().TransformPosition(JumpTarget), HumanExtent);
			UUTPathNode* TargetNode = NavData->GetNodeFromPoly(TargetPoly);
			if (TargetPoly != INVALID_NAVNODEREF && TargetNode != NULL)
			{
				UUTReachSpec_JumpPad* JumpSpec = NewObject<UUTReachSpec_JumpPad>(MyNode);
				JumpSpec->JumpPad = this;
				FUTPathLink* NewLink = new(MyNode->Paths) FUTPathLink(MyNode, MyPoly, TargetNode, TargetPoly, JumpSpec, HumanSize.Radius, HumanSize.Height, R_JUMP);
				for (NavNodeRef SrcPoly : MyNode->Polys)
				{
					NewLink->Distances.Add(NavData->CalcPolyDistance(SrcPoly, MyPoly) + FMath::TruncToInt(1000.0f * JumpTime));
				}
			}
		}

		// if we support air control, look for additional jump targets that could be reached by air controlling against the jump pad's standard velocity
		if (NavData->ScoutClass != NULL && NavData->ScoutClass.GetDefaultObject()->CharacterMovement->AirControl > 0.0f)
		{
			// intentionally place start loc high up to avoid clipping the edges of any irrelevant geometry
			MyLoc.Z += NavData->ScoutClass.GetDefaultObject()->CapsuleComponent->GetUnscaledCapsuleHalfHeight() * 3.0f;

			const float AirControlPct = NavData->ScoutClass.GetDefaultObject()->CharacterMovement->AirControl;
			const FCollisionShape ScoutShape = FCollisionShape::MakeCapsule(NavData->ScoutClass.GetDefaultObject()->CapsuleComponent->GetUnscaledCapsuleRadius(), NavData->ScoutClass.GetDefaultObject()->CapsuleComponent->GetUnscaledCapsuleHalfHeight());
			const FVector JumpVel = CalculateJumpVelocity(this);
			const float XYSpeed = JumpVel.Size2D();
			const float JumpZ = JumpVel.Z;
			const float JumpTargetDist = JumpTarget.Size();
			const float GravityZ = GetWorld()->GetDefaultGravityZ(); // TODO: gravity at jump pad location
			const FVector HeightAdjust(0.0f, 0.0f, NavData->AgentHeight * 0.5f);
			const TArray<const UUTPathNode*>& NodeList = NavData->GetAllNodes();
			for (const UUTPathNode* TargetNode : NodeList)
			{
				if (TargetNode != MyNode)
				{
					for (NavNodeRef TargetPoly : TargetNode->Polys)
					{
						FVector TargetLoc = NavData->GetPolyCenter(TargetPoly) + HeightAdjust;
						const float Dist = (TargetLoc - MyLoc).Size();
						if (Dist < JumpTargetDist && Dist > JumpTargetDist * (1.0f - AirControlPct) && NavData->JumpTraceTest(MyLoc, TargetLoc, MyPoly, TargetPoly, ScoutShape, XYSpeed, GravityZ, JumpZ, JumpZ, NULL, NULL))
						{
							// TODO: account for MaxFallSpeed
							bool bFound = false;
							for (FUTPathLink& ExistingLink : MyNode->Paths)
							{
								if (ExistingLink.End == TargetNode && ExistingLink.StartEdgePoly == TargetPoly)
								{
									ExistingLink.AdditionalEndPolys.Add(TargetPoly);
									bFound = true;
									break;
								}
							}

							if (!bFound)
							{
								UUTReachSpec_JumpPad* JumpSpec = NewObject<UUTReachSpec_JumpPad>(MyNode);
								JumpSpec->JumpPad = this;
								FUTPathLink* NewLink = new(MyNode->Paths) FUTPathLink(MyNode, MyPoly, TargetNode, TargetPoly, JumpSpec, HumanSize.Radius, HumanSize.Height, R_JUMP);
								for (NavNodeRef SrcPoly : MyNode->Polys)
								{
									NewLink->Distances.Add(NavData->CalcPolyDistance(SrcPoly, MyPoly) + FMath::TruncToInt(1000.0f * JumpTime)); // TODO: maybe Z adjust cost if this target is higher/lower and the jump will end slightly faster/slower?
								}
							}
						}
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
void AUTJumpPad::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

}
void AUTJumpPad::CheckForErrors()
{
	Super::CheckForErrors();

	FVector JumpVelocity = CalculateJumpVelocity(this);
	// figure out default game mode from which we will derive the default character
	TSubclassOf<AGameMode> GameClass = GetWorld()->GetWorldSettings()->DefaultGameMode;
	if (GameClass == NULL)
	{
		// horrible config hack around unexported function UGameMapsSettings::GetGlobalDefaultGameMode()
		FString GameClassPath;
		GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), GameClassPath, GEngineIni);
		GameClass = LoadClass<AGameMode>(NULL, *GameClassPath, NULL, 0, NULL);
	}
	const ACharacter* DefaultChar = (GameClass != NULL) ? Cast<ACharacter>(GameClass.GetDefaultObject()->DefaultPawnClass.GetDefaultObject()) : GetDefault<AUTCharacter>();
	if (DefaultChar != NULL && DefaultChar->CharacterMovement != NULL)
	{
		JumpVelocity *= FMath::Sqrt(DefaultChar->CharacterMovement->GravityScale);
	}
	// check if velocity is faster than physics will allow
	APhysicsVolume* PhysVolume = (RootComponent != NULL) ? RootComponent->GetPhysicsVolume() : GetWorld()->GetDefaultPhysicsVolume();
	if (JumpVelocity.Size() > PhysVolume->TerminalVelocity)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		Arguments.Add(TEXT("Speed"), FText::AsNumber(JumpVelocity.Size()));
		Arguments.Add(TEXT("TerminalVelocity"), FText::AsNumber(PhysVolume->TerminalVelocity));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("UTJumpPad", "TerminalVelocityWarning", "{ActorName} : Jump pad speed on default character would be {Speed} but terminal velocity is {TerminalVelocity}!"), Arguments)))
			->AddToken(FMapErrorToken::Create(FName(TEXT("JumpPadTerminalVelocity"))));
	}
}
#endif // WITH_EDITOR
