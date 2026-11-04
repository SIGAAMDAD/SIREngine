#include <Streaming/SIRStreamingCoordinator.h>

#include <stdint.h>

static sirStreamingDemand_t makeDemand( uint64_t nUnitId, uint64_t nConsumerId ) noexcept
{
	sirStreamingDemand_t demand = {};
	demand.keyUnit.nContainerId = 12;
	demand.keyUnit.nUnitId = nUnitId;
	demand.keyUnit.nManifestGeneration = 3;
	demand.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
	demand.nConsumerId = nConsumerId;
	demand.nConsumerGeneration = 1;
	demand.nRequestEpoch = 1;
	demand.nDeadlineTick = 60;
	demand.nPriority = 100;
	demand.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	return demand;
}

static bool testLifecycle( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRStreamingCoordinator coordinator;
	if ( coordinator.requiredBytes( 2 ) > sizeof( szStorage ) ) {
		return false;
	}
	if ( coordinator.initialize( szStorage + 1, sizeof( szStorage ) - 1, 2 ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	if ( coordinator.initialize( szStorage, sizeof( szStorage ), 2 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t first = {};
	sirStreamingDemand_t demand = makeDemand( 200, 7 );
	if ( coordinator.submit( demand, &first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.submit( demand, &first ) != SIR_STREAMING_DUPLICATE ) {
		return false;
	}

	sirStreamingSnapshot_t snapshot = {};
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_INVALID_TRANSITION ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.query( first, &snapshot ) != SIR_STREAMING_OK || snapshot.eStage != SIR_STREAMING_STAGE_ACTIVE ) {
		return false;
	}
	if ( coordinator.shutdown() != SIR_STREAMING_BUSY ) {
		return false;
	}
	if ( coordinator.cancel( first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.query( first, &snapshot ) != SIR_STREAMING_STALE_TICKET ) {
		return false;
	}
	return coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testCancellationAndCapacity( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRStreamingCoordinator coordinator;
	if ( coordinator.initialize( szStorage, sizeof( szStorage ), 2 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t demand = makeDemand( 30, 1 );
	sirStreamingTicket_t first = {};
	sirStreamingTicket_t second = {};
	if ( coordinator.submit( demand, &first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.cancel( first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_INVALID_TRANSITION ) {
		return false;
	}
	if ( coordinator.getStats().nCancellationPending != 1 ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_INVALID_TRANSITION ) {
		return false;
	}

	demand.nConsumerId = 2;
	if ( coordinator.submit( demand, &second ) != SIR_STREAMING_OK ) {
		return false;
	}
	demand.nConsumerId = 3;
	sirStreamingTicket_t rejected = {};
	if ( coordinator.submit( demand, &rejected ) != SIR_STREAMING_OUT_OF_CAPACITY ) {
		return false;
	}
	if ( coordinator.cancel( second ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.cancel( second ) != SIR_STREAMING_STALE_TICKET ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}
	return coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testGenerationAndTarget( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRStreamingCoordinator coordinator;
	if ( coordinator.initialize( szStorage, sizeof( szStorage ), 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t first = {};
	sirStreamingTicket_t second = {};
	sirStreamingDemand_t demand = makeDemand( 1, 1 );
	demand.eTarget = SIR_STREAMING_TARGET_PREPARED;
	if ( coordinator.submit( demand, &first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_INVALID_TRANSITION ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}
	demand.nConsumerId = 2;
	if ( coordinator.submit( demand, &second ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( first.nSlot != second.nSlot || first.nGeneration == second.nGeneration ) {
		return false;
	}
	if ( coordinator.cancel( first ) != SIR_STREAMING_STALE_TICKET ) {
		return false;
	}
	if ( coordinator.cancel( second ) != SIR_STREAMING_OK ) {
		return false;
	}
	return coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testIndependentConsumersAndFailure( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRStreamingCoordinator coordinator;
	if ( coordinator.initialize( szStorage, sizeof( szStorage ), 2 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t demand = makeDemand( 800, 21 );
	sirStreamingTicket_t first = {};
	sirStreamingTicket_t second = {};
	if ( coordinator.submit( demand, &first ) != SIR_STREAMING_OK ) {
		return false;
	}

	demand.nConsumerId = 22;
	if ( coordinator.submit( demand, &second ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.getStats().nOccupied != 2 ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_FAILED ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingSnapshot_t snapshot = {};
	if ( coordinator.query( second, &snapshot ) != SIR_STREAMING_OK || snapshot.eStage != SIR_STREAMING_STAGE_QUEUED ) {
		return false;
	}
	if ( coordinator.cancel( second ) != SIR_STREAMING_OK ) {
		return false;
	}

	demand.nRequestEpoch = 0;
	if ( coordinator.submit( demand, &second ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	return coordinator.shutdown() == SIR_STREAMING_OK;
}

int main( void )
{
	if ( !testLifecycle() ) {
		return 1;
	}
	if ( !testCancellationAndCapacity() ) {
		return 2;
	}
	if ( !testGenerationAndTarget() ) {
		return 3;
	}
	if ( !testIndependentConsumersAndFailure() ) {
		return 4;
	}
	return 0;
}
