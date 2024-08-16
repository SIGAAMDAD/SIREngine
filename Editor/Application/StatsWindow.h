#ifndef __VALDEN_STATS_WINDOW_H__
#define __VALDEN_STATS_WINDOW_H__

#pragma once

#include "Application.h"

#define MAX_CAPTURE_FRAMES 16

namespace Valden {
	class CStatsWindow : public IEditorWidget
	{
	public:
		CStatsWindow( void );
		virtual ~CStatsWindow() override
		{ }

		virtual void Draw( void ) override;
		virtual void Dock( void ) override;

		void SetActive( void )
		{ m_bActive = true; }

		SIRENGINE_FORCEINLINE static CStatsWindow& Get( void )
		{ return g_StatsWindow; }
	private:
		double m_szVirtualMemoryUsage[ MAX_CAPTURE_FRAMES ];
		double m_szPhysicalMemoryUsage[ MAX_CAPTURE_FRAMES ];
		double m_szMemoryUsageTime[ MAX_CAPTURE_FRAMES ];
		uint64_t m_nCaptureFrame;
		
		bool m_bActive;

		static CStatsWindow g_StatsWindow;
	};
};

#endif