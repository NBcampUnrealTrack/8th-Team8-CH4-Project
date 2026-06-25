// TCNetStatics.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TCNetStatics.generated.h"

// TeamCarry 네트워크 전용 로그 카테고리
DECLARE_LOG_CATEGORY_EXTERN(LogTCNet, Log, All);

// 권위 체크·역할 로깅 등 네트워크 공용 헬퍼 모음
UCLASS()
class TEAMCARRY_API UTCNetStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 액터가 서버 권위(Authority)를 갖는지 — 상태 변경 전 가드용
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Net")
	static bool HasAuthority(const AActor* Actor);

	// 이 폰을 로컬에서 조종 중인지(소유 클라/싱글) — 트레이스·입력 게이트용
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Net")
	static bool IsLocallyControlledPawn(const APawn* Pawn);

	// 멀티 환경(리슨/데디/클라/싱글) 여부 — true면 복제 경로 검증 필요
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Net")
	static bool IsNetworked(const AActor* Actor);

	// ENetRole을 로그용 문자열로 변환
	static FString NetRoleToString(ENetRole Role);
};
