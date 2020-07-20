// Fill out your copyright notice in the Description page of Project Settings.


#include "SCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "SWeapon.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Components/CapsuleComponent.h"
#include "CoopShooter.h"
#include "SHealthComponent.h"
#include "Gameframework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "SWeaponPickup.h"
#include "Components/PostProcessComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetMathLibrary.h" // For look at rotation

// Sets default values
ASCharacter::ASCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create the default spring arm comp
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->bUsePawnControlRotation = true; // Rotate based on pawn
	SpringArmComponent->SetupAttachment(RootComponent);
	SpringArmComponent->TargetArmLength = 200.f;

	GetMovementComponent()->GetNavAgentPropertiesRef().bCanCrouch = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);

	// Create the camera component
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);

	// Create the post fx component
	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostFXComponent"));
	PostFXDamage.bOverride_ColorSaturation = true;

	// Create the health component
	HealthComponentProtected = CreateDefaultSubobject<USHealthComponent>(TEXT("HealthComponent"));

	// Setup the viewport
	ViewPort = EViewportEnum::VE_Right;

	// defaults 
	ADSFOV = 65.0f;
	ADSInterpSpeed = 20.0f;
	WeaponAttachSocketName = "weapon_socket";
	RifleHolsterName = "weapon_rifle_holster";
	bIsCharacterRagdoll = false;
	bIsCheckingFall = false;
	bIsDead = false;


	NetUpdateFrequency = 66.0f;
	MinNetUpdateFrequency = 33.0f;
}

// Called when the game starts or when spawned
void ASCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	DefaultFOV = CameraComponent->FieldOfView;
	HealthComponentProtected->OnHealthChanged.AddDynamic(this, &ASCharacter::OnHealthChanged);

	if (Role == ROLE_Authority)
	{
		SpawnWeapons();
	}
}

/** Move the player forward */
void ASCharacter::MoveForward(float Val)
{
	AddMovementInput(GetActorForwardVector() * Val);

	APlayerController* PC = Cast<APlayerController>(GetController());

	if (PC && HorMovementCameraShake)
	{
		if (GetVelocity().Y > 0.1 || GetVelocity().Y < -0.1)
			PC->ClientPlayCameraShake(HorMovementCameraShake);
	}
}

/** Move the player right */
void ASCharacter::MoveRight(float Val)
{
	AddMovementInput(GetActorRightVector() * Val);

	APlayerController* PC = Cast<APlayerController>(GetController());

	if (PC && VerMovementCameraShake)
	{
		if (GetVelocity().X > 0.1 || GetVelocity().X < -0.1)
			PC->ClientPlayCameraShake(VerMovementCameraShake);
	}
}

void ASCharacter::BeginCrouch()
{
	Crouch();
}

void ASCharacter::EndCrouch()
{
	UnCrouch();
}

void ASCharacter::BeginADS()
{
	bADS = true;
}

void ASCharacter::EndADS()
{
	bADS = false;
}

void ASCharacter::StartFire()
{
	if (CurrentWeapon)
		CurrentWeapon->BeginFire();
}

void ASCharacter::EndFire()
{
	if (CurrentWeapon)
		CurrentWeapon->EndFire();
}

void ASCharacter::OnHealthChanged(USHealthComponent* HealthComponent, float Health, float HealthDelta, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
	if (Role == ROLE_Authority)
	{
		/* TODO: Fix bug where damage to other entities calls this to fire */
		if (Health <= 30 && Health > 20)
		{
			this->PostFXDamage.ColorSaturation.Set(1.0f, 1.0f, 1.0f, 0.6f);
			this->PostProcessComponent->Settings = PostFXDamage;
		}
		else if (Health <= 20 && Health > 10)
		{
			this->PostFXDamage.ColorSaturation.Set(1.0f, 1.0f, 1.0f, 0.3f);
			this->PostProcessComponent->Settings = PostFXDamage;
		}
		else if (Health <= 10)
		{
			this->PostFXDamage.ColorSaturation.Set(1.0f, 1.0f, 1.0f, 0.1f);
			this->PostProcessComponent->Settings = PostFXDamage;
		}
	}

	if (Health <= 0.0f)
	{
		Health = 0.0f;

		GetMovementComponent()->StopMovementImmediately();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		ActivateRagdoll();
		bIsDead = true;

		GetWorldTimerManager().SetTimer(TimerHandle_WeaponDetatchTimer, this, &ASCharacter::DetatchWeapon, 2, false);
		return;
	}
	else {
		if (Health == 100)
		{
			GetWorldTimerManager().ClearTimer(TimerHandle_HealthCharacter);
			return;
		}

		GetWorldTimerManager().SetTimer(TimerHandle_FallChecker, this, &ASCharacter::Heal, .5, true, 5);
		UE_LOG(LogTemp, Warning, TEXT("%f"), Health);
	}
}

void ASCharacter::SwitchViewport()
{
	if (ViewPort == EViewportEnum::VE_Right)
	{
		ViewPort = EViewportEnum::VE_Left;
		SpringArmComponent->AddLocalOffset(FVector(0, -CameraViewportOffset*2, 0));
	}
	else 
	{
		ViewPort = EViewportEnum::VE_Right;
		SpringArmComponent->AddLocalOffset(FVector(0, CameraViewportOffset*2, 0));
	}
}

// Called every frame
void ASCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ADSCheck(DeltaTime);
	UpdateCameraSway();

	if (!bIsCheckingFall && GetMovementComponent()->IsFalling())
	{
		GetWorldTimerManager().SetTimer(TimerHandle_FallChecker, this, &ASCharacter::Kill, 2, false);
		bIsCheckingFall = true;
	}
	else if (bIsCheckingFall && !GetMovementComponent()->IsFalling())
	{
		bIsCheckingFall = false;
		GetWorldTimerManager().ClearTimer(TimerHandle_FallChecker);
	}

	if (bIsDead)
	{
		// Get the camera location
		FVector CameraLocation = CameraComponent->GetComponentLocation();
		FVector MeshLocation = GetMesh()->GetComponentLocation();	
		FRotator NewCameraRotation = UKismetMathLibrary::FindLookAtRotation(CameraLocation, MeshLocation);

		float NewRotX = FMath::FInterpTo(CameraComponent->GetComponentRotation().Pitch, NewCameraRotation.Pitch, DeltaTime, 2);
		float NewRotY = FMath::FInterpTo(CameraComponent->GetComponentRotation().Yaw, NewCameraRotation.Yaw, DeltaTime, 2);
		float NewRotZ = FMath::FInterpTo(CameraComponent->GetComponentRotation().Roll, NewCameraRotation.Roll, DeltaTime, 2);

		CameraComponent->SetWorldRotation(FRotator(NewRotX,NewRotY,NewRotZ));
	}
}

// Called to bind functionality to input
void ASCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind the left & right movement 
	PlayerInputComponent->BindAxis("MoveForward", this, &ASCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASCharacter::MoveRight);

	// Use a built in function to get the mouse input
	PlayerInputComponent->BindAxis("LookUp", this, &ASCharacter::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Turn", this, &ASCharacter::AddControllerYawInput);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ASCharacter::BeginCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ASCharacter::EndCrouch);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

	// Bind the action for switching the character viewport
	PlayerInputComponent->BindAction("CameraView", IE_Pressed, this, &ASCharacter::SwitchViewport);

	// Bind action for ADS
	PlayerInputComponent->BindAction("ADS", IE_Pressed, this, &ASCharacter::BeginADS);
	PlayerInputComponent->BindAction("ADS", IE_Released, this, &ASCharacter::EndADS);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ASCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ASCharacter::EndFire);

	// Testing key
	PlayerInputComponent->BindAction("TestKey", IE_Pressed, this, &ASCharacter::TestFunction);

}

FVector ASCharacter::GetPawnViewLocation() const
{
	if (CameraComponent)
		return CameraComponent->GetComponentLocation();

	return Super::GetPawnViewLocation();
}

void ASCharacter::ADSCheck(float DeltaTime)
{
	float TargetFOV = bADS ? ADSFOV : DefaultFOV;
	float NewFOV = FMath::FInterpTo(CameraComponent->FieldOfView, TargetFOV, DeltaTime, ADSInterpSpeed);

	CameraComponent->SetFieldOfView(NewFOV);
}

void ASCharacter::UpdateCameraSway()
{
	APlayerController* PC = Cast<APlayerController>(GetController());

	if (PC && IdleCamSway)
	{
		if (GetVelocity() == FVector::ZeroVector)
			PC->ClientPlayCameraShake(IdleCamSway);
	}
}

void ASCharacter::SpawnWeapons()
{
	// Spawn a default weapon
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CurrentWeapon = GetWorld()->SpawnActor<ASWeapon>(StarterWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (CurrentWeapon)
	{
		CurrentWeapon->SetOwner(this);
		CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketName);
	}

	// Spawn holstered weapon
	FActorSpawnParameters SpawnParamsHolster;
	SpawnParamsHolster.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	HolsteredWeapon = GetWorld()->SpawnActor<ASWeapon>(HolsteredWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParamsHolster);

	if (HolsteredWeapon)
	{
		HolsteredWeapon->SetOwner(this);
		HolsteredWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, RifleHolsterName);
	}
}

void ASCharacter::ActivateRagdoll()
{
	if (Role == ROLE_Authority)
	{
		DetachFromControllerPendingDestroy();

		UCharacterMovementComponent* CharacterComponent = Cast<UCharacterMovementComponent>(GetMovementComponent());
		if (CharacterComponent)
		{
			CharacterComponent->StopMovementImmediately();
			CharacterComponent->DisableMovement();
			CharacterComponent->SetComponentTickEnabled(false);
		}

		GetMesh()->SetAllBodiesSimulatePhysics(true);
		GetMesh()->WakeAllRigidBodies();
		GetMesh()->bBlendPhysics = true;
		NetMulticastBeginRagdoll();
	}
}

void ASCharacter::BeginRagdoll()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}


bool ASCharacter::NetMulticastBeginRagdoll_Validate()
{
	return true; 
}

void ASCharacter::NetMulticastBeginRagdoll_Implementation() 
{ 
	BeginRagdoll(); 
}

void ASCharacter::DetatchWeapon()
{
	// Detatch the current weapon
	if (CurrentWeapon && CurrentWeapon->DroppedWeapon)
	{
		FActorSpawnParameters SpawnParamsHolster;
		SpawnParamsHolster.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASWeaponPickup* temp = GetWorld()->SpawnActor<ASWeaponPickup>(CurrentWeapon->DroppedWeapon, CurrentWeapon->GetActorLocation(), CurrentWeapon->GetActorRotation(), SpawnParamsHolster);

		if (temp)
		{
			GetWorld()->GetTimerManager().ClearTimer(TimerHandle_WeaponDetatchTimer);
			CurrentWeapon->Destroy();
		}
	}
}

void ASCharacter::Heal()
{

	TakeDamageSimple(-10.0f);

}

void ASCharacter::TestFunction()
{
	TakeDamageSimple(10.0f);
}

/** Used to take damage with a single parameter */
void ASCharacter::TakeDamageSimple(float Damage)
{
	FDamageEvent DamageEvent;
	TakeDamage(Damage, DamageEvent, this->GetController(), this);
}

/** Used to instantly kill the player */
void ASCharacter::Kill()
{
	TakeDamageSimple(100.0f);
}

void ASCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASCharacter, CurrentWeapon);
	DOREPLIFETIME(ASCharacter, HolsteredWeapon);
}

//////////////////////////OLD CODE

	//if (Role < ROLE_Authority)
	//{
	//	ServerRagdoll();
	//}

	///* Turn the player into a ragdoll*/
	//DetachFromControllerPendingDestroy();

	///* Disable all the collsion on the capusle */
	//UCapsuleComponent* CapsuleComponent = GetCapsuleComponent();
	//CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

	//GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	//SetActorEnableCollision(true);

	//if (!bIsCharacterRagdoll)
	//{
	//	/* Ragdoll */
	//	GetMesh()->SetAllBodiesSimulatePhysics(true);
	//	GetMesh()->SetSimulatePhysics(true);
	//	GetMesh()->WakeAllRigidBodies();
	//	GetMesh()->bBlendPhysics = true;

	//	UCharacterMovementComponent* CharacterComponent = Cast<UCharacterMovementComponent>(GetMovementComponent());
	//	if (CharacterComponent)
	//	{
	//		CharacterComponent->StopMovementImmediately();
	//		CharacterComponent->DisableMovement();
	//		CharacterComponent->SetComponentTickEnabled(false);
	//	}

	//	// SetLifeSpan(10.0f);
	//	bIsCharacterRagdoll = true;
	//}