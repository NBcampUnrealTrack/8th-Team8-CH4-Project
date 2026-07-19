// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/S_Loading.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "CommonButtonBase.h"
#include "Input/CommonUIInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Core/TeamCarryGameState.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCSessionFlow.h"

void US_Loading::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_LeaveSession)
	{
		Btn_LeaveSession->OnClicked().AddUObject(this, &US_Loading::HandleLeaveSessionClicked);
	}

	// 지속형 인스턴스(UMockUIController 가 뷰포트에 직접 추가)는 CommonUI 스택에 등록되지 않아
	// NativeOnActivated 가 호출되지 않을 수 있다(bAutoActivate 기본 false). RootLayout 인스턴스는
	// 반대로 재사용될 수 있어 활성화 시점에도 리셋이 필요하다 — 양쪽에서 부르고 idempotent 하게 둔다.
	ResetLoadingState();
}

void US_Loading::NativeOnActivated()
{
	Super::NativeOnActivated();
	ResetLoadingState();
}

void US_Loading::ResetLoadingState()
{
	ElapsedSeconds = 0.0f;
	AnimTime = 0.0f;
	WheelAngle = 0.0f;
	bLayoutCached = false;

	FinishElapsed = 0.0f;
	Stage = ELoadingStage::Traveling;
	DisplayProgress = 0.0f;
	StageStartProgress = 0.0f;
	StageElapsed = 0.0f;
	bFinishBroadcast = false;

	bStalled = false;
	if (Btn_LeaveSession)
	{
		Btn_LeaveSession->SetVisibility(ESlateVisibility::Collapsed);
	}

	ApplyLoadingProgress(0.0f);
	ApplyLoadingStatusText(false, 0, 0, FString());
}

void US_Loading::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 연출 시계는 진행률과 무관하게 항상 흐른다(트럭 덜컹거림이 단계 전환에 얼어붙지 않도록).
	AnimTime += InDeltaTime;

	switch (Stage)
	{
	case ELoadingStage::Traveling:
		ElapsedSeconds += InDeltaTime;
		DisplayProgress = ComputeTimeBasedProgress(ElapsedSeconds);
		break;

	case ELoadingStage::LocalComplete:
	{
		StageElapsed += InDeltaTime;
		const float T = (LocalCompleteDuration > 0.0f)
			? FMath::Clamp(StageElapsed / LocalCompleteDuration, 0.0f, 1.0f)
			: 1.0f;
		DisplayProgress = FMath::Lerp(StageStartProgress, LocalCompleteProgress,
			FMath::InterpEaseOut(0.0f, 1.0f, T, 2.0f));
	}
	break;

	case ELoadingStage::AllComplete:
	{
		StageElapsed += InDeltaTime;
		const float T = (FinishDuration > 0.0f)
			? FMath::Clamp(StageElapsed / FinishDuration, 0.0f, 1.0f)
			: 1.0f;
		DisplayProgress = FMath::Lerp(StageStartProgress, 1.0f,
			FMath::InterpEaseOut(0.0f, 1.0f, T, 2.0f));

		if (T >= 1.0f && !bFinishBroadcast)
		{
			bFinishBroadcast = true;
			OnFinishAnimationCompleted.Broadcast();   // 이제 화면을 내려도 100% 가 보인 뒤다.
		}
	}
	break;
	}

	int32 Loaded = 0, Total = 0;
	FString PendingName;
	const bool bInGate = TryGetLoadingGateStatus(Loaded, Total, PendingName);

	ApplyLoadingProgress(DisplayProgress);
	ApplyLoadingStatusText(bInGate, Loaded, Total, PendingName);
	ApplyTruckAnimation(DisplayProgress, InDeltaTime, MyGeometry);
}

