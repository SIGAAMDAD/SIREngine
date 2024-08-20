namespace TheNomad::SGame {
	//
	// HudOverlay: a hud element that doesn't use a spritesheet, most usually a fullscreen opaque image
	//
	class HudOverlay {
		HudOverlay() {
		}
		
		void Draw() const {
			ImGui::Begin( "##HudOverlay" + hShader, null, ImGui::MakeWindowFlags( ImGuiWindowFlags::NoResize | ImGuiWindowFlags::NoMove
				| ImGuiWindowFlags::NoCollapse | ImGuiWindowFlags::NoBackground | ImGuiWindowFlags::NoTitleBar
				| ImGuiWindowFlags::NoScrollbar ) );
			ImGui::SetWindowSize( size );
			ImGui::SetWindowPos( origin );
			ImGui::Image( hShader, origin, size );
			ImGui::End();
		}
		
		vec4 color = vec4( 1.0f ); // default to white
		vec2 origin = vec2( 0.0f, 0.0f );
		vec2 size = vec2( 0.0f, 0.0f );
		int hShader = FS_INVALID_HANDLE;
	};
};