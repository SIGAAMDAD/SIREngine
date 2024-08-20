namespace TheNomad::SGame {
    class BossManager {
        BossManager() {
        }

        MobObject@ SpawnBoss( MapCheckpoint@ trigger, BossInfo@ info ) {
            const vec3 origin = vec3( trigger.m_Origin.x, trigger.m_Origin.y, trigger.m_Origin.z );
            MobObject@ obj = cast<MobObject@>( @EntityManager.Spawn( TheNomad::GameSystem::EntityType::Mob, info.type, origin, 
                vec2( info.base.width, info.base.height ) ) );

            m_DataList.Add( @obj );

            return @obj;
        }

        private array<MobObject@> m_DataList;
    };
};