TOptional<FUIInputConfig> US_Loading::GetDesiredInputConfig() const
{
	// 명세 4장-9: 로딩 중 게임 입력 무시. Menu 모드라 Btn_LeaveSession 은 정상 동작한다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void US_Loading::EnterStage(ELoadingStage NewStage)
{
	Stage = NewStage;
	StageStartProgress = DisplayProgress;   // 지금 보이는 값에서 이어붙인다
	StageElapsed = 0.0f;
}

void US_Loading::NotifyLocalLoadComplete()
{
	// 전원 완료 신호가 먼저 도착했다면(내가 마지막 로더였던 경우) 되돌리지 않는다.
	if (Stage != ELoadingStage::Traveling)
	{
		return;
	}
	EnterStage(ELoadingStage::LocalComplete);
}

void US_Loading::PlayFinishAnimation()
{
	// 재진입 방지 — 서버가 RPC 를 중복 발사할 수 있다.
	if (Stage == ELoadingStage::AllComplete)
	{
		return;
	}
	bFinishBroadcast = false;
	EnterStage(ELoadingStage::AllComplete);
}

void US_Loading::NotifyLoadingStalled()
{
	bStalled = true;
	if (Btn_LeaveSession)
	{
		Btn_LeaveSession->SetVisibility(ESlateVisibility::Visible);
	}
}

void US_Loading::HandleLeaveSessionClicked()
{
	UGameInstance* GI = GetTypedOuter<UGameInstance>();
	if (!GI)
	{
		GI = GetGameInstance();   // RootLayout 인스턴스(PC 소유) 폴백
	}

	if (UTCSessionFlow* Flow = GI ? GI->GetSubsystem<UTCSessionFlow>() : nullptr)
	{
		// 세션 파기 + 타이틀 맵 복귀. 호스트/클라 공통 경로.
		Flow->LeaveToTitle();
	}
}

const UWorld* US_Loading::ResolveCurrentWorld() const
{
	if (const UGameInstance* GI = GetTypedOuter<UGameInstance>())
	{
		return GI->GetWorld();
	}
	return GetWorld();
}

float US_Loading::ComputeTimeBasedProgress(float InElapsedSeconds) const
{
	if (PseudoProgressDuration <= 0.0f || PseudoCurveSharpness <= 0.0f)
	{
		return PseudoProgressCap;
	}

	// 지수 점근: Cap 에 무한히 근접할 뿐 도달하지 않는다. 선형과 달리 "정해진 시간에 딱 멈춤"이
	// 없으므로, 실제 로딩이 예상보다 길어져도 게이지가 계속 미세하게 움직여 정지처럼 보이지 않는다.
	const float Tau = PseudoProgressDuration / PseudoCurveSharpness;
	return PseudoProgressCap * (1.0f - FMath::Exp(-InElapsedSeconds / Tau));
}

bool US_Loading::TryGetLoadingGateStatus(int32& OutLoaded, int32& OutTotal, FString& OutPendingName) const
{
	const UWorld* World = ResolveCurrentWorld();
	const ATeamCarryGameState* GS = World ? World->GetGameState<ATeamCarryGameState>() : nullptr;

	// 트래블 중이거나 GameState 미복제면 대기 구간이 아니다.
	if (!GS || GS->CurrentPhase != EGamePhase::WaitingToStart)
	{
		return false;
	}

	// 내 로딩 완료 보고(ServerReportMapLoaded)가 서버를 거쳐 다시 복제돼 오기까지 왕복 지연이
	// 있다. 그 사이 내가 "미완료"로 잡히면 내 이름이 "○○님을 기다리는 중"에 뜬다 — 로컬에서
	// 이미 내 로딩 완료를 알고 있으므로(Stage) 그 값을 우선한다.
	const APlayerController* LocalPC = World->GetFirstPlayerController();
	const APlayerState* LocalPS = LocalPC ? LocalPC->PlayerState : nullptr;
	const bool bSelfKnownLoaded = (Stage != ELoadingStage::Traveling);

	int32 Loaded = 0, Total = 0;
	FString FirstPending;

	for (const APlayerState* PS : GS->PlayerArray)
	{
		const ATCPlayerState* TCPS = Cast<ATCPlayerState>(PS);
		if (!TCPS)
		{
			continue;
		}
		++Total;

		const bool bLoaded = TCPS->HasLoadedCurrentMap()
			|| (TCPS == LocalPS && bSelfKnownLoaded);

		if (bLoaded)
		{
			++Loaded;
		}
		else if (FirstPending.IsEmpty())
		{
			FirstPending = TCPS->GetPlayerName();
		}
	}

	if (Total <= 0)
	{
		return false;   // PlayerArray 복제 전
	}

	OutLoaded = Loaded;
	OutTotal = Total;
	OutPendingName = FirstPending;
	return true;
}

void US_Loading::ApplyLoadingProgress(float Alpha01)
{
	if (PB_Loading)
	{
		PB_Loading->SetPercent(Alpha01);
	}
	if (Txt_LoadingGauge)
	{
		Txt_LoadingGauge->SetText(FText::FromString(
			FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Alpha01 * 100.0f))));
	}
}

