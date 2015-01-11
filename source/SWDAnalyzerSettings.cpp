#include "SWDAnalyzerSettings.h"
#include <AnalyzerHelpers.h>


SWDAnalyzerSettings::SWDAnalyzerSettings()
:	mSWDIOChannel( UNDEFINED_CHANNEL ),
        mSWCLKChannel( UNDEFINED_CHANNEL )
{
	mSWDIOChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mSWDIOChannelInterface->SetTitleAndTooltip( "SWDIO", "Serial Wire Debug Input/Output" );
	mSWDIOChannelInterface->SetChannel( mSWDIOChannel );

	mSWCLKChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mSWCLKChannelInterface->SetTitleAndTooltip( "SWCLK", "Serial Wire Debug Clock" );
	mSWCLKChannelInterface->SetChannel( mSWCLKChannel );

	AddInterface( mSWDIOChannelInterface.get() );
	AddInterface( mSWCLKChannelInterface.get() );

	AddExportOption( 0, "Export as text/csv file" );
	AddExportExtension( 0, "text", "txt" );
	AddExportExtension( 0, "csv", "csv" );

	ClearChannels();
	AddChannel( mSWDIOChannel, "Serial", false );
	AddChannel( mSWCLKChannel, "Serial", false );
}

SWDAnalyzerSettings::~SWDAnalyzerSettings()
{
}

bool SWDAnalyzerSettings::SetSettingsFromInterfaces()
{
	mSWDIOChannel = mSWDIOChannelInterface->GetChannel();
	mSWCLKChannel = mSWCLKChannelInterface->GetChannel();

	ClearChannels();
	AddChannel( mSWDIOChannel, "Input/Output", true );
	AddChannel( mSWCLKChannel, "Clock", true );

	return true;
}

void SWDAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mSWDIOChannelInterface->SetChannel( mSWDIOChannel );
	mSWCLKChannelInterface->SetChannel( mSWCLKChannel );
}

void SWDAnalyzerSettings::LoadSettings( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString( settings );

	text_archive >> mSWDIOChannel;
	text_archive >> mSWCLKChannel;

	ClearChannels();
	AddChannel( mSWDIOChannel, "Input/Output", true );
	AddChannel( mSWCLKChannel, "Clock", true );

	UpdateInterfacesFromSettings();
}

const char* SWDAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive << mSWDIOChannel;
	text_archive << mSWCLKChannel;

	return SetReturnString( text_archive.GetString() );
}
