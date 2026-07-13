#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TC_DataTypes.h"
#include "TC_Interfaces.h"
#include "TeamCarryGameState.h"
#include "TCSaveGame.h"
#include "StageClearProvider.h"
#include "StageHost.h"
#include "TeamCarryGameMode.generated.h"

// 트럭 안 가구 정보 구조체
struct FTruckFurnitureInfo
{
	FName RowName;
	float CurrentHealth;
	float MaxHealth;
	int32 BaseScore;
};

UCLASS()
class TEAMCARRY_API ATeamCarryGameMode : public AGameModeBase, public IStageClearProvider, public IStageHost
{
	GENERATED_BODY()

public:
	ATeamCarryGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	// IStageClearProvider
	virtual bool IsStageCleared() const override;
	virtual int32 GetDeliveredCount() const override;
	virtual int32 GetTargetCount() const override;

	// IStageHost
	virtual void OnStageCleared_Implementation() override;

	// 가구 트럭 진입 시 호출
	UFUNCTION(BlueprintCallable)
	void OnFurnitureEnterTruck(FName RowName, float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 가구 트럭 이탈 시 호출
	UFUNCTION(BlueprintCallable)
	void OnFurnitureExitTruck(FName RowName);
	
	// 가구 파괴 시 호출
	UFUNCTION(BlueprintCallable)
	void OnFurnitureDestroyed();

	// 스테이지 시작 시 옮겨야 할 가구 개수 설정
	UFUNCTION(BlueprintCallable)
	void SetTotalFurnitureCount(int32 Count);
	
	// 플레이어 로그아웃 시 호출
	virtual void Logout(AController* Exiting) override;
	
	// 플레이어 재접속 시 호출
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	// 게임 저장
	UFUNCTION(BlueprintCallable)
	void SaveGame(const FString& StageName);

	// 게임 불러오기
	UFUNCTION(BlueprintCallable)
	UTCSaveGame* LoadGame();
	
	// 게임 단계 전환
	UFUNCTION(BlueprintCallable)
	void SetGamePhase(EGamePhase NewPhase);

	// 카운트다운 시작
	void StartCountdown();

	// 별 3개 기준 남은 시간 (블루프린트에서 스테이지마다 수정 가능)
	// 남은 시간이 이 값 이상이면 별 3개
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float StarThreeTime = 240.0f; // 기본 4분 이상 남으면 별 3개

	// 별 2개 기준 남은 시간
	// 남은 시간이 이 값 이상이면 별 2개
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float StarTwoTime = 120.0f; // 기본 2분 이상 남으면 별 2개

	// 게임 제한시간(초). 경과 시 게임 종료. 0 이하 = 무제한
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TimeLimitSeconds = 300.0f; // 기본 5분

	// 이 스테이지에서 획득 가능한 전체 목표 값어치. 가구별 BaseScore가 블루프린트 이벤트 그래프에서만
	// 관리되어(C++/DataTable에 없음) 자동 합산이 불가능하므로, 다른 스테이지별 상수(TimeLimitSeconds 등)와
	// 같은 방식으로 디자이너가 스테이지마다 직접 설정한다. S_InGame 팀 값어치 게이지(PB_TeamMoney)의
	// Max 값으로 쓰인다(UI_Technical_Spec.md 4장-7).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 TotalLevelValue = 10000;

protected:
	// 점수 계산
	int32 CalculateScore(float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 게임 종료 처리
	void FinishGame(bool bIsClear);
	
	// 별 개수 판정 (남은 시간 기준)
	int32 CalculateStar(float RemainingTime);

private:
	// 총 옮겨야 할 가구 개수
	int32 TotalFurnitureCount;

	// 트럭 안 가구 목록
	TArray<FTruckFurnitureInfo> FurnitureInTruck;

	// 트럭 안 가구 누적 점수 (CalculateFinalScore 최적화용)
	int32 AccumulatedScore;

	// 최종 점수 계산
	int32 CalculateFinalScore();
	
	// 카운트다운 남은 시간
	float CountdownTime;

	// 카운트다운 타이머 핸들
	FTimerHandle CountdownTimerHandle;
	
	// 튕긴 플레이어 ID 목록
	TArray<FUniqueNetIdRepl> DisconnectedPlayerIds;

	// GameState 캐시 (매 프레임 GetGameState 호출 방지)
	UPROPERTY()
	ATeamCarryGameState* CachedGameState = nullptr;

	// GameState 캐시 가져오기
	ATeamCarryGameState* GetCachedGameState();
};
