namespace TheNomad::Engine::Math {
    class Sphere {
    public:
    	Sphere( void );
    	Sphere( const vec3& in point );
    	Sphere( const vec3& in point, const float r );

    	float			operator[]( const int index ) const;
    	float &			operator[]( const int index );
    	Sphere		operator+( const vec3& in t ) const;				// returns tranlated sphere
    	Sphere &		operator+=( const vec3& in t );					// translate the sphere
    	Sphere		operator+( const Sphere& in s ) const;
    	Sphere &		operator+=( const Sphere& in s );

    	bool			Compare( const Sphere& in a ) const;						// exact compare, no epsilon
    	bool			Compare( const Sphere& in a, const float epsilon ) const;	// compare with epsilon
    	bool			operator==(	const Sphere& in a ) const;						// exact compare, no epsilon
    	bool			operator!=(	const Sphere& in a ) const;						// exact compare, no epsilon

    	void			Clear( void );									// inside out sphere
    	void			Zero( void );									// single point at origin
    	void			SetOrigin( const vec3& in o );					// set origin of sphere
    	void			SetRadius( const float r );						// set square radius

    	const vec3& in 	GetOrigin( void ) const;						// returns origin of sphere
    	float			GetRadius( void ) const;						// returns sphere radius
    	bool			IsCleared( void ) const;						// returns true if sphere is inside out

    	bool			AddPoint( const vec3& in p );					// add the point, returns true if the sphere expanded
    	bool			AddSphere( const Sphere& in s );				// add the sphere, returns true if the sphere expanded
    	Sphere		    Expand( const float d ) const;					// return bounds expanded in all directions with the given value
    	Sphere &		ExpandSelf( const float d );					// expand bounds in all directions with the given value
    	Sphere		    Translate( const vec3& in translation ) const;
    	Sphere &		TranslateSelf( const vec3& in translation );

    	float			PlaneDistance( const idPlane &plane ) const;
    	int				PlaneSide( const idPlane &plane, const float epsilon = ON_EPSILON ) const;

    	bool			ContainsPoint( const vec3& in p ) const;			// includes touching
    	bool			IntersectsSphere( const Sphere& in s ) const;	// includes touching
    	bool			LineIntersection( const vec3& in start, const vec3& in end ) const;
    					// intersection points are (start + dir * scale1) and (start + dir * scale2)
    	bool			RayIntersection( const vec3& in start, const vec3& in dir, float &scale1, float &scale2 ) const;

    					// Tight sphere for a point set.
    	void			FromPoints( const array<vec3>@ points );
    					// Most tight sphere for a translation.
    	void			FromPointTranslation( const vec3& in point, const vec3& in translation );
    	void			FromSphereTranslation( const Sphere& in sphere, const vec3& in start, const vec3& in translation );
    					// Most tight sphere for a rotation.
    	void			FromPointRotation( const vec3& in point, const idRotation &rotation );
    	void			FromSphereRotation( const Sphere& in sphere, const vec3& in start, const idRotation &rotation );

    	void			AxisProjection( const vec3& in dir, float &min, float &max ) const;

    private:
    	idVec3			origin;
    	float			radius;
    };

    extern Sphere	sphere_zero;

    ID_INLINE Sphere::Sphere( void ) {
    }

    ID_INLINE Sphere::Sphere( const vec3& in point ) {
    	origin = point;
    	radius = 0.0f;
    }

    ID_INLINE Sphere::Sphere( const vec3& in point, const float r ) {
    	origin = point;
    	radius = r;
    }

    ID_INLINE float Sphere::operator[]( const int index ) const {
    	return ((float *) &origin)[index];
    }

    ID_INLINE float &Sphere::operator[]( const int index ) {
    	return ((float *) &origin)[index];
    }

    ID_INLINE Sphere Sphere::operator+( const vec3& in t ) const {
    	return Sphere( origin + t, radius );
    }

    ID_INLINE Sphere &Sphere::operator+=( const vec3& in t ) {
    	origin += t;
    	return *this;
    }

    ID_INLINE bool Sphere::Compare( const Sphere& in a ) const {
    	return ( origin.Compare( a.origin ) && radius == a.radius );
    }

    ID_INLINE bool Sphere::Compare( const Sphere& in a, const float epsilon ) const {
    	return ( origin.Compare( a.origin, epsilon ) && idMath::Fabs( radius - a.radius ) <= epsilon );
    }

    ID_INLINE bool Sphere::operator==( const Sphere& in a ) const {
    	return Compare( a );
    }

    ID_INLINE bool Sphere::operator!=( const Sphere& in a ) const {
    	return !Compare( a );
    }

    ID_INLINE void Sphere::Clear( void ) {
    	origin.Zero();
    	radius = -1.0f;
    }

    ID_INLINE void Sphere::Zero( void ) {
    	origin.Zero();
    	radius = 0.0f;
    }

    ID_INLINE void Sphere::SetOrigin( const vec3& in o ) {
    	origin = o;
    }

    ID_INLINE void Sphere::SetRadius( const float r ) {
    	radius = r;
    }

    ID_INLINE const vec3& in Sphere::GetOrigin( void ) const {
    	return origin;
    }

    ID_INLINE float Sphere::GetRadius( void ) const {
    	return radius;
    }

    ID_INLINE bool Sphere::IsCleared( void ) const {
    	return ( radius < 0.0f );
    }

    ID_INLINE bool Sphere::AddPoint( const vec3& in p ) {
    	if ( radius < 0.0f ) {
    		origin = p;
    		radius = 0.0f;
    		return true;
    	}
    	else {
    		float r = ( p - origin ).LengthSqr();
    		if ( r > radius * radius ) {
    			r = idMath::Sqrt( r );
    			origin += ( p - origin ) * 0.5f * (1.0f - radius / r );
    			radius += 0.5f * ( r - radius );
    			return true;
    		}
    		return false;
    	}
    }

    ID_INLINE bool Sphere::AddSphere( const Sphere& in s ) {
    	if ( radius < 0.0f ) {
    		origin = s.origin;
    		radius = s.radius;
    		return true;
    	}
    	else {
    		float r = ( s.origin - origin ).LengthSqr();
    		if ( r > ( radius + s.radius ) * ( radius + s.radius ) ) {
    			r = idMath::Sqrt( r );
    			origin += ( s.origin - origin ) * 0.5f * (1.0f - radius / ( r + s.radius ) );
    			radius += 0.5f * ( ( r + s.radius ) - radius );
    			return true;
    		}
    		return false;
    	}
    }

    ID_INLINE Sphere Sphere::Expand( const float d ) const {
    	return Sphere( origin, radius + d );
    }

    ID_INLINE Sphere &Sphere::ExpandSelf( const float d ) {
    	radius += d;
    	return *this;
    }

    ID_INLINE Sphere Sphere::Translate( const vec3& in translation ) const {
    	return Sphere( origin + translation, radius );
    }

    ID_INLINE Sphere &Sphere::TranslateSelf( const vec3& in translation ) {
    	origin += translation;
    	return *this;
    }

    ID_INLINE bool Sphere::ContainsPoint( const vec3& in p ) const {
    	if ( ( p - origin ).LengthSqr() > radius * radius ) {
    		return false;
    	}
    	return true;
    }

    ID_INLINE bool Sphere::IntersectsSphere( const Sphere& in s ) const {
    	float r = s.radius + radius;
    	if ( ( s.origin - origin ).LengthSqr() > r * r ) {
    		return false;
    	}
    	return true;
    }

    ID_INLINE void Sphere::FromPointTranslation( const vec3& in point, const vec3& in translation ) {
    	origin = point + 0.5f * translation;
    	radius = idMath::Sqrt( 0.5f * translation.LengthSqr() );
    }

    ID_INLINE void Sphere::FromSphereTranslation( const Sphere& in sphere, const vec3& in start, const vec3& in translation ) {
    	origin = start + sphere.origin + 0.5f * translation;
    	radius = idMath::Sqrt( 0.5f * translation.LengthSqr() ) + sphere.radius;
    }

    ID_INLINE void Sphere::FromPointRotation( const vec3& in point, const idRotation &rotation ) {
    	idVec3 end = rotation * point;
    	origin = ( point + end ) * 0.5f;
    	radius = idMath::Sqrt( 0.5f * ( end - point ).LengthSqr() );
    }

    ID_INLINE void Sphere::FromSphereRotation( const Sphere& in sphere, const vec3& in start, const idRotation &rotation ) {
    	idVec3 end = rotation * sphere.origin;
    	origin = start + ( sphere.origin + end ) * 0.5f;
    	radius = idMath::Sqrt( 0.5f * ( end - sphere.origin ).LengthSqr() ) + sphere.radius;
    }

    ID_INLINE void Sphere::AxisProjection( const vec3& in dir, float &min, float &max ) const {
    	float d;
    	d = dir * origin;
    	min = d - radius;
    	max = d + radius;
    }
};