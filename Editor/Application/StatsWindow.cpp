#include "StatsWindow.h"
#include "Project/ProjectManager.h"
#include <implot/implot.h>
#include <Engine/Memory/MemoryStats.h>
#include <EABase/int128.h>

namespace Valden {

CStatsWindow CStatsWindow::g_StatsWindow;

CStatsWindow::CStatsWindow( void )
	: IEditorWidget( "StatsWindow" ), m_nCaptureFrame( 0 ), m_bActive( false )
{
	m_szMemoryUsageTime[ 0 ] = 10 * 1000;
	m_szMemoryUsageTime[ 1 ] = 20 * 1000;
	m_szMemoryUsageTime[ 2 ] = 30 * 1000;
	m_szMemoryUsageTime[ 3 ] = 40 * 1000;
	m_szMemoryUsageTime[ 4 ] = 50 * 1000;
	m_szMemoryUsageTime[ 5 ] = 60 * 1000;
}

void CStatsWindow::Draw( void )
{
	const Application::MemoryConstants_t MemoryConstants = Application::Get()->GetMemoryConstants();
	const Application::MemoryStats_t MemoryStats = Application::Get()->GetMemoryStats();
	uint64_t nTotalMemory;
	uint64_t nTotalFiles;
	uint64_t nTotalFolders;

	{
		const uint64_t nIndex = m_nCaptureFrame++;

		m_szVirtualMemoryUsage[ nIndex % MAX_CAPTURE_FRAMES ] = MemoryStats.nUsedVirtual;
		m_szPhysicalMemoryUsage[ nIndex % MAX_CAPTURE_FRAMES ] = MemoryStats.nUsedPhysical;
	}

	nTotalFolders = g_pFileSystem->GetFileCache().size();
	nTotalFiles = 0;
	nTotalMemory = 0;
	for ( const auto& it : g_pFileSystem->GetFileCache() ) {
		nTotalFiles += it.second->GetCachedFiles().size();
		for ( const auto& file : it.second->GetCachedFiles() ) {
			nTotalMemory += file.second.nSize;
		}
	}

	if ( !m_bActive ) {
		return;
	}

	if ( ImGui::Begin( "FileCache Statistics", NULL, ImGuiWindowFlags_NoCollapse ) ) {
		ImGui::SeparatorText( "Total Statistics" );
		ImGui::Text( "Total Cached Memory: %s", SIREngine_GetMemoryString( (double)nTotalMemory ) );
		ImGui::Text( "Total Cached Files: %lu", nTotalFiles );
		ImGui::Text( "Total Cached Folders: %lu", nTotalFolders );
		ImGui::Separator();

		if ( ImGui::BeginTable( "##FileCacheListStatistics", 2, ImGuiTableFlags_Resizable ) ) {
			for ( const auto& it : g_pFileSystem->GetFileCache() ) {
				ImGui::TableNextColumn();
				if ( ImGui::CollapsingHeader( it.first.c_str() ) ) {
					ImGui::TableNextColumn();
					if ( ImGui::BeginTable( "##FileCacheListStatistics_Table", 3, ImGuiTableFlags_Resizable ) ) {
						ImGui::TableNextColumn();
						ImGui::TextUnformatted( ICON_FA_FILE_ARCHIVE "File Path" );
						ImGui::TableNextColumn();
						ImGui::TextUnformatted( "Size (B)" );
						ImGui::TableNextColumn();
						ImGui::TextUnformatted( "Mapped Address" );
						ImGui::TableNextRow();

						ImGui::Separator();

						for ( const auto& file : it.second->GetCachedFiles() ) {
							ImGui::TableNextColumn();
							ImGui::TextUnformatted( file.first.c_str() );

							ImGui::TableNextColumn();
							ImGui::Text( "%s", SIREngine_GetMemoryString( (double)file.second.nSize ) );

							ImGui::TableNextColumn();
							ImGui::Text( "0x%lx", file.second.pMemory );

							ImGui::TableNextRow();
						}
						ImGui::EndTable();
					}
				}

				ImGui::TableNextRow();
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}

	if ( ImGui::Begin( "Engine Statistics", &m_bActive, ImGuiWindowFlags_NoCollapse ) ) {
		ImGui::SeparatorText( "Platform Memory Limits" );
		ImGui::Text( "\tTotal Physical RAM: %s", SIREngine_GetMemoryString( MemoryConstants.nTotalPhysical ) );
		ImGui::Text( "\tTotal Virtaul RAM: %s", SIREngine_GetMemoryString( MemoryConstants.nTotalVirtual ) );

		ImGui::SeparatorText( "Platform Memory Usage" );

		if ( ImPlot::BeginPlot( "Platform Memory Usage##ImGuiMemoryUsagePlot" ) ) {
			ImPlot::PlotLine<double>( "Virtual Memory Usage", m_szMemoryUsageTime, m_szVirtualMemoryUsage, MAX_CAPTURE_FRAMES );
			ImPlot::PlotLine<double>( "Physical Memory Usage", m_szMemoryUsageTime, m_szPhysicalMemoryUsage, MAX_CAPTURE_FRAMES );
			ImPlot::EndPlot();
		}
		ImGui::Text( "\tUsed Physical RAM: %s", SIREngine_GetMemoryString( MemoryStats.nUsedPhysical ) );
		ImGui::Text( "\tUsed Virtual RAM: %s", SIREngine_GetMemoryString( MemoryStats.nUsedVirtual ) );
		ImGui::Text( "\tPeak Physical RAM: %s", SIREngine_GetMemoryString( MemoryStats.nPeakUsedPhysical ) );
		ImGui::Text( "\tPeak Virtual RAM: %s", SIREngine_GetMemoryString( MemoryStats.nPeakUsedVirtual ) );
		ImGui::Text( "\tAvailable Physical RAM: %s", SIREngine_GetMemoryString( MemoryStats.nAvailablePhysical ) );
		ImGui::Text( "\tAvailable Virtual RAM: %s", SIREngine_GetMemoryString( MemoryStats.nAvailableVirtual ) );

		ImGui::End();
	}
}

void CStatsWindow::Dock( void )
{
}

};