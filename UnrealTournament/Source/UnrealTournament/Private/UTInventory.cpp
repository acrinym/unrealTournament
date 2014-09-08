// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UnrealNetwork.h"
#include "UTDroppedPickup.h"

AUTInventory::AUTInventory(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	SetReplicates(true);
	bOnlyRelevantToOwner = true;
	bReplicateInstigator = true;

	RespawnTime = 30.0f;

	RootComponent = PCIP.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false, false, false);
	PickupMesh = PCIP.CreateDefaultSubobject<UStaticMeshComponent, UStaticMeshComponent>(this, TEXT("PickupMesh0"), false, false, false);
	if (PickupMesh != NULL)
	{
		PickupMesh->AttachParent = RootComponent;
		PickupMesh->bAutoRegister = false;
	}

	DroppedPickupClass = AUTDroppedPickup::StaticClass();
}

void AUTInventory::PostInitProperties()
{
	Super::PostInitProperties();
	// attempt to set defaults for event early outs based on whether the class has implemented them
	// note that this only works for blueprints, C++ classes need to manually set
	if (Cast<UBlueprintGeneratedClass>(GetClass()) != NULL)
	{
		if (!bCallDamageEvents)
		{
			static FName NAME_ModifyDamageTaken(TEXT("ModifyDamageTaken"));
			static FName NAME_PreventHeadShot(TEXT("PreventHeadShot"));
			UFunction* Func = FindFunction(NAME_ModifyDamageTaken);
			bCallDamageEvents = (Func != NULL && Func->Script.Num() > 0);
			if (!bCallDamageEvents)
			{
				Func = FindFunction(NAME_PreventHeadShot);
				bCallDamageEvents = (Func != NULL && Func->Script.Num() > 0);
			}
		}
		if (!bCallOwnerEvent)
		{
			static FName NAME_OwnerEvent(TEXT("OwnerEvent"));
			UFunction* Func = FindFunction(NAME_OwnerEvent);
			bCallOwnerEvent = (Func != NULL && Func->Script.Num() > 0);
		}
	}
}

void AUTInventory::PreInitializeComponents()
{
	// get rid of components that are only supposed to be part of the pickup mesh
	// TODO: would be better to not create in the first place, no reasonable engine hook to filter
	for (int32 i = 0; i < SerializedComponents.Num(); i++)
	{
		USceneComponent* SceneComp = Cast<USceneComponent>(SerializedComponents[i]);
		if (SceneComp != NULL && SceneComp->AttachParent != NULL && SceneComp->AttachParent == PickupMesh && !SceneComp->AttachParent->IsRegistered())
		{
			TArray<USceneComponent*> Children;
			SceneComp->GetChildrenComponents(true, Children);
			for (USceneComponent* Child : Children)
			{
				Child->DestroyComponent();
			}
			SceneComp->DestroyComponent();
		}
	}

	Super::PreInitializeComponents();
}

UMeshComponent* AUTInventory::GetPickupMeshTemplate_Implementation(FVector& OverrideScale) const
{
	return PickupMesh;
}

void AUTInventory::AddOverlayMaterials_Implementation(AUTGameState* GS) const
{
}

void AUTInventory::Destroyed()
{
	if (UTOwner != NULL)
	{
		UTOwner->RemoveInventory(this);
	}

	Super::Destroyed();
}

void AUTInventory::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Instigator = NewOwner;
	SetOwner(NewOwner);
	UTOwner = NewOwner;
	PrimaryActorTick.AddPrerequisite(UTOwner, UTOwner->PrimaryActorTick);
	eventGivenTo(NewOwner, bAutoActivate);
	ClientGivenTo(bAutoActivate);
}

void AUTInventory::Removed()
{
	eventRemoved();

	if (UTOwner != NULL)
	{
		PrimaryActorTick.RemovePrerequisite(UTOwner, UTOwner->PrimaryActorTick);
	}

	ClientRemoved(); // must be first, since it won't replicate after Owner is lost

	Instigator = NULL;
	SetOwner(NULL);
	UTOwner = NULL;
	NextInventory = NULL;
}

