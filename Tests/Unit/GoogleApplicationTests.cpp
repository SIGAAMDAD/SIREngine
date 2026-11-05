#include <Core/SIRApplication.h>
#include <gtest/gtest.h>

TEST( SIRApplicationGoogleTest, InitialStateAndInvalidTick )
{
	SIRApplication application;

	EXPECT_EQ( application.getState(), SIR_APP_UNINITIALIZED );
	EXPECT_EQ( application.getTickCount(), 0U );
	EXPECT_FALSE( application.tick() );
}

TEST( SIRApplicationGoogleTest, InitializationIsSingleUse )
{
	SIRApplication application;

	EXPECT_TRUE( application.initialize() );
	EXPECT_EQ( application.getState(), SIR_APP_READY );
	EXPECT_FALSE( application.initialize() );
}

TEST( SIRApplicationGoogleTest, TickCountTracksSuccessfulTicks )
{
	SIRApplication application;

	ASSERT_TRUE( application.initialize() );

	for ( uint32_t iTick = 0; iTick < 32; ++iTick ) {
		ASSERT_TRUE( application.tick() );
	}

	EXPECT_EQ( application.getTickCount(), 32U );
}

TEST( SIRApplicationGoogleTest, ShutdownStopsTickingAndIsIdempotent )
{
	SIRApplication application;

	ASSERT_TRUE( application.initialize() );
	ASSERT_TRUE( application.tick() );

	application.shutdown();
	EXPECT_EQ( application.getState(), SIR_APP_STOPPED );
	EXPECT_FALSE( application.tick() );
	EXPECT_FALSE( application.initialize() );

	application.shutdown();
	EXPECT_EQ( application.getTickCount(), 1U );
}
