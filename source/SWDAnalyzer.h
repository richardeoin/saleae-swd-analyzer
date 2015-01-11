#ifndef SWD_ANALYZER_H
#define SWD_ANALYZER_H

#include <Analyzer.h>
#include "SWDAnalyzerResults.h"
#include "SWDSimulationDataGenerator.h"
#include <deque>
#include <bitset>

enum TransactionType {
	TT_READ,
	TT_WRITE,
};

class SWDAnalyzerSettings;
class ANALYZER_EXPORT SWDAnalyzer : public Analyzer
{
public:
	SWDAnalyzer();
	virtual ~SWDAnalyzer();
	virtual void WorkerThread();

	virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
	virtual U32 GetMinimumSampleRateHz();

	virtual const char* GetAnalyzerName() const;
	virtual bool NeedsRerun();

protected:// functions

	void PushToBitset(std::bitset<32>& bitset, bool bit);

	U64 AdvanceToMasterSample(U64& last_clock_end);
	U64 AdvanceToSlaveSample(U64& last_clock_end);
	void AdvanceSingleEdge();
	void AdvanceThroughTurn();

	void AddResetFrame(std::deque<U64> sample_builder);
	bool AddRequestFrame(std::deque<U64> sample_builder, U64 data);

	int ReadAcknowledge();
	void ReadData(enum TransactionType type);

protected: //vars
	std::auto_ptr< SWDAnalyzerSettings > mSettings;
	std::auto_ptr< SWDAnalyzerResults > mResults;
	AnalyzerChannelData* mData;
	AnalyzerChannelData* mClock;

	SWDSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

	//Serial analysis vars:
	U32 mSampleRateHz;
	U32 mStartOfStopBitOffset;
	U32 mEndOfStopBitOffset;
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //SWD_ANALYZER_H