void AUTInventory::CheckPendingClientGivenTo()
{
	if (bPendingClientGivenTo && Instigator != NULL)
	{
		bPendingClientGivenTo = false;
		ClientGivenTo_Implementation(bPendingAutoActivate);
	}
}
void AUTInventory::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	CheckPendingClientGivenTo();
}

void AUTInventory::ClientGivenTo_Implementation(bool bAutoActivate)
{
	if (Instigator == NULL || !Cast<AUTCharacter>(Instigator)->IsInInventory(this))
	{
		bPendingClientGivenTo = true;
		bPendingAutoActivate = bAutoActivate;
		GetWorld()->GetTimerManager().SetTimer(this, &AUTInventory::CheckPendingClientGivenTo, 0.1f, false);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(this, &AUTInventory::CheckPendingClientGivenTo);
		bPendingClientGivenTo = false;
		ClientGivenTo_Internal(bAutoActivate);
		eventClientGivenTo(bAutoActivate);
	}
}

void AUTInventory::ClientGivenTo_Internal(bool bAutoActivate)
{
	checkSlow(Instigator != NULL);
	SetOwner(Instigator);
	UTOwner = Cast<AUTCharacter>(Instigator);
	checkSlow(UTOwner != NULL);
	PrimaryActorTick.AddPrerequisite(UTOwner, UTOwner->PrimaryActorTick);
}

void AUTInventory::ClientRemoved_Implementation()
{
	if (UTOwner != NULL)
	{
		PrimaryActorTick.RemovePrerequisite(UTOwner, UTOwner->PrimaryActorTick);
	}
	eventClientRemoved();
	SetOwner(NULL);
	UTOwner = NULL;
	Instigator = NULL;
	NextInventory = NULL;
}

void AUTInventory::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTInventory, NextInventory, COND_OwnerOnly);
}

void AUTInventory::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	if (Role == ROLE_Authority)
	{
		APawn* FormerInstigator = Instigator;

		if (UTOwner != NULL)
		{
			UTOwner->RemoveInventory(this);
		}
		Instigator = NULL;
		SetOwner(NULL);
		if (DroppedPickupClass != NULL)
		{
			// pull back spawn location if it is embedded in world geometry
			FVector AdjustedStartLoc = StartLocation;
			UCapsuleComponent* TestCapsule = DroppedPickupClass.GetDefaultObject()->Collision;
			if (TestCapsule != NULL)
			{
				FHitResult Hit;
				if (GetWorld()->SweepSingle(Hit, StartLocation - TossVelocity * 0.25f, StartLocation, FQuat::Identity, TestCapsule->GetCollisionObjectType(), TestCapsule->GetCollisionShape(), FCollisionQueryParams(FName(TEXT("DropPlacement")), false), TestCapsule->GetCollisionResponseToChannels()))
				{
					AdjustedStartLoc = Hit.Location;
				}
			}

			FActorSpawnParameters Params;
			Params.Instigator = FormerInstigator;
			AUTDroppedPickup* Pickup = GetWorld()->SpawnActor<AUTDroppedPickup>(DroppedPickupClass, AdjustedStartLoc, TossVelocity.Rotation(), Params);
			if (Pickup != NULL)
			{
				Pickup->Movement->Velocity = TossVelocity;
				Pickup->SetInventory(this);
			}
			else
			{
				Destroy();
			}
		}
		else
		{
			Destroy();
		}
	}
}

bool AUTInventory::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	return false;
}

void AUTInventory::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, AUTInventory*& HitArmor, const FDamageEvent& DamageEvent, AController* InstigatedBy, AActor* DamageCauser)
{
}

bool AUTInventory::PlayArmorEffects_Implementation(AUTCharacter* HitPawn) const
{
	return false;
}

bool AUTInventory::PreventHeadShot_Implementation(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor)
{
	return false;
}

void AUTInventory::OwnerEvent_Implementation(FName EventName)
{
}

void AUTInventory::DrawInventoryHUD_Implementation(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size)
{
}