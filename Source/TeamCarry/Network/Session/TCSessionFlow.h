// TCSessionFlow.h

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "TCSessionFlow.generated.h"

class UTCGameInstance;

// 스테이지 정의(명세 2장·7장-3). DT_Stages(DataTable) 의 행 구조체.
// 현재는 StageId=1 / L_LevelProto 단일 폴백 행만 존재하지만, 행이 늘어나도
// O_StageSelect/GetSelectedStageMapPath() 는 수정 없이 그대로 확장된다.
USTRUCT(BlueprintType)
struct FStageInfo : public FTableRowBase
{
	GENERATED_BODY()

	// 스테이지 식별자. SetStageSelection()/GetSelectedStageId() 가 참조하는 키.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Stage")
	int32 StageId = 0;

	// UI 표시용 이름(O_StageSelect 카드 라벨).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Stage")
	FText DisplayName;

	// 트래블 대상 맵 경로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Stage")
	FString MapPath;

	// 확장 여지(미구현): 썸네일 SoftObjectPtr, 해금 조건, 별 획득 조건 등.
};

// 세이브 슬롯 하나의 요약 정보(명세: 슬롯 선택 화면/저장 관리 오버레이 공용).
// GetAllSaveSlotInfos() 가 슬롯 개수(NumSaveSlots)만큼 매번 새로 조회해 만든다 — 복제/캐시되지 않음.
USTRUCT(BlueprintType)
struct FSaveSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TeamCarry|Save")
	int32 SlotIndex = 0;

	// UGameplayStatics::SaveGameToSlot/DoesSaveGameExist 등에 그대로 넘기는 실제 슬롯 이름.
	UPROPERTY(BlueprintReadOnly, Category = "TeamCarry|Save")
	FString SlotName;

	// 이 슬롯에 세이브 파일이 존재하는지(UGameplayStatics::DoesSaveGameExist). UI 가 New/Saved
	// 카드 위젯 중 무엇을 보여줄지 이 값으로 판단한다.
	UPROPERTY(BlueprintReadOnly, Category = "TeamCarry|Save")
	bool bHasSaveData = false;

	// 존재하는 경우, 그 세이브의 마지막 플레이 스테이지(UTCSaveGame::LastPlayedStage) — 카드 표시용.
	UPROPERTY(BlueprintReadOnly, Category = "TeamCarry|Save")
	FString LastPlayedStage;
};

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

