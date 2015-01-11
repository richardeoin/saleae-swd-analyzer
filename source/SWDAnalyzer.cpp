#include "SWDAnalyzer.h"
#include "SWDAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>
#include <deque>
#include <bitset>

SWDAnalyzer::SWDAnalyzer()
:	Analyzer(),
	mSettings( new SWDAnalyzerSettings() ),
	mSimulationInitilized( false )
{
	SetAnalyzerSettings( mSettings.get() );
}

SWDAnalyzer::~SWDAnalyzer()
{
	KillThread();
}

///
/// Pushes to the bitset we use
///
void SWDAnalyzer::PushToBitset(std::bitset<32>& bitset, bool bit)
{
	for (int i = 30; (i+1); i--) {
		bitset[i+1] = bitset[i];
	}
	bitset[0] = bit;
}

///
/// Fast-forwards to the next master sample and marks it
///
U64 SWDAnalyzer::AdvanceToMasterSample(U64& last_clock_end)
{
	// Clock is idle low. Goto low state
  	if( mClock->GetBitState() == BIT_HIGH ) {
		mClock->AdvanceToNextEdge();
		last_clock_end = mClock->GetSampleNumber();
	}

	// Goto rising edge. Data is latched here
	mClock->AdvanceToNextEdge();
	mResults->AddMarker( mClock->GetSampleNumber(), AnalyzerResults::UpArrow, mSettings->mSWCLKChannel );

	// Fast forward the data channel too
	mData->AdvanceToAbsPosition( mClock->GetSampleNumber() );
	return mClock->GetSampleNumber();
}
///
/// Fast-forwards to the next slave sample and marks it
///
U64 SWDAnalyzer::AdvanceToSlaveSample(U64& last_clock_end)
{
	// Clock is idle low. Goto low state
	if( mClock->GetBitState() == BIT_HIGH ) {
		mClock->AdvanceToNextEdge();
		last_clock_end = mClock->GetSampleNumber();
	}

	// Goto falling edge. Data is latched here
	mClock->AdvanceToNextEdge();
	mResults->AddMarker( mClock->GetSampleNumber(), AnalyzerResults::UpArrow, mSettings->mSWCLKChannel );

	// Fast-forward the data channel too
	mData->AdvanceToAbsPosition( mClock->GetSampleNumber() );
	return mClock->GetSampleNumber();
}
///
/// Advance a single edge
///
void SWDAnalyzer::AdvanceSingleEdge()
{
	// Go through a single edge
  	mClock->AdvanceToNextEdge();

	// Fast-forward the data channel too
	mData->AdvanceToAbsPosition( mClock->GetSampleNumber() );
}
///
/// Advance through a turnaround
///
void SWDAnalyzer::AdvanceThroughTurn()
{
	// Go through a clock cycle
  	mClock->AdvanceToNextEdge();
	mClock->AdvanceToNextEdge();

	// Fast-forward the data channel too
	mData->AdvanceToAbsPosition( mClock->GetSampleNumber() );
}


///
/// Adds a reset frame
///
void SWDAnalyzer::AddResetFrame(std::deque<U64> sample_builder)
{
	// Put together a frame
	Frame frame;
	frame.mData1 = 0;
	frame.mFlags = 0;
	frame.mType = SWDReset;
	frame.mStartingSampleInclusive = sample_builder.at(7);
	frame.mEndingSampleInclusive = mClock->GetSampleNumber();

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}
///
/// Adds a request frame. Returns true if frame is valid
///
bool SWDAnalyzer::AddRequestFrame(std::deque<U64> sample_builder, U64 data)
{
	// Put together a frame
	Frame frame;
	frame.mData1 = data & 0xFF;
	frame.mFlags = 0;
	frame.mType = SWDRequest;
	frame.mStartingSampleInclusive = sample_builder.at(7);
	frame.mEndingSampleInclusive = mClock->GetSampleNumber();

	// Test parity
	int payload = (data & 0x7C) >> 2;
	bool parity_good = true;
	while (payload) {
	  if (payload & 1) parity_good = !parity_good;
	  payload >>= 1;
	}

	// Mark bad parity
	if (!parity_good) {
		mResults->AddMarker( sample_builder.at(2),
				     AnalyzerResults::ErrorSquare, mSettings->mSWDIOChannel );
	}

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

	return true;
}

