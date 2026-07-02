// TCSessionFlow.h

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TCSessionFlow.generated.h"

class UTCGameInstance;

// 세션 진행 단계 — UI 로딩/에러 표시에 사용.
UENUM(BlueprintType)
enum class ETCSessionPhase : uint8
{
	Idle,
	Creating,   // 방 생성 중
	Hosting,    // 호스트 성공(로비 진입)
	Searching,  // 세션 검색 중
	Joining,    // 조인 시도 중
	Joined,     // 조인 성공(클라 합류)
	Failed      // 실패(메시지 동반)
};

// 세션 단계 변화 통지(로딩 스피너/팝업/에러 토스트용).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTCSessionPhaseChanged, ETCSessionPhase, Phase, const FString&, Message);

/**
 * UTCSessionFlow - UI 와 Steam 세션(UTCGameInstance) 사이의 단일 바인딩 계층(seam).
 *
 * UI 위젯은 ReplaceState/세션API 를 직접 만지지 않고, 이 서브시스템의 "의도(intent)" 함수만 부른다.
 * 내부에서 UTCGameInstance 세션 API 호출 + 레벨 트래블 오케스트레이션 + 단계 통지를 수행한다.
 *
 * 레벨 경로는 UCLASS(Config=Game) 로 DefaultGame.ini 에서 덮어쓸 수 있다.
 * (EDITOR-TASKS §1 참고)
 */
UCLASS(Config = Game)
class TEAMCARRY_API UTCSessionFlow : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── UI 단계 통지 ──
	UPROPERTY(BlueprintAssignable, Category = "TeamCarry|Session")
	FOnTCSessionPhaseChanged OnSessionPhaseChanged;

	// ── 세이브 선택(슬롯) ──
	// S_SlotSelect 에서 슬롯 확정 시 호출. SlotName 은 후속 SaveGame 연동 지점.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void SetSaveSelection(const FString& InSlotName, bool bInContinue);

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	bool IsContinueMode() const { return bContinueMode; }

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	FString GetSelectedSlotName() const { return SelectedSlotName; }

	// ── 호스트 의도 ──
	// 방 생성 → 로비 레벨로 ServerTravel(HostSteamSession 콜백서 자동).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostCreateRoom();

	// 로비 시작(호스트) → 새 게임=Tutorial / 이어하기=StageSelect 로 ServerTravel.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostStartGame();

	// StageSelect 에서 스테이지 확정(호스트 독점) → 해당 맵으로 ServerTravel.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostTravelToStage(const FString& StageMapPath);

	// 세션 유지한 채 로비로 복귀(호스트).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostReturnToLobby();

	// ── 클라이언트 의도 ──
	// 방 코드로 접속. (현재는 TeamCarry 세션 필터 후 첫 결과 조인 — 코드매칭은 확장지점)
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void JoinRoomByCode(const FString& RoomCode);

	// ── 공용 ──
	// 세션 파기 후 타이틀 맵으로 복귀(호스트/클라 공통).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void LeaveToTitle();

	// 이 인스턴스가 호스트(서버 권위)인가.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	bool IsHost() const;

	// 호스트가 광고 중인 방 코드(UI 표시·공유용). 호스트 아니면 빈 문자열.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	FString GetRoomCode() const;

protected:
	// ── 레벨 경로(Config=Game 로 ini 덮어쓰기 가능) ──
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString TitleMapPath = TEXT("/Game/Maps/L_Title");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString LobbyMapPath = TEXT("/Game/Maps/L_Lobby");	

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString TutorialMapPath = TEXT("/Game/Maps/L_Tutorial");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString StageSelectMapPath = TEXT("/Game/Maps/L_StageSelect");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session")
	int32 MaxPlayers = 4;

private:
	// 선택된 세이브 슬롯/이어하기 여부(세션 수명 동안 유지).
	FString SelectedSlotName;
	bool bContinueMode = false;

	// UTCGameInstance 핸들/델리게이트 바인딩.
	UTCGameInstance* GetTCGameInstance() const;
	void BindGameInstanceEvents();
	void UnbindGameInstanceEvents();

	// UTCGameInstance 세션 콜백 핸들러.
	UFUNCTION()
	void HandleCreateSessionComplete(bool bSuccess);
	UFUNCTION()
	void HandleFindSessionsComplete(bool bSuccess, int32 NumFound);
	UFUNCTION()
	void HandleJoinSessionComplete(bool bSuccess);

	// 단계 통지 헬퍼.
	void SetPhase(ETCSessionPhase Phase, const FString& Message = FString());

	// 서버 권위에서 맵 트래블.
	void HostServerTravel(const FString& MapPath);

	// 조인 대기 중 코드(검색 완료 후 매칭에 사용).
	FString PendingJoinCode;
	bool bWantsJoinAfterSearch = false;
};