// 레벨 트래블 시작 통지(S_Loading 표시 트리거용). 실제 트래블 직전에 Broadcast된다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTCTravelStarted, const FString&, TargetMapPath);

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

	// 레벨 트래블 시작 통지(S_Loading 표시용). 각 트래블 지점(HostServerTravel/LeaveToTitle/
	// 세션 콜백 경유 호스트 생성·조인)에서 실제 트래블 직전에 Broadcast된다.
	UPROPERTY(BlueprintAssignable, Category = "TeamCarry|Session")
	FOnTCTravelStarted OnTravelStarted;

	// ── 세이브 선택(슬롯) ──
	// S_SlotSelect 에서 슬롯 확정 시 호출. SlotName 은 후속 SaveGame 연동 지점.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void SetSaveSelection(const FString& InSlotName, bool bInContinue);

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	bool IsContinueMode() const { return bContinueMode; }

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	FString GetSelectedSlotName() const { return SelectedSlotName; }

	// 슬롯 인덱스(0..NumSaveSlots-1)로부터 실제 세이브 슬롯 이름을 만든다("SaveSlot_0" 형식).
	// S_SlotSelect/O_SaveLoad 가 카드 위치와 슬롯을 매칭하는 데 사용하는 단일 규칙.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Save")
	static FString MakeSaveSlotName(int32 SlotIndex) { return FString::Printf(TEXT("SaveSlot_%d"), SlotIndex); }

	// NumSaveSlots 개 슬롯 전체를 스캔해 존재 여부/마지막 플레이 스테이지를 조회한다
	// (UGameplayStatics::DoesSaveGameExist 기반, 매 호출마다 새로 스캔).
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Save")
	TArray<FSaveSlotInfo> GetAllSaveSlotInfos() const;

	// 해당 슬롯의 세이브 파일을 영구 삭제한다(UGameplayStatics::DeleteGameInSlot). 되돌릴 수 없으므로
	// 호출부(O_Confirm 등)에서 먼저 확인을 받아야 한다.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session|Save")
	void DeleteSaveSlot(const FString& SlotName);

	// ── 스테이지 선택 ──
	// O_StageSelect 에서 스테이지 확정(호스트 전용) 시 호출. 세션 수명 동안 유지.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void SetStageSelection(int32 InStageId);

	// 선택된 스테이지 ID 조회. 미선택(0 이하) 시 기본 1스테이지를 반환한다.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	int32 GetSelectedStageId() const;

	// 선택된 스테이지의 맵 경로 조회. StageDataTable 에 일치하는 행이 있으면 그 MapPath 를,
	// 없으면(DT_Stages 미설정/행 없음) DefaultStageMapPath 로 폴백한다.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	FString GetSelectedStageMapPath() const;

	// O_StageSelect 목록 UI 용 전체 스테이지 조회(DT_Stages 미설정 시 빈 배열).
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	TArray<FStageInfo> GetAllStageInfos() const;

	// StageId 로 단일 스테이지 정보 조회. 찾으면 true.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session")
	bool FindStageInfo(int32 StageId, FStageInfo& OutInfo) const;

	// ── 호스트 의도 ──
	// 방 생성 → 로비 레벨로 ServerTravel(HostSteamSession 콜백서 자동).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostCreateRoom();

	// 로비 시작(호스트) → 새 게임=Tutorial / 이어하기=선택된 스테이지(미선택 시 기본 1스테이지)로 직행 ServerTravel.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostStartGame();

	// 세션 유지한 채 로비로 복귀(호스트).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void HostReturnToLobby();

	// 세션 유지한 채 현재 선택된 스테이지 맵으로 재트래블(호스트). O_PauseMenu(Btn_Reset)가 호출한다.
	// 가구 배치·팀 값어치·타이머 등 인게임 상태를 초기화한다(UI_Technical_Spec.md 2장·4장-12).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void RestartStage();

	// 튜토리얼 마지막 Step 완료/건너뛰기(호스트 전용, 명세 4장-6·5장) 시 S_Tutorial 이 호출.
	// 세이브에 bTutorialCompleted=true 를 기록하고, 같은 방의 다음 HostStartGame() 이 이어하기
	// (스테이지 직행) 경로를 타도록 전환한 뒤, 세션 유지한 채 로비로 복귀한다.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Session")
	void CompleteTutorial();

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

	// ── 레벨 경로 조회(ATCPlayerController::BeginPlay 의 맵 판별용) ──
	// DefaultGame.ini 로 덮어쓴 실제 값을 그대로 돌려준다 — 호출부가 맵 이름을 하드코딩하지 않도록.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Maps")
	FString GetTitleMapPath() const { return TitleMapPath; }

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Maps")
	FString GetLobbyMapPath() const { return LobbyMapPath; }

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Session|Maps")
	FString GetTutorialMapPath() const { return TutorialMapPath; }

protected:
	// ── 레벨 경로(Config=Game 로 ini 덮어쓰기 가능) ──
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString TitleMapPath = TEXT("/Game/Maps/L_Title");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString LobbyMapPath = TEXT("/Game/Maps/L_Lobby");	

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString TutorialMapPath = TEXT("/Game/Maps/L_Tutorial");

	// 스테이지 데이터(DT_Stages) 조회 실패 시 GetSelectedStageMapPath()가 반환하는 단일 폴백 맵.
	// L_StageSelect 맵은 폐기되었으므로(v2), 이어하기는 항상 이 경로(또는 DT_Stages 행)로 직행한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Maps")
	FString DefaultStageMapPath = TEXT("/Game/Prototype/L_LevelProto");

	// 스테이지 목록 데이터 테이블(행 구조체=FStageInfo). DefaultGame.ini 에서 지정.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "TeamCarry|Session|Stages")
	TSoftObjectPtr<UDataTable> StageDataTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session")
	int32 MaxPlayers = 4;

	// 세이브 슬롯 총 개수(S_SlotSelect/O_SaveLoad 카드 4개와 일치). MakeSaveSlotName()/
	// GetAllSaveSlotInfos() 가 이 값 기준으로 "SaveSlot_0".."SaveSlot_{N-1}" 을 스캔한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TeamCarry|Session|Save")
	int32 NumSaveSlots = 4;

private:
	// 선택된 세이브 슬롯/이어하기 여부(세션 수명 동안 유지).
	FString SelectedSlotName;
	bool bContinueMode = false;

	// 선택된 스테이지 ID(0 = 미선택 → GetSelectedStageId()가 기본 1스테이지로 폴백).
	int32 SelectedStageId = 0;

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
