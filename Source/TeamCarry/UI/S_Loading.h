// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_Loading.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTextBlock;
class UCanvasPanel;
class UCanvasPanelSlot;
class UCommonButtonBase;

// 로딩 게이지의 3단계. 각 단계 전환 시 "현재 표시값"에서 이어붙이므로 어느 시점에 전환돼도
// 게이지가 튀지 않는다(로딩이 빨라 40% 에서 완료돼도 40→99 로 자연스럽게 이어진다).
enum class ELoadingStage : uint8
{
	// 내 맵이 로드 중. 0 → PseudoProgressCap(90%) 시간 기반 연출 — 유일한 "가짜" 구간.
	Traveling,
	// 내 로컬 로딩 완료(진짜 신호: ATCPlayerController::BeginPlay 스테이지 분기).
	// LocalCompleteProgress(99%) 로 올린 뒤 다른 플레이어를 기다린다.
	LocalComplete,
	// 전원 로딩 완료(진짜 신호: 서버 권위 판정). 100% 로 마무리한다.
	AllComplete
};
/**
 * US_Loading - 레벨 트래블 구간 로딩 화면(명세 4장-9, EE_UIState::Loading).
 *
 * UTCSessionFlow::OnTravelStarted 를 구독한 UMockUIController 가 ReplaceState(Loading) 으로
 * 띄운다. 트래블이 끝나면 도착 맵의 GameMode/PlayerController 가 목적지 State 로 다시
 * ReplaceState 하므로, 이 화면은 별도 종료 처리 없이 자연히 교체되어 사라진다.
 *
 * UE 의 맵 로드는 정밀 진행률 콜백을 제공하지 않으므로, 시간 기반 유사 진행(0→90% 보간, 이후
 * 유지)을 사용한다. "진행률 계산"(ComputeTimeBasedProgress)과 "표시 반영"(ApplyLoadingProgress)을
 * 분리해 두어, 실제 스트리밍/AsyncLoad 진행률이 생기면 계산 함수 호출부만 교체하면 된다.
 */
UCLASS()
class TEAMCARRY_API US_Loading : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	// 서버가 전원 로딩 완료를 판정했을 때 ATCPlayerController 가 호출. 현재 표시값 → 100% 마무리
	// 연출을 재생하고, 끝나면 OnFinishAnimationCompleted 를 브로드캐스트한다. 호출자는 이 신호를
	// 받은 뒤에 화면을 내려야 100% 가 실제로 보인다.
	void PlayFinishAnimation();

	// 이 클라이언트의 맵 로딩이 실제로 끝난 시점에 ATCPlayerController::BeginPlay() 가 호출한다.
	// 시간 추측이 아니라 실제 사건이므로, 여기서부터 게이지는 "진짜"가 된다.
	void NotifyLocalLoadComplete();

	DECLARE_MULTICAST_DELEGATE(FOnLoadingFinishAnimCompleted);
	FOnLoadingFinishAnimCompleted OnFinishAnimationCompleted;

	// 게이트가 비정상적으로 지연될 때 서버가 호출. 진행률/연출은 건드리지 않고 안내 문구와
	// 나가기 버튼만 노출한다.
	void NotifyLoadingStalled();

	// 마무리 연출 길이(초). 이 시간만큼 화면 하강이 지연된다 — ATCPlayerController 의 세이프티
	// 타이머 기준값으로도 쓰이므로 public.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float FinishDuration = 0.5f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 로딩 중 게임 입력을 무시한다(명세 4장-9). Menu 모드이므로 Btn_LeaveSession(지연 시에만
	// 노출)은 정상 동작한다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// --- 구성 요소 ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Img_Background;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UProgressBar> PB_Loading;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_LoadingGauge;

	// 전원 로딩 대기 현황("민기님을 기다리는 중... (3/4)"). 진행률 경로와 분리되어 있어,
	// HasLoadedCurrentMap 복제가 없거나 실패해도 바/트럭 연출은 영향받지 않는다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_LoadingStatus;

	// 지연 시에만 노출되는 탈출 경로. 평소 Collapsed.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_LeaveSession;

	// --- 연출 구성 요소(없으면 정적 화면으로 폴백) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCanvasPanel> Panel_Truck;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Img_Wheel_Rear;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Img_Wheel_Front;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Img_Cloud_Far;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Img_Cloud_Near;

	// --- 진행률 상수 ---
	// 지수 곡선의 시간 상수 기준값. 실제 로딩 시간과 일치할 필요 없다(곡선이 점근이라 어긋나도
	// 게이지가 하드 스톱하지 않음) — 순수하게 "게이지가 얼마나 빨리 기어오르나" 취향값.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float PseudoProgressDuration = 4.0f;

	// 시간 기반 연출의 상한. 이 값은 절대 도달하지 않으며, 100% 는 PlayFinishAnimation() 전용이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float PseudoProgressCap = 0.9f;

	// 클수록 초반이 급하고 뒷심이 길어진다. 3.0 이면 PseudoProgressDuration 시점에 Cap 의 약 95%.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float PseudoCurveSharpness = 3.0f;

	// 내 로딩 완료 시 도달하는 값. 100% 는 전원 완료 전용이므로 그 직전까지만 올린다 —
	// 99% 에 앉아 있는 화면이 곧 "내 건 끝났고 남을 기다리는 중"을 뜻한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float LocalCompleteProgress = 0.99f;

	// 90 → 99% 구간 연출 길이(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float LocalCompleteDuration = 0.4f;

	// --- 연출 상수 ---
	// 진행률에 연동하지 않는다. 게이트 대기 중 트럭 X 가 정체돼도 바퀴와 구름이 계속 움직여
	// "정속 주행 중"으로 읽히게 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading|Anim")
	float WheelRotationSpeed = 540.0f;      // deg/sec

	// 진행률 0 / 100% 일 때의 트럭 X(Canvas 로컬). 연출 구간은 Cap(90%)까지이므로 트럭은
	// 전체 구간의 90% 지점까지만 오고, 마무리 연출이 남은 10% 를 채우며 EndX 에 도착한다.
	// EndX 는 화면 끝이 아니라 중앙 부근으로 잡는다 — 대기로 정체될 때 화면 끝에 박힌 그림이
	// 되지 않도록.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading|Anim")
	float TruckStartX = -360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading|Anim")
	float TruckEndX = 520.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading|Anim")
	float BounceAmplitude = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading|Anim")
	float BounceFrequency = 17.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading|Anim")
	float CloudSpeedFar = 22.0f;            // px/sec

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading|Anim")
	float CloudSpeedNear = 58.0f;