void US_Loading::ApplyLoadingStatusText(bool bInGate, int32 Loaded, int32 Total, const FString& PendingName)
{
	if (!Txt_LoadingStatus)
	{
		return;
	}

	FString Status;

	if (bStalled)
	{
		Status = FString::Printf(TEXT("접속이 지연되고 있습니다... (%d/%d)"), Loaded, Total);
	}
	else if (Stage == ELoadingStage::AllComplete)
	{
		Status = TEXT("출발합니다!");
	}
	else if (Stage == ELoadingStage::LocalComplete)
	{
		// 내 로딩은 끝났고 남을 기다리는 중 — 이 구간에서만 인원/이름을 보여준다.
		// 이름이 있으면 유저가 외부(디스코드 등)로 직접 확인할 수 있게 되어 대기 체감이 크게 달라진다.
		if (!bInGate)
		{
			Status = TEXT("다른 플레이어를 기다리는 중...");   // PlayerArray 복제 전
		}
		else
		{
			Status = PendingName.IsEmpty()
				? FString::Printf(TEXT("다른 플레이어를 기다리는 중... (%d/%d)"), Loaded, Total)
				: FString::Printf(TEXT("%s님을 기다리는 중... (%d/%d)"), *PendingName, Loaded, Total);
		}
	}
	else
	{
		Status = TEXT("맵을 불러오는 중...");
	}

	Txt_LoadingStatus->SetText(FText::FromString(Status));
}
UCanvasPanelSlot* US_Loading::GetCanvasSlot(UWidget* InWidget)
{
	return InWidget ? Cast<UCanvasPanelSlot>(InWidget->Slot) : nullptr;
}

void US_Loading::CacheLayoutBaseline()
{
	if (bLayoutCached)
	{
		return;
	}

	if (UCanvasPanelSlot* S = GetCanvasSlot(Panel_Truck))
	{
		TruckBaseY = S->GetPosition().Y;
	}
	if (UCanvasPanelSlot* S = GetCanvasSlot(Img_Cloud_Far))
	{
		CloudBaseY_Far = S->GetPosition().Y;
		CloudOffsetX_Far = S->GetPosition().X;
	}
	if (UCanvasPanelSlot* S = GetCanvasSlot(Img_Cloud_Near))
	{
		CloudBaseY_Near = S->GetPosition().Y;
		CloudOffsetX_Near = S->GetPosition().X;
	}

	// 바퀴는 중심 회전. BP 에서 Pivot 설정을 잊어도 되도록 여기서 강제한다.
	if (Img_Wheel_Rear) { Img_Wheel_Rear->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); }
	if (Img_Wheel_Front) { Img_Wheel_Front->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); }

	bLayoutCached = true;
}

void US_Loading::ApplyTruckAnimation(float Alpha01, float InDeltaTime, const FGeometry& MyGeometry)
{
	CacheLayoutBaseline();

	// 바퀴 — 등속. 이동에 비례시키면 게이트 대기 중 멈춰 "죽은 화면"이 된다.
	WheelAngle = FMath::Fmod(WheelAngle + WheelRotationSpeed * InDeltaTime, 360.0f);
	if (Img_Wheel_Rear) { Img_Wheel_Rear->SetRenderTransformAngle(WheelAngle); }
	if (Img_Wheel_Front) { Img_Wheel_Front->SetRenderTransformAngle(WheelAngle); }

	// 트럭 — X 는 게이지와 완전히 같은 값을 쓴다(둘이 어긋날 수 없음). Y 는 캐시된 기준값 + 진동
	// (슬롯의 현재 Y 를 읽어 더하면 매 프레임 누적되어 화면 밖으로 날아간다).
	if (UCanvasPanelSlot* TruckSlot = GetCanvasSlot(Panel_Truck))
	{
		const float PosX = FMath::Lerp(TruckStartX, TruckEndX, FMath::Clamp(Alpha01, 0.0f, 1.0f));
		const float BounceY = FMath::Sin(AnimTime * BounceFrequency) * BounceAmplitude;
		TruckSlot->SetPosition(FVector2D(PosX, TruckBaseY + BounceY));
	}

	// 구름 — 진행률과 무관하게 항상 스크롤. 트럭 X 가 정체돼도 이게 "주행 중"을 유지시킨다.
	const float ViewWidth = MyGeometry.GetLocalSize().X;
	ScrollCloud(Img_Cloud_Far, CloudOffsetX_Far, CloudBaseY_Far, CloudSpeedFar, ViewWidth, InDeltaTime);
	ScrollCloud(Img_Cloud_Near, CloudOffsetX_Near, CloudBaseY_Near, CloudSpeedNear, ViewWidth, InDeltaTime);
}

void US_Loading::ScrollCloud(UImage* InCloud, float& InOutOffsetX, float InBaseY,
	float InSpeed, float InViewWidth, float InDeltaTime)
{
	UCanvasPanelSlot* CloudSlot = GetCanvasSlot(InCloud);
	if (!CloudSlot)
	{
		return;
	}

	InOutOffsetX -= InSpeed * InDeltaTime;
	if (InOutOffsetX < -CloudSlot->GetSize().X)
	{
		InOutOffsetX = InViewWidth;   // 오른쪽 밖에서 재등장
	}
	CloudSlot->SetPosition(FVector2D(InOutOffsetX, InBaseY));
}