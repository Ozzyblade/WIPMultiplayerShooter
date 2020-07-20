// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class ASWeapon;
class UCameraShake;
class USHealthComponent;
class UPostProcessComponent;

enum class EViewportEnum : uint8
{
	VE_Left UMETA(DisplayName="Left"),
	VE_Right UMETA(DisplayName="Right")
};

UCLASS()
class COOPSHOOTER_API ASCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Move the player forward */
	void MoveForward(float Val);

	/** Move the player right */
	void MoveRight(float Val);

	/** Called when starting to crouch */
	void BeginCrouch();

	/** Called when ending crouch */
	void EndCrouch();

	/** Called when switching the viewport */
	void SwitchViewport();

	/** Start aiming down sight */
	void BeginADS();

	/** End aiming down sight */
	void EndADS();

	UPROPERTY(Replicated)
	ASWeapon* CurrentWeapon;

	UPROPERTY(Replicated)
	ASWeapon* HolsteredWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Player")
	TSubclassOf<ASWeapon> StarterWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Player")
	TSubclassOf<ASWeapon> HolsteredWeaponClass;

	UPROPERTY(VisibleDefaultsOnly, Category = "Player")
	FName WeaponAttachSocketName;

	UPROPERTY(VisibleDefaultsOnly, Category = "Player")
	FName RifleHolsterName;

	void StartFire();

	void EndFire();

	/** Called when the player takes damage? */
	UFUNCTION()
	void OnHealthChanged(USHealthComponent* HealthComponent, float Health, float HealthDelta, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	/** The camera atttached to the player */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly , Category = "Components")
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* SpringArmComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPostProcessComponent* PostProcessComponent;
	FPostProcessSettings PostFXDamage;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USHealthComponent* HealthComponentProtected;

	/** The offset of the camera, used when switching between left and right */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float CameraViewportOffset;

	/** The FOV when zoomed in */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float ADSFOV;

	/** Default FOV set during begin play */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float DefaultFOV;

	/** How fast will the camera transition into ADS */
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = 0.1, ClampMax = 100))
	float ADSInterpSpeed;

	/** The camera sway that will be added when in horizontal movement */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSubclassOf<UCameraShake> HorMovementCameraShake;

	/** The camera sway that will be added when in vertical movement */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSubclassOf<UCameraShake> VerMovementCameraShake;

	/** The camera sway that will be added when idle */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSubclassOf<UCameraShake> IdleCamSway;

	/** Turns the player charcter into a ragdoll*/
	void BeginRagdoll();
	void ActivateRagdoll();
	/** After death detatch  the weapon from the players hands */
	void DetatchWeapon();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual FVector GetPawnViewLocation() const override;

private:

	//UPROPERTY(EditAnywhere, Category = "ViewPort")
	EViewportEnum ViewPort;

	bool bADS;

	/** Handles updating of the ADS mechanic */
	void ADSCheck(float DeltaTime);

	void UpdateCameraSway();
	void SpawnWeapons();

	/** Used when testing some code */
	void TestFunction();
	void TakeDamageSimple(float Damage);
	void Kill();
	void Heal();

	/** Handle the ragdoll on the cliet and server */
	UFUNCTION(NetMulticast, Reliable, WithValidation)
	void NetMulticastBeginRagdoll();
	virtual bool NetMulticastBeginRagdoll_Validate();
	virtual void NetMulticastBeginRagdoll_Implementation();

private:

	UPROPERTY(Replicated)
	bool bIsCharacterRagdoll;

	bool bIsCheckingFall;
	bool bIsDead;

	FTimerHandle TimerHandle_WeaponDetatchTimer;
	FTimerHandle TimerHandle_FallChecker;
	FTimerHandle TimerHandle_HealthCharacter;
};
