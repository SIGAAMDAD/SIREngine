#include "SGame/PlayerSystem/BloodStain.as"

namespace TheNomad::SGame {
    class GoreSystem : TheNomad::GameSystem::GameObject {
        GoreSystem() {
        }

        void OnInit() {
        }
        void OnShutdown() {
        }
        void OnLevelStart() {
            InitBloodStains();
        }
        void OnLevelEnd() {
            m_BloodStains.Clear();
        }
        void OnRunTic() {
        }

        void OnSave() const {
        }
        void OnLoad() {
        }
        void OnPlayerDeath( int ) {
        }
        void OnCheckpointPassed( uint ) {
        }
        const string& GetName() const {
            return "GoreSystem";
        }

        void OnRenderScene() {
			BloodStain@ stain;
			BloodStain@ next;

            if ( sgame_Blood.GetInt() == 1 && m_BloodStains.Count() == 0 ) {
                InitBloodStains();
            } else if ( sgame_Blood.GetInt() == 0 && m_BloodStains.Count() > 0 ) {
                m_BloodStains.Clear();
            }

			// walk the list backwards, so any new local entities generated
			// (trails, marks, etc) will be present this frame
			@stain = @m_ActiveBloodStains.m_Prev;
			for ( ; @stain !is @m_ActiveBloodStains; @stain = @next ) {
				// grab next now, so if the local entity is freed we
				// still have it
				@next = @stain.m_Prev;

			    stain.Draw();
			}
		}

		private void InitBloodStains() {
			const uint numGfx = 512;
			m_BloodStains.Resize( numGfx );

			@m_ActiveBloodStains.m_Next = @m_ActiveBloodStains;
			@m_ActiveBloodStains.m_Prev = @m_ActiveBloodStains;
			@m_FreeBloodStains = @m_BloodStains[0];

			for ( uint i = 0; i < m_BloodStains.Count() - 1; i++ ) {
				@m_BloodStains[i].m_Next = @m_BloodStains[i + 1];
			}
		}

		private BloodStain@ AllocBloodStain() {
			BloodStain@ stain;
			uint time;

			if ( @m_FreeBloodStains is null ) {
				// no free polys, so free the one at the end of the chain
				// remove the oldest active entity
				FreeBloodStain( @m_ActiveBloodStains.m_Prev );
			}

			@stain = @m_FreeBloodStains;
			@m_FreeBloodStains = @m_FreeBloodStains.m_Next;

			// link into active list
			@stain.m_Next = @m_ActiveBloodStains.m_Next;
			@stain.m_Prev = @m_ActiveBloodStains;
			@m_ActiveBloodStains.m_Next.m_Prev = @stain;
			@m_ActiveBloodStains.m_Next = @stain;

			return @stain;
		}

        private void FreeBloodStain( BloodStain@ stain ) {
			if ( @stain.m_Prev is null ) {
				GameError( "GoreSystem::FreeBloodStain: not active" );
			}

			// remove from doubly linked list
			@stain.m_Prev.m_Next = @stain.m_Next;
			@stain.m_Next.m_Prev = @stain.m_Prev;

			// the free list is only singly linked
			@stain.m_Next = @m_FreeBloodStains;
			@m_FreeBloodStains = @stain;
        }

        void AddBlood( const vec3& in origin ) {
            BloodStain@ stain;

            @stain = @AllocBloodStain();
//            @stain = @GfxManager.Bleed( vec3( origin.x, origin.y, origin.z + 0.25f ) );
        }

        private array<BloodStain> m_BloodStains;
		private BloodStain m_ActiveBloodStains;
		private BloodStain@ m_FreeBloodStains = null;
    };

    GoreSystem@ GoreManager = null;
};