int SWDAnalyzer::ReadAcknowledge()
{
	std::bitset<3> ack;
	U64 start, end, dummy;

	// Read in 3 bits, lsb first
	start = AdvanceToSlaveSample(dummy);
	ack[0] = mData->GetBitState();
	AdvanceToSlaveSample(dummy);
	ack[1] = mData->GetBitState();
	AdvanceToSlaveSample(dummy);
	ack[2] = mData->GetBitState();

	AdvanceSingleEdge();
	end = mClock->GetSampleNumber();

	int data = ack.to_ulong() & 0x7;

	// Put together a frame
	Frame frame;
	frame.mData1 = data;
	frame.mFlags = 0;
	frame.mType = SWDAck;
	frame.mStartingSampleInclusive = start;
	frame.mEndingSampleInclusive = end;

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

	return data;
}
void SWDAnalyzer::ReadData(enum TransactionType type)
{
	std::bitset<33> data;
	U64 start, end, dummy, parity;

	// Read bits
	if (type == TT_READ) {
		start = AdvanceToSlaveSample(dummy);
	} else {
		start = AdvanceToMasterSample(dummy);
	}
	data[0] = mData->GetBitState();
	for (int i = 1; i < 33; i++) {
		if (type == TT_READ) {
			parity = AdvanceToSlaveSample(dummy);
		} else {
			parity = AdvanceToMasterSample(dummy);
		}
		data[i] = mData->GetBitState();
	}

	AdvanceSingleEdge();
	end = mClock->GetSampleNumber();

	// Check the parity
	U64 payload = data.to_ulong();
	bool parity_good = true;
	while (payload) {
	  if (payload & 1) parity_good = !parity_good;
	  payload >>= 1;
	}

	// Mark bad parity
	if (!parity_good) {
		mResults->AddMarker( parity,
				     AnalyzerResults::ErrorSquare, mSettings->mSWDIOChannel );
	}

	// Get the actual value
	//U64 value = (payload >> 1) & 0xFFFFFFFF;

	// Put together a frame
	Frame frame;
	frame.mData1 = data.to_ulong();
	frame.mFlags = 0;
	frame.mType = (type == TT_READ) ? SWDRead : SWDWrite;
	frame.mStartingSampleInclusive = start;
	frame.mEndingSampleInclusive = end;

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

void SWDAnalyzer::WorkerThread()
{
	mResults.reset( new SWDAnalyzerResults( this, mSettings.get() ) );
	SetAnalyzerResults( mResults.get() );
	mResults->AddChannelBubblesWillAppearOn( mSettings->mSWDIOChannel );

	mData = GetAnalyzerChannelData( mSettings->mSWDIOChannel );
	mClock = GetAnalyzerChannelData( mSettings->mSWCLKChannel );

	std::deque<U64> sample_builder;
	std::bitset<32> data_builder;
	sample_builder.clear();
	data_builder.set();

	U64 last_clock_end;

	for( ; ; )
	{
		// Record a sample
		sample_builder.push_front( AdvanceToMasterSample(last_clock_end) );
		PushToBitset(data_builder, mData->GetBitState() );

		U64 data = data_builder.to_ulong();

		// If we have a reset
		if (sample_builder.size() >= 8 && (data & 0xFF) == 0) {

			// Yes, we have a reset
			AdvanceSingleEdge();
			AddResetFrame( sample_builder );

			sample_builder.clear();
			data_builder.set();
		}

		// If we have a request
		if (sample_builder.size() >= 8 && (data & 0x83) == 0x81) {

			// Yes, we have a request
			AdvanceSingleEdge();
			if (AddRequestFrame( sample_builder, data )) {

				// Determine if this is read or write
				enum TransactionType type = (data & 0x20) ? TT_READ : TT_WRITE;

				AdvanceThroughTurn();

				ReadAcknowledge();

				if (type == TT_WRITE) AdvanceThroughTurn();
				ReadData(type);
				if (type == TT_READ) AdvanceThroughTurn();
			}

			sample_builder.clear();
			data_builder.set();
		}

		mResults->CommitResults();
	}
}

bool SWDAnalyzer::NeedsRerun()
{
	return false;
}

U32 SWDAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	if( mSimulationInitilized == false )
	{
		mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
		mSimulationInitilized = true;
	}

	return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 SWDAnalyzer::GetMinimumSampleRateHz()
{
	return 25000;
}

const char* SWDAnalyzer::GetAnalyzerName() const
{
	return "Serial Wire Debug";
}

const char* GetAnalyzerName()
{
	return "Serial Wire Debug";
}

Analyzer* CreateAnalyzer()
{
	return new SWDAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
	delete analyzer;
}
