#include "SWDAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "SWDAnalyzer.h"
#include "SWDAnalyzerSettings.h"
#include <iostream>
#include <fstream>
#include <sstream>

char dp_registers[][100] = {
	"IDCODE", "ABORT", "", "",
	"CTRL/STAT", "CTRL/STAT", "", "",
	"RESEND", "SELECT", "", "",
	"RDBUFF", "N/A !!!!", "", ""};


SWDAnalyzerResults::SWDAnalyzerResults( SWDAnalyzer* analyzer, SWDAnalyzerSettings* settings )
:	AnalyzerResults(),
	mSettings( settings ),
	mAnalyzer( analyzer )
{
}

SWDAnalyzerResults::~SWDAnalyzerResults()
{
}

void SWDAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base )
{
	ClearResultStrings();
	Frame frame = GetFrame( frame_index );
	U64 data = frame.mData1;

	char number_str[128];
	std::stringstream ss;


	if ( frame.mType == SWDReset ) {

		AddResultString( "RST" );
		AddResultString( "Reset" );

	}
	if ( frame.mType == SWDRequest ) {
		bool apdp	= (data & 0x40);
		bool rw	= (data & 0x20);
		int addr	= (data & 0x18) >> 1;

		AnalyzerHelpers::GetNumberString( addr, display_base, 8, number_str, 128 );

		AddResultString( "R" );
		AddResultString( "Req" );

		ss << "Req " << number_str;
		AddResultString( ss.str().c_str() ); ss.str("");

		ss << "Req " << (apdp?"AP ":"DP ") << number_str;
		AddResultString( ss.str().c_str() ); ss.str("");

		ss << "Req " << (apdp?"AP ":"DP ") << (rw?"Read ":"Write ") << number_str;
		AddResultString( ss.str().c_str() ); ss.str("");

		ss << "Host Requests " << (apdp?"AP ":"DP ") << (rw?"Read ":"Write ") << number_str;
		AddResultString( ss.str().c_str() ); ss.str("");

		ss << "Host Requests  "
		   << (apdp?"Access Port Access Register  ":"Debug Port  ")
		   << (rw?"Read  ":"Write  ") << number_str;
                AddResultString( ss.str().c_str() ); ss.str("");

		if (apdp) {

		} else { // Debug Port
			ss << "Host Requests  "
			   << (apdp?"Access Port Access Register  ":"Debug Port  ")
			   << (rw?"Read  ":"Write  ") << number_str << "   "
			   << "( " << dp_registers[addr + (rw?0:1)] << " )";
			AddResultString( ss.str().c_str() ); ss.str("");
		}

	}
	if ( frame.mType == SWDAck ) {
		int ack	= (data & 0x7);
		AnalyzerHelpers::GetNumberString( ack, display_base, 3, number_str, 128 );

		AddResultString( "A" );
		AddResultString( "Ack" );

		ss << "Ack " << number_str;
                AddResultString( ss.str().c_str() ); ss.str("");

		ss << "Ack " << number_str;
		if (ack == 1) {
		  ss << "  ( OK )";
		} else if (ack == 2) {
		  ss << "  ( WAIT )";
		} else if (ack == 4) {
		  ss << "  ( FAULT )";
		}
                AddResultString( ss.str().c_str() ); ss.str("");

	}
	if ( frame.mType == SWDWrite || frame.mType == SWDRead ) {

	  	AnalyzerHelpers::GetNumberString( data, display_base, 32, number_str, 128 );
		AddResultString( number_str );
	}
	if ( frame.mType == SWDWrite ) {
	}
	if ( frame.mType == SWDRead ) {

	}
}

void SWDAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
	std::ofstream file_stream( file, std::ios::out );

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	file_stream << "Time [s],Value" << std::endl;

	U64 num_frames = GetNumFrames();
	for( U32 i=0; i < num_frames; i++ )
	{
		Frame frame = GetFrame( i );

		char time_str[128];
		AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

		char number_str[128];
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

		file_stream << time_str << "," << number_str << std::endl;

		if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
		{
			file_stream.close();
			return;
		}
	}

	file_stream.close();
}

void SWDAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	Frame frame = GetFrame( frame_index );
	ClearResultStrings();

	char number_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );
	AddResultString( number_str );
}

void SWDAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}

void SWDAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}
