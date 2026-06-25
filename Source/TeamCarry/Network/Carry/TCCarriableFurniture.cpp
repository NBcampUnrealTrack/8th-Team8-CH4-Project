// TCCarriableFurniture.cpp

#include "Network/Carry/TCCarriableFurniture.h"
#include "Network/Carry/TCCarryStatics.h"
#include "Network/Net/TCNetStatics.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ATCCarriableFurniture::ATCCarriableFurniture()
{
	// 서버에서 매 틱 합산 물리를 돌려야 하므로 틱 활성화
	PrimaryActorTick.bCanEverTick = true;

	// 네트워크 복제 + 이동 복제(서버가 옮기면 클라에 반영)
	bReplicates = true;
	SetReplicateMovement(true);

	// 메시를 루트로 구성
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void ATCCarriableFurniture::BeginPlay()
{
	Super::BeginPlay();
}

void ATCCarriableFurniture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATCCarriableFurniture, bIsGrabbed);
	DOREPLIFETIME(ATCCarriableFurniture, GrabbingPlayers);
	DOREPLIFETIME(ATCCarriableFurniture, CombinedVelocity);
}

void ATCCarriableFurniture::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 합산 물리는 서버 권위에서만 (다인 동시 조작 → 서버에서 합산 후 복제)
	if (!UTCNetStatics::HasAuthority(this) || !bIsGrabbed)
	{
		return;
	}

	// 각 운반자의 의도 이동 벡터를 수집 (이동 속도 방향을 의도로 사용)
	TArray<FVector> MoveInputs;
	MoveInputs.Reserve(GrabbingPlayers.Num());
	for (const TObjectPtr<ATCPlayerCharacter>& Carrier : GrabbingPlayers)
	{
		if (Carrier)
		{
			MoveInputs.Add(Carrier->GetVelocity().GetSafeNormal());
		}
	}

	// 공용 유틸로 합산 (중복 구현 금지 — TCCarryStatics 1곳)
	CombinedVelocity = UTCCarryStatics::CombineCarryVelocity(MoveInputs, RequiredCarriers, BaseSpeed);
	ApplyCarryVelocity(CombinedVelocity, DeltaSeconds);
}

bool ATCCarriableFurniture::CanAddCarrier(ATCPlayerCharacter* Carrier) const
{
	// 유효하고 아직 등록되지 않은 운반자만 추가 가능
	return Carrier != nullptr && !GrabbingPlayers.Contains(Carrier);
}

void ATCCarriableFurniture::AddCarrier(ATCPlayerCharacter* Carrier)
{
	// 서버 권위에서만 상태 변경
	if (!UTCNetStatics::HasAuthority(this) || !CanAddCarrier(Carrier))
	{
		return;
	}

	GrabbingPlayers.Add(Carrier);
	bIsGrabbed = GrabbingPlayers.Num() > 0;
	OnRep_IsGrabbed(); // 서버 로컬에서도 즉시 반영(리슨서버)

	UE_LOG(LogTCNet, Log, TEXT("[Furniture] AddCarrier: %s (총 %d명)"),
		*GetNameSafe(Carrier), GrabbingPlayers.Num());
}

void ATCCarriableFurniture::RemoveCarrier(ATCPlayerCharacter* Carrier)
{
	if (!UTCNetStatics::HasAuthority(this))
	{
		return;
	}

	const int32 Removed = GrabbingPlayers.Remove(Carrier);
	if (Removed > 0)
	{
		bIsGrabbed = GrabbingPlayers.Num() > 0;
		if (!bIsGrabbed)
		{
			CombinedVelocity = FVector::ZeroVector;
		}
		OnRep_IsGrabbed();

		UE_LOG(LogTCNet, Log, TEXT("[Furniture] RemoveCarrier: %s (남은 %d명)"),
			*GetNameSafe(Carrier), GrabbingPlayers.Num());
	}
}

void ATCCarriableFurniture::ApplyCarryVelocity(const FVector& InCombinedVelocity, float DeltaSeconds)
{
	if (InCombinedVelocity.IsNearlyZero())
	{
		return;
	}

	// 서버에서 이동 → bReplicateMovement로 클라 동기화
	const FVector NewLocation = GetActorLocation() + InCombinedVelocity * DeltaSeconds;
	SetActorLocation(NewLocation, /*bSweep=*/true);
}

int32 ATCCarriableFurniture::GetRequiredCarriers() const
{
	return RequiredCarriers;
}

bool ATCCarriableFurniture::CanInteract_Implementation(ATCPlayerCharacter* Player)
{
	// 새로 잡을 수 있거나, 이미 잡고 있는(놓기 가능) 플레이어면 상호작용 허용
	return CanAddCarrier(Player) || GrabbingPlayers.Contains(Player);
}

void ATCCarriableFurniture::OnFocus_Implementation()
{
	// 외곽선 하이라이트 등은 BP에서 확장
}

void ATCCarriableFurniture::OnUnfocus_Implementation()
{
	// 외곽선 하이라이트 해제 등은 BP에서 확장
}

void ATCCarriableFurniture::OnInteract_Implementation(ATCPlayerCharacter* Player)
{
	// GrabComponent::ServerTryInteract(서버)에서 호출됨 → 등록/해제 토글
	if (!UTCNetStatics::HasAuthority(this) || Player == nullptr)
	{
		return;
	}

	if (GrabbingPlayers.Contains(Player))
	{
		RemoveCarrier(Player);
	}
	else
	{
		AddCarrier(Player);
	}
}

void ATCCarriableFurniture::OnRep_IsGrabbed()
{
	// 클라에서 잡힘 상태 변화 시 후처리 훅 (현재는 로그만)
	UE_LOG(LogTCNet, Verbose, TEXT("[Furniture] OnRep_IsGrabbed: %s"),
		bIsGrabbed ? TEXT("true") : TEXT("false"));
}