private:
	// 진행률 "계산" — 시간 기반 연출(0 → Cap 점근).
	float ComputeTimeBasedProgress(float InElapsedSeconds) const;

	// 전원 로딩 대기 현황 조회. 표시 텍스트 전용이며 진행률 계산에 관여하지 않는다.
	// 대기 구간이 아니거나 PlayerArray 미복제 시 false.
	bool TryGetLoadingGateStatus(int32& OutLoaded, int32& OutTotal, FString& OutPendingName) const;

	// 진행률 "표시 반영" — 계산 방식이 바뀌어도 그대로 재사용된다.
	void ApplyLoadingProgress(float Alpha01);
	void ApplyLoadingStatusText(bool bInGate, int32 Loaded, int32 Total, const FString& PendingName);
	void ApplyTruckAnimation(float Alpha01, float InDeltaTime, const FGeometry& MyGeometry);

	// UUserWidget::GetWorld() 는 생성 시점의 World 를 CachedWorld 에 스냅샷해 둔다. 이 위젯은
	// GameInstance 소유로 트래블을 넘어 살아남으므로, GetWorld() 가 이미 파괴된 출발지(로비)
	// World 를 계속 가리킬 수 있다 — 현재 World 는 GameInstance 를 통해 다시 얻는다.
	const UWorld* ResolveCurrentWorld() const;

	// 지속형 인스턴스와 RootLayout 인스턴스 양쪽 모두에서 안전하게 불릴 수 있는 idempotent 리셋.
	void ResetLoadingState();

	// 슬롯 기준 위치를 첫 Tick 에 1회만 캐시(누적 가산 방지). BP 레이아웃 적용 후에야 슬롯 위치가
	// 확정되므로 생성자/NativeConstruct 에서 캐시하면 안 된다.
	void CacheLayoutBaseline();
	void ScrollCloud(UImage* InCloud, float& InOutOffsetX, float InBaseY,
		float InSpeed, float InViewWidth, float InDeltaTime);
	static UCanvasPanelSlot* GetCanvasSlot(UWidget* InWidget);

	void HandleLeaveSessionClicked();

	// 진행률 계산용 시계. 마무리 연출이 시작되면 정지한다.
	float ElapsedSeconds = 0.0f;

	// 연출용 시계. 진행률과 무관하게 항상 흐른다 — 마무리 연출 중에도 트럭 덜컹거림이
	// 멈추지 않도록 ElapsedSeconds 와 분리한다.
	float AnimTime = 0.0f;

	// 마무리 연출 상태;
	float FinishElapsed = 0.0f;
	
	// 현재 단계로 진입하며 StageStartProgress 에 표시값을 스냅샷한다.
	void EnterStage(ELoadingStage NewStage);

	ELoadingStage Stage = ELoadingStage::Traveling;

	// 실제로 화면에 나가는 값. 바와 트럭이 이 하나를 공유하므로 둘은 어긋날 수 없다.
	float DisplayProgress = 0.0f;

	float StageStartProgress = 0.0f;   // 단계 진입 시점의 표시값(이어붙이기 기준)
	float StageElapsed = 0.0f;
	bool bFinishBroadcast = false;

	bool bStalled = false;

	// 레이아웃 캐시
	float WheelAngle = 0.0f;
	float TruckBaseY = 0.0f;
	float CloudOffsetX_Far = 0.0f;
	float CloudOffsetX_Near = 0.0f;
	float CloudBaseY_Far = 0.0f;
	float CloudBaseY_Near = 0.0f;
	bool bLayoutCached = false;
};