// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerController.generated.h"

UCLASS(dependson=UTCharacter, dependson=UTPlayerState, config=Game)
class AUTPlayerController : public APlayerController
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY()
	AUTCharacter* UTCharacter;

public:

	UFUNCTION(BlueprintCallable, Category = PlayerController)
	virtual AUTCharacter* GetUTCharacter();

	UPROPERTY()
	AUTPlayerState* UTPlayerState;

	UPROPERTY()
	class AUTHUD* MyUTHUD;

	virtual void InitPlayerState();
	virtual void OnRep_PlayerState();
	virtual void SetPawn(APawn* InPawn);
	virtual void SetupInputComponent() OVERRIDE;

	virtual void CheckAutoWeaponSwitch(AUTWeapon* TestWeapon);

	/** check if sound is audible to this player and call ClientHearSound() if so to actually play it
	 * SoundPlayer may be NULL
	 */
	virtual void HearSound(USoundBase* InSoundCue, AActor* SoundPlayer, const FVector& SoundLocation, bool bStopWhenOwnerDestroyed);
	/** plays a heard sound locally
	 * SoundPlayer may be NULL for an unattached sound
	 * if SoundLocation is zero then the sound should be attached to SoundPlayer
	 */
	UFUNCTION(client, unreliable)
	void ClientHearSound(USoundBase* TheSound, AActor* SoundPlayer, FVector SoundLocation, bool bStopWhenOwnerDestroyed, bool bOccluded);

	virtual void SwitchToBestWeapon();

	inline void AddWeaponPickup(class AUTPickupWeapon* NewPickup)
	{
		// insert new pickups at the beginning so the order should be newest->oldest
		// this makes iteration and removal faster when deciding whether the pickup is still hidden in the per-frame code
		RecentWeaponPickups.Insert(NewPickup, 0);
	}

	virtual void UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents);

	virtual void SetName(const FString& S);

	UFUNCTION(exec)
	virtual void ToggleScoreboard(bool bShow);

	UFUNCTION(client, reliable)
	virtual void ClientToggleScoreboard(bool bShow);

	UFUNCTION(client, reliable)
	virtual void ClientSetHUDAndScoreboard(TSubclassOf<class AHUD> NewHUDClass, TSubclassOf<class UUTScoreboard> NewScoreboardClass);

	UFUNCTION(BlueprintCallable, Category = PlayerController)
	virtual APlayerCameraManager* GetPlayerCameraManager();


protected:

	/** list of weapon pickups that my Pawn has recently picked up, so we can hide the weapon mesh per player */
	UPROPERTY()
	TArray<class AUTPickupWeapon*> RecentWeaponPickups;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseTurnRate;
	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseLookUpRate;

	// @TODO FIXMESTEVE make config
	UPROPERTY(EditAnywhere, Category = Dodging)
	float MaxDodgeClickTime;

	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapLeftTime;

	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapRightTime;

	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapForwardTime;

	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapBackTime;

	UPROPERTY(config, BlueprintReadOnly, Category = Weapon)
	bool bAutoWeaponSwitch;


	/** weapon selection */
	void PrevWeapon();
	void NextWeapon();
	virtual void SwitchWeaponInSequence(bool bPrev);
	virtual void SwitchWeapon(int32 Group);

	/** weapon fire input handling -- NOTE: Just forward to the pawn */
	virtual void OnFire();
	virtual void OnStopFire();
	virtual void OnAltFire();
	virtual void OnStopAltFire();
	/** Handles moving forward/backward */
	virtual void MoveForward(float Val);
	/** Handles stafing movement, left and right */
	virtual void MoveRight(float Val);
	/**
	* Called via input to turn at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	virtual void TurnAtRate(float Rate);
	/**
	* Called via input to turn look up/down at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	virtual void LookUpAtRate(float Rate);
	/** called to set the jump flag from input */
	virtual void Jump();

	/** Handler for a touch input beginning. */
	void TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location);

	/** If double tap, tell pawn to dodge */
	bool CheckDodge(float LastTapTime, bool bForward, bool bBack, bool bLeft, bool bRight);

	/** Dodge tap input handling */
	void OnTapLeft();
	void OnTapRight();
	void OnTapForward();
	void OnTapBack();

	virtual void OnShowScores();
	virtual void OnHideScores();
};



