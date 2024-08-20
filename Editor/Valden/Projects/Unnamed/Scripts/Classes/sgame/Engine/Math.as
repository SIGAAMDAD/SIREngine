namespace TheNomad::Engine::Math {
    const uint NaNMask = ( 255<<23 );
    
    bool IsNaN( int x ) {
        return ( int( x ) & NaNMask == NaNMask );
    }

    void CrossProduct( const vec3& in v1, const vec3& in v2, vec3& out cross ) {
		cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
		cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
		cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
	}

	void VectorScale( const vec3& in src, float scale, vec3& out dst ) {
		dst[0] = src[0] * scale;
		dst[1] = src[1] * scale;
		dst[2] = src[2] * scale;
	}

	float DotProduct( const vec3& in a, const vec3& in b ) {
		return ( a[0]*b[0] + a[1]*b[1] + a[2]*b[2] );
	}

	float VectorLength( const vec3& in v ) {
		return sqrt( DotProduct( v, v ) );
	}

	float VectorNormalize( vec3& out v ) {
		float length, ilength;

		length = DotProduct( v, v );

		if ( length > 0.0f ) {
			ilength = 1.0f / float( sqrt( length ) );
			/* sqrt(length) = length * (1 / sqrt(length)) */
			length *= ilength;
            v *= ilength;
		}
		
		return length;
	}

    /*
    =================
    RadiusFromBounds
    =================
    */
    float RadiusFromBounds( const TheNomad::GameSystem::BBox& in bounds ) {
    	int		i;
    	vec3	corner;
    	float	a, b;

    	for ( i = 0; i < 3; i++ ) {
    		a = abs( bounds.m_Mins[i] );
    		b = abs( bounds.m_Maxs[i] );
    		corner[i] = a > b ? a : b;
    	}

    	return VectorLength( corner );
    }
};