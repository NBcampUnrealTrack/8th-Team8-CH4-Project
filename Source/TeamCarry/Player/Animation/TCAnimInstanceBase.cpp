// TCAnimInstanceBase.cpp


#include "Player/Animation/TCAnimInstanceBase.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// 매 프레임 애니메이션 상태 갱신 함수
void UTCAnimInstanceBase::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Pawn 가져오기
	APawn* OwnerPawn = TryGetPawnOwner();

	// Pawn 유효성 확인
	if (IsValid(OwnerPawn))
	{
		// Z축을 제외한 평면(X, Y) 이동 속도만 계산해서 Speed 변수에 저장
		Speed = OwnerPawn->GetVelocity().Size2D();

		// 캐릭터 클래스 형변환
		ACharacter* Character = Cast<ACharacter>(OwnerPawn);

		// 형변환 성공 시
		if (IsValid(Character))
		{
			// 캐릭터가 공중에 있는지 확인해서 bIsFalling에 저장
			bIsFalling = Character->GetCharacterMovement()->IsFalling();
		}
	}
}
