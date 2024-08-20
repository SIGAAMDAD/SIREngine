namespace TheNomad::Engine::Renderer {
    class RenderEntity {
        RenderEntity() {
        }

		void Draw() const {
			AddEntityToScene( sheetNum, spriteId, renderfx, lightingOrigin, origin, frame, flags, color, shaderTexCoord, shaderTime,
				radius, rotation, scale );
		}

        // texturing
	    int sheetNum;  // sprite sheet index
    	int spriteId;  // sprite id

    	int renderfx;

	    vec3 lightingOrigin; // RF_LIGHTING_ORIGIN
	    vec3 origin;
	    uint64 frame;

	    uint32 flags;

	    // misc
	    uint32		color;
	    vec2		shaderTexCoord = 0.0f; // texture coordinates used by tcMod entity modifiers

    	// subtracted from refdef time to control effect start times
    	float		shaderTime = 0.0f;

    	// extra sprite information
    	float		radius = 0.0f;
    	float		rotation = 0.0f;
        float       scale = 0.0f;
    };
};