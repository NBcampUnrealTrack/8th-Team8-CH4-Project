#include "CatchCharacter/Public/FurniturePawn2.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "CatchCharacter/Public/GrabActorComponent.h"

AFurniturePawn2::AFurniturePawn2()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	FurnitureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FurnitureMesh"));
	RootComponent = FurnitureMesh;

	FurnitureStat = CreateDefaultSubobject<UFurnitureStat>(TEXT("FurnitureStat"));

	// 보간을 위한 작업
	//const static float ActorNetUpdateFrequency = 100.f;
	//SetNetUpdateFrequency(ActorNetUpdateFrequency);
	//// 1초에 100번씩 액터 레플리케이션 시도
	//NetUpdatePeriod = 1 / GetNetUpdateFrequency();
	//// 주기 = 1 / 주파수
}

void AFurniturePawn2::BeginPlay()
{
	Super::BeginPlay();

	//메쉬에서 피직스 시뮬레이션 킴
	if (FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	}

	// 서버에서 초기 스텟값을 데이터테이블에서 가져와서 스텟 정보 반영
	if (HasAuthority())
	{
		if (!FurnitureDataRow.IsNull())
		{
			FFurnitureData* Data = FurnitureDataRow.GetRow<FFurnitureData>(TEXT("FurniturePawn_Init"));
			if (Data && FurnitureStat)
			{
				FurnitureStat->InitializeStats(*Data);
			}
		}
	}
}

void AFurniturePawn2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버에서 매틱마다 연산
	if (HasAuthority() && GrabbedPlayers.Num() > 0)
	{
		// 가구가 이동을 하며 캐릭터에게 이동, 회전해야할 값을 보냄
		HandleMovement(DeltaTime);
	}
	// 클라에서 보간하여 부드럽게 움직이도록 함
	else if (!HasAuthority() && GrabbedPlayers.Num() > 0)
	{
		UpdateClientInterpolation(DeltaTime);
	}
	// 아무도 가구를 잡지 않고 방치된 상태일 때 (물리 낙하 등)
	else if (!HasAuthority() && GrabbedPlayers.Num() == 0)
	{
		// 출발점 갱신
		PreviousClientLoc = GetActorLocation();
		PreviousClientRot = GetActorRotation();
	}
}

void AFurniturePawn2::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFurniturePawn2, GrabbedPlayers);
	DOREPLIFETIME(AFurniturePawn2, ServerLocation);
	DOREPLIFETIME(AFurniturePawn2, ServerRotation);
}

void AFurniturePawn2::Grab(ACharacter* Grabber, FVector height,UPrimitiveComponent* GrabberComponent)
{
	// Grab은 서버에서만 작업해야한다.
	if (!HasAuthority() || !Grabber || GrabbedPlayers.Contains(Grabber)) 
		return;

	// 필요 인원까지만 잡을 수 있도록 제한
	if (FurnitureStat && GrabbedPlayers.Num() >= FurnitureStat->GetRequiredPlayer())
	{
		return;
	}

	// 들어올려지는 가구는 물리를 끄고 일정높이 들어올려준다.
	if (GrabbedPlayers.Num() == 0)
	{
		FurnitureMesh->SetSimulatePhysics(false);
		FurnitureMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		AddActorWorldOffset(height);
	}
	// 잡은 사람과 가구끼리 충돌을 제거시킨다.
	Grabber->MoveIgnoreActorAdd(this);
	this->MoveIgnoreActorAdd(Grabber);

	// 잡고있는 플레이어 등록
	GrabbedPlayers.Add(Grabber);

	// 현재 잡은 플레이어의 위치를 기록
	PreviousPlayerLocations.Add(Grabber, Grabber->GetActorLocation());
	PreviousPlayerYaws.Add(Grabber, Grabber->GetActorRotation().Yaw);

	// 플레이어와 가구사이의 거리와 각도 저장
	InitialVectors.Add(Grabber, GetActorLocation() - Grabber->GetActorLocation());
	InitialYaws.Add(Grabber, Grabber->GetActorRotation().Yaw);

	// 가구 스텟에 현재 잡고있는 플레이어 반영
	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}
}

void AFurniturePawn2::Release(ACharacter* Grabber)
{
	// 서버에서만 작동함
	if (!HasAuthority() || !Grabber || !GrabbedPlayers.Contains(Grabber)) 
		return;

	Grabber->MoveIgnoreActorRemove(this);
	this->MoveIgnoreActorRemove(Grabber);

	GrabbedPlayers.Remove(Grabber);
	PreviousPlayerLocations.Remove(Grabber);
	PreviousPlayerYaws.Remove(Grabber);
	InitialVectors.Remove(Grabber);
	InitialYaws.Remove(Grabber);

	if (GrabbedPlayers.Num() == 0)
	{
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	}

	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}
}

void AFurniturePawn2::HandleMovement(float DeltaTime)
{
	// 해당 연산은 서버에서 이루어져야함
	if (!HasAuthority() || !FurnitureStat)
		return;

	// 이동되어야할 총합
	FVector CombinedDeltaLoc = FVector::ZeroVector;
	float CombinedDeltaYaw = 0.0f;
	
	// 각 플레이어가 가구에 요청한 개별 이동량을 저장할 맵
	TMap<ACharacter*, FVector> PlayerDeltaLocs;
	TMap<ACharacter*, float> PlayerDeltaYaws;

	FVector OldFurnitureLoc = GetActorLocation();

	for (ACharacter* Player : GrabbedPlayers)
	{
		if (Player && PreviousPlayerLocations.Contains(Player) && PreviousPlayerYaws.Contains(Player) && InitialVectors.Contains(Player))
		{
			// 현재 가구가 이동되어야 할 값 계산
			float CurrentYaw = Player->GetActorRotation().Yaw;
			float PreviousYaw = PreviousPlayerYaws[Player];
			float PlayerDeltaYaw = FMath::FindDeltaAngleDegrees(PreviousYaw, CurrentYaw);

			// 플레이어가 처음 잡았을 때부터 지금까지 '총' 회전한 각도
			float TotalYawChange = FMath::FindDeltaAngleDegrees(InitialYaws[Player], CurrentYaw);

			// 플레이어 회전에 영향받아 추가이동량 (초기 거리를 총 회전량만큼 돌림)
			FVector CurrentVectorToFurniture = InitialVectors[Player].RotateAngleAxis(TotalYawChange, FVector::UpVector);
			FVector DesiredFurnitureLoc = Player->GetActorLocation() + CurrentVectorToFurniture;

			FVector PlayerDeltaLoc = DesiredFurnitureLoc - OldFurnitureLoc;
			
			// 계산된 플레이어별 요청량을 맵에 저장
			PlayerDeltaLocs.Add(Player, PlayerDeltaLoc);
			PlayerDeltaYaws.Add(Player, PlayerDeltaYaw);

			CombinedDeltaLoc += PlayerDeltaLoc;
			CombinedDeltaYaw += PlayerDeltaYaw;
		}
	}

	int32 NumPlayers = GrabbedPlayers.Num();
	if (NumPlayers > 0)
	{
		// 옳기고잇는 플레이어수에 따른 속도값제어용...데미데이터긴한데 혹시모름
		int32 Divider = NumPlayers;

		FVector AverageDeltaLoc = CombinedDeltaLoc / Divider;
		float AverageDeltaYaw = CombinedDeltaYaw / Divider;

		FRotator OldFurnitureRot = GetActorRotation();

		// 이동해야할 위치
		// 회전은 Yaw만 할거임
		FVector TargetLocation = OldFurnitureLoc + AverageDeltaLoc;
		FRotator TargetRotation = OldFurnitureRot;
		TargetRotation.Yaw += AverageDeltaYaw;
		
		// 가구를 플레이어들의 평균 이동량만큼 이동
		SetActorLocationAndRotation(TargetLocation, TargetRotation, true);

		// 가구가 벽에 부딪혀서 이동하지 못한 경우 등 실제 이동량 체크해보기
		FVector ActualDeltaLoc = GetActorLocation() - OldFurnitureLoc;
		float ActualDeltaYaw = FMath::FindDeltaAngleDegrees(OldFurnitureRot.Yaw, GetActorRotation().Yaw);

		// 플레이어 이동시킴(실제 이동량으로 움직이기)
		for (ACharacter* Player : GrabbedPlayers)
		{
			if (PlayerDeltaLocs.Contains(Player))
			{
				// 내가 밀고 싶었던 만큼 가구가 못 갔다면, 그 차이만큼 강제로 이동시킴
				FVector LocCorrection = ActualDeltaLoc - PlayerDeltaLocs[Player];
				float YawCorrection = ActualDeltaYaw - PlayerDeltaYaws[Player];
				if (!LocCorrection.IsNearlyZero(0.1f))
				{
					Player->AddActorWorldOffset(LocCorrection, true);
				}
				if (FMath::Abs(YawCorrection) > 0.1f)
				{
					Player->AddActorWorldRotation(FRotator(0.0f, YawCorrection, 0.0f));
				}

				// 로컬 클라이언트 무브먼트의 고집을 꺾기 위해 강제 위치/회전 브로드캐스트
				if (!LocCorrection.IsNearlyZero(0.1f) || FMath::Abs(YawCorrection) > 0.1f)
				{
					Multicast_ForcePlayerPositionAndRotation(Player, Player->GetActorLocation(), Player->GetActorRotation().Yaw);
				}
			}
		}
	}

	// 모든 잡고 있는 플레이어의 위치기록
	for (ACharacter* Player : GrabbedPlayers)
	{
		if (Player)
		{
			// 다음 연산에 쓰일예정
			PreviousPlayerLocations.Add(Player, Player->GetActorLocation());
			PreviousPlayerYaws.Add(Player, Player->GetActorRotation().Yaw);
		}
	}

	// 서버에서의 가구 최종 위치를 클라이언트 전송용 변수에 복사
	ServerLocation = GetActorLocation();
	ServerRotation = GetActorRotation();
}



void AFurniturePawn2::OnRep_GrabbedPlayers() 
{
	if (GrabbedPlayers.Num() > 0)
	{
		// 가구를 잡은 상태: 클라이언트에서도 물리를 끄고 충돌 무시 적용
		FurnitureMesh->SetSimulatePhysics(false);
		FurnitureMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

		for (ACharacter* Player : GrabbedPlayers)
		{
			if (Player)
			{
				Player->MoveIgnoreActorAdd(this);
				this->MoveIgnoreActorAdd(Player);
			}
		}
	}
	else
	{
		// 아무도 안 잡은 상태: 물리를 다시 켜고 충돌 원상복구
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));

		// 클라이언트 월드에 존재하는 내 플레이어의 충돌 무시 상태 해제
		if (GetWorld())
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (ACharacter* LocalPlayer = Cast<ACharacter>(PC->GetPawn()))
				{
					LocalPlayer->MoveIgnoreActorRemove(this);
					this->MoveIgnoreActorRemove(LocalPlayer);
				}
			}
		}
	}
}

void AFurniturePawn2::UpdateClientInterpolation(float DeltaTime)
{
	// 보간 속도
	float InterpSpeed = 30.0f; 

	// 타이머 없이, 항상 직전의 부드러운 위치(Previous)에서 서버의 최신 목표를 향해 자석처럼 당겨줌
	FVector EstimatedLoc = FMath::VInterpTo(PreviousClientLoc, ServerLocation, DeltaTime, InterpSpeed);
	FRotator EstimatedRot = FMath::RInterpTo(PreviousClientRot, ServerRotation, DeltaTime, InterpSpeed);

	SetActorLocationAndRotation(EstimatedLoc, EstimatedRot, false);

	// 다음 프레임을 위해 현재 위치 백업
	PreviousClientLoc = EstimatedLoc;
	PreviousClientRot = EstimatedRot;
}

void AFurniturePawn2::Multicast_ForcePlayerPositionAndRotation_Implementation(ACharacter* PlayerToTarget, FVector LocToSet, float YawToSet)
{
	// 이 함수는 모든 클라이언트에게 전송됨.
	// 하지만 정작 "자기 자신"을 조종하는 로컬 클라이언트에서만 강제 업데이트를 적용함.
	if (PlayerToTarget && PlayerToTarget->IsLocallyControlled() && !HasAuthority())
	{
		FRotator TargetRot = PlayerToTarget->GetActorRotation();
		TargetRot.Yaw = YawToSet;
		// 언리얼 무브먼트 컴포넌트의 고집을 뚫고 억지로 위치와 회전을 세팅
		PlayerToTarget->SetActorLocationAndRotation(LocToSet, TargetRot, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
