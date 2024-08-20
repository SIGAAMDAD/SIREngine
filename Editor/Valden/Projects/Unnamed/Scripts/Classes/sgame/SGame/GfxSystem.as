#include "Engine/Renderer/LocalEntity.as"
#include "Engine/Renderer/Particle.as"

namespace TheNomad::SGame {
	class GfxSystem : TheNomad::GameSystem::GameObject {
		GfxSystem() {
		}

		void OnInit() {
		}
		void OnShutdown() {
		}

		TheNomad::Engine::Renderer::PolyVert[]@ GetVerts() {
			return @m_DrawVerts;
		}

		void OnLoad() {
		}
		void OnSave() const {
		}
		void OnRunTic() {
		}
		void OnPlayerDeath( int ) {
		}
		void OnCheckpointPassed( uint ) {
		}
		void OnLevelStart() {
			InitLocalEntities();
		}
		void OnLevelEnd() {
			m_LocalEnts.Clear();
		}
		const string& GetName() const override {
			return "GfxManager";
		}
		void OnRenderScene() {
			TheNomad::Engine::Renderer::LocalEntity@ ent;
			TheNomad::Engine::Renderer::LocalEntity@ next;

			// walk the list backwards, so any new local entities generated
			// (trails, marks, etc) will be present this frame
			@ent = @m_ActiveLocalEnts.m_Prev;
			for ( ; @ent !is @m_ActiveLocalEnts; @ent = @next ) {
				// grab next now, so if the local entity is freed we
				// still have it
				@next = @ent.m_Prev;

				if ( TheNomad::GameSystem::GameManager.GetGameTic() - ent.m_nStartTime >= ent.m_nLifeTime ) {
					FreeLocalEntity( @ent );
					continue;
				}

				ent.RunTic();
			}
		}

		private void InitLocalEntities() {
			const uint numGfx = 512;
			m_LocalEnts.Resize( numGfx );

			@m_ActiveLocalEnts.m_Next = @m_ActiveLocalEnts;
			@m_ActiveLocalEnts.m_Prev = @m_ActiveLocalEnts;
			@m_FreeLocalEnts = @m_LocalEnts[0];

			for ( uint i = 0; i < m_LocalEnts.Count() - 1; i++ ) {
				@m_LocalEnts[i].m_Next = @m_LocalEnts[i + 1];
			}
		}

		private TheNomad::Engine::Renderer::LocalEntity@ AllocLocalEntity() {
			TheNomad::Engine::Renderer::LocalEntity@ ent;
			uint time;

			if ( @m_FreeLocalEnts is null ) {
				// no free polys, so free the one at the end of the chain
				// remove the oldest active entity
				FreeLocalEntity( @m_ActiveLocalEnts.m_Prev );
			}

			@ent = @m_FreeLocalEnts;
			@m_FreeLocalEnts = @m_FreeLocalEnts.m_Next;

			// link into active list
			@ent.m_Next = @m_ActiveLocalEnts.m_Next;
			@ent.m_Prev = @m_ActiveLocalEnts;
			@m_ActiveLocalEnts.m_Next.m_Prev = @ent;
			@m_ActiveLocalEnts.m_Next = @ent;

			return @ent;
		}
		//
		// FreeLocalEntity: only LocalEntity when its finished should ever call this, or AllocLocalEntity
		//
		private void FreeLocalEntity( TheNomad::Engine::Renderer::LocalEntity@ ent ) {
			if ( @ent.m_Prev is null ) {
				GameError( "GfxManager::FreeLocalEntity: not active" );
			}

			// remove from doubly linked list
			@ent.m_Prev.m_Next = @ent.m_Next;
			@ent.m_Next.m_Prev = @ent.m_Prev;

			// the free list is only singly linked
			@ent.m_Next = @m_FreeLocalEnts;
			@m_FreeLocalEnts = @ent;
		}


		//
		// GfxSystem::Bleed: this is a spurt of blood when an entity gets hit
		//
		TheNomad::Engine::Renderer::LocalEntity@ Bleed( const vec3& in origin ) {
			TheNomad::Engine::Renderer::LocalEntity@ ent = null;

			if ( sgame_Blood.GetInt() == 0 ) {
				return null;
			}

			float x, y;
			const uint randX = Util::PRandom();
			const uint randY = Util::PRandom();

			@ent = AllocLocalEntity();

			x = origin.x - ( 1.25f / ( randX == 0 ? 1 : randX ) );
			y = origin.y - ( 1.25f / ( randY == 0 ? 1 : randY ) );

			ent.m_nStartTime = TheNomad::GameSystem::GameManager.GetGameTic();
			ent.m_nLifeTime = 1000;
			ent.m_Velocity = vec3( 0.05f, -0.03f, 0.0f );

			ent.m_Origin = vec3( x, y - 0.2f, 0.0f );
			ent.m_hShader = TheNomad::Engine::ResourceCache.GetShader( "gfx/bloodSplatter0" );
			ent.m_bGravity = true;

			return @ent;
		}

		void SmokePuff( const vec3& in origin, const vec3& in vel ) {
		}

		void AddDustPoly( const vec3& in origin, const vec3& in vel, uint lifeTime, int hShader ) {
			TheNomad::Engine::Renderer::LocalEntity@ ent;

			@ent = AllocLocalEntity();

			ent.m_nStartTime = TheNomad::GameSystem::GameManager.GetGameTic();
			ent.m_nLifeTime = lifeTime;
			ent.m_Velocity = vel;

			ent.m_bGravity = false;
			ent.m_Origin = origin;
			ent.m_hShader = hShader;
		}
		
		void AddMarkPoly() {
		}
		void AddSmokePoly() {
		}
		void AddFlarePoly() {
		}

		void AddExplosionGfx( const vec3& in origin ) {
		}

		private array<TheNomad::Engine::Renderer::LocalEntity> m_LocalEnts;
		private TheNomad::Engine::Renderer::LocalEntity m_ActiveLocalEnts;
		private TheNomad::Engine::Renderer::LocalEntity@ m_FreeLocalEnts = null;

		// a single pre-allocated array of polys cuz angelscript won't let me use
		// stack allocated arrays
		private TheNomad::Engine::Renderer::PolyVert[] m_DrawVerts( 4 );

		private bool m_bAllowGfx = true;
	};
	
	GfxSystem@ GfxManager = null;
};