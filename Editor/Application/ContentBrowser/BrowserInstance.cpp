#include "ContentBrowser.h"
#include "BrowserInstance.h"
#include "../Project/ProjectManager.h"
#include <Engine/Core/ResourceDef.h>

namespace Valden::ContentBrowser {

CVar<float> ThumbnailSize(
	"ui.ThumbnailSize",
	172.0f,
	Cvar_Save,
	"Sets the ContentBrowser Thumbnail Size",
	CVG_USERINFO
);

CBrowserInstance::CBrowserInstance( const FileSystem::CFilePath& directory )
	: m_AssetTree( this )
{
	Init( directory );
}

void CBrowserInstance::Init( const FileSystem::CFilePath& directory )
{
	LoadDirectoryTree( directory );
	m_Favorites.reserve( 256 );
}

void CBrowserInstance::AddSubDir( FileView_t& Parent, const CString& dirName )
{
	SIRENGINE_LOG_LEVEL( ContentBrowser, ELogLevel::Verbose, "Adding AssetTree SubDir \"%s\" to ContentBrowser Cache...", 
		SIRENGINE_TEMP_VSTRING( "%s/%s", Parent.Path.c_str(), dirName.c_str() ) );

	FileView_t& subDir = m_AssetTree.AddSubDirectory( Parent, SIRENGINE_TEMP_VSTRING( "%s/%s", Parent.Path.c_str(), dirName.c_str() ) );

	AddCacheDir( Parent );
	AddCacheDir( subDir );
}

void CBrowserInstance::LoadDirectoryTree( const FileSystem::CFilePath& directory )
{
	SIRENGINE_LOG_LEVEL( ContentBrowser, ELogLevel::Verbose, "Adding AssetTree \"%s\" to ContentBrowser Cache...", directory.c_str() );

	const CString dirName = directory;
	m_AssetTree.GetBase().Path = dirName;
	m_AssetTree.GetBase().DirectoryName = directory.GetFileName();
}

void CBrowserInstance::AddCacheDir( const FileView_t& fileView )
{
	for ( const auto& it : fileView.Directories ) {
		for ( const auto& dir : it.Directories ) {
			AddCacheDir( dir );
		}
		for ( const auto& file : it.FileList ) {
			Application::FileInfo_t info;
			FileInfo_t fileData;

			if ( m_FileDatas.find( file ) != m_FileDatas.end() ) {
				continue; // already cached
			}

			Application::Get()->GetFileStats( file, &info );

			fileData.fileName = eastl::addressof( file );
			fileData.nDiskSize = info.nSize;

			// TODO: move this stuff over to the resource manager system
			const CString extension = FileSystem::CFilePath::GetExtension( file.c_str() );
			if ( extension == "wav" || extension == "ogg" || extension == "mp3" || extension == "aiff" || extension == "flac" ) {
				fileData.nType = EContentType::Audio;
			} else if ( extension == "tex2d" || extension == "png" || extension == "tga" || extension == "jpeg" || extension == "jpg"
				|| extension == "pcx" || extension == "bmp" || extension == "dds" )
			{
				fileData.nType = EContentType::Material;
			} else if ( extension == "json" ) {
				fileData.nType = EContentType::JsonFile;
			} else if ( extension == "csv" ) {
				fileData.nType = EContentType::CsvFile;
			} else if ( extension == "ini" ) {
				fileData.nType = EContentType::IniFile;
			} else if ( extension == "xml" ) {
				fileData.nType = EContentType::XmlFile;
			} else if ( extension == "as" ) {
				fileData.nType = EContentType::ScriptClass;
			} else {
				fileData.nType = EContentType::File;
			}

			SIRENGINE_LOG_LEVEL( ContentBrowser, ELogLevel::Verbose, "Added cached file data for \"%s\"", file.c_str() );
			m_FileDatas.try_emplace( file, fileData );
		}
//		g_pFileSystem->AddCacheDirectory( it.Path.c_str() );
	}
}

void CBrowserInstance::Draw( void )
{
	const float cellSize = ThumbnailSize.GetValue();
	const float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = 0, columnIndex;

	ImGui::Begin( "Content Browser", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

	if ( ImGui::Button( ICON_FA_PLUS "Add" ) ) {

	}
	ImGui::SameLine();
	if ( ImGui::Button( ICON_FA_FILE_IMPORT "Import" ) ) {

	}
	ImGui::SameLine();

	bool bPushedColor = false;
	FileView_t *history = m_HistoryBuffer.GetBack();

	if ( !m_HistoryBuffer.CanGoBack() ) {
		bPushedColor = true;
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.90f, 0.90f, 0.90f, 1.0f ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.90f, 0.90f, 0.90f, 1.0f ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.90f, 0.90f, 0.90f, 1.0f ) );
	}
	if ( ImGui::Button( ICON_FA_ARROW_CIRCLE_LEFT ) && history ) {
		m_HistoryBuffer.GoBack();
		m_AssetTree.SetSelectedDir( history );
	}
	if ( bPushedColor ) {
		ImGui::PopStyleColor( 3 );
	} else if ( m_HistoryBuffer.CanGoBack() ) {
		ITEM_TOOLTIP_STRING( "Go back to %s",
			strstr( m_HistoryBuffer.GetBack()->Path.c_str(),
			SIRENGINE_TEMP_VSTRING( "/%s", CProjectManager::Get()->GetProject()->GetName().c_str() ) ) );
	}
	ImGui::SameLine();

	bPushedColor = false;
	history = m_HistoryBuffer.GetForward();
	if ( !m_HistoryBuffer.CanGoForward() ) {
		bPushedColor = true;
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.90f, 0.90f, 0.90f, 1.0f ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.90f, 0.90f, 0.90f, 1.0f ) );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.90f, 0.90f, 0.90f, 1.0f ) );
	}
	if ( ImGui::Button( ICON_FA_ARROW_CIRCLE_RIGHT ) && history ) {
		m_HistoryBuffer.GoForward();
		m_AssetTree.SetSelectedDir( history );
	}
	if ( bPushedColor ) {
		ImGui::PopStyleColor( 3 );
	} else if ( m_HistoryBuffer.CanGoForward() ) {
		ITEM_TOOLTIP_STRING( "Go to %s",
			strstr( m_HistoryBuffer.GetForward()->Path.c_str(),
			SIRENGINE_TEMP_VSTRING( "/%s", CProjectManager::Get()->GetProject()->GetName().c_str() ) ) );
	}
	ImGui::SameLine();
	if ( ImGui::Button( ICON_FA_FOLDER ) ) {

	}

	ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );

	if ( ImGui::BeginTable( "##ContentBrowserTable", 2, ImGuiTableFlags_Resizable ) ) {
		ImGui::TableNextColumn();

		if ( ImGui::BeginTable( "##ContentBrowserSidePanel", 1, ImGuiTableFlags_ScrollY ) ) {
			ImGui::TableNextColumn();
			if ( ImGui::CollapsingHeader( "Favorites##ContentBrowserSidePanelFavorites" ) ) {
				for ( auto& it : m_Favorites ) {
					const char *pName = strrchr( it.pFavorite->c_str(), '/' ) + 1;
					if ( ImGui::MenuItem( pName ) ) {
					}
				}
			}
			if ( ImGui::CollapsingHeader( "Project##ContentBrowserSidePanelProject" ) ) {
				m_AssetTree.Draw();
			}
			ImGui::EndTable();
		}

		ImGui::TableNextColumn();

		columnCount = (int)( panelWidth / cellSize );
		if ( columnCount < 1 ) {
			columnCount = 1;
		}

		ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2( 1.0f, 1.0f ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
		if ( ImGui::BeginTable( "##ContentBrowserMainPanelTable", 3, ImGuiTableFlags_ScrollY ) ) {
			columnIndex = 0;

			FileView_t *pDirectory = m_AssetTree.GetSelectedDir();

			if ( pDirectory ) {
				for ( auto& file : pDirectory->FileList ) {
					ImGui::PushID( file.c_str() );

					CMaterial *pIcon;
					const char *pFilePath = strstr( file.c_str(), CProjectManager::Get()->GetProject()->GetName().c_str() ) - 1;
					const auto& fileInfo = m_FileDatas.find( file );

					EContentType nType = EContentType::File;
					uint64_t nDiskSize = 0;

					if ( fileInfo != m_FileDatas.cend() ) {
						nType = fileInfo->second.nType;
						nDiskSize = fileInfo->second.nDiskSize;
					}

					switch ( nType ) {
					case EContentType::File:
					default:
						pIcon = Cast<CMaterial>( CContentBrowser::Get().m_pFileIcon );
						break;
					case EContentType::IniFile:
						pIcon = Cast<CMaterial>( CContentBrowser::Get().m_pIniFileIcon );
						break;
					case EContentType::CsvFile:
						pIcon = Cast<CMaterial>( CContentBrowser::Get().m_pCsvFileIcon );
						break;
					case EContentType::JsonFile:
						pIcon = Cast<CMaterial>( CContentBrowser::Get().m_pJsonFileIcon );
						break;
					case EContentType::XmlFile:
						pIcon = Cast<CMaterial>( CContentBrowser::Get().m_pXmlFileIcon );
						break;
					case EContentType::Audio:
						pIcon = Cast<CMaterial>( CContentBrowser::Get().m_pAudioIcon );
						break;
					case EContentType::Material:
						pIcon = Cast<CMaterial>( CContentBrowser::Get().m_pMaterialIcon );
						break;
					};

					if ( ImGui::ImageButtonEx( ImGui::GetID( pIcon ), IMGUI_TEXTURE_ID( pIcon ),
						{ ThumbnailSize.GetValue(), ThumbnailSize.GetValue() }, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 },
						ImGuiButtonFlags_PressedOnDoubleClick ) )
					{
					}
					if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
						ImGui::OpenPopup( "File Utilities##ContentBrowserFileUtilities" );
						m_MousePopupPosition = ImGui::GetMousePos();
						m_pPopupFile = &file;
					}
					if ( ImGui::BeginPopup( "File Utilities##ContentBrowserFileUtilities" ) ) {
						ImGui::SetWindowPos( m_MousePopupPosition );
						ImGui::SetWindowFocus();

						ImGui::SeparatorText( "Common" );
						ImGui::Indent( 0.5f );
						if ( ImGui::MenuItem( ICON_FA_EDIT "Edit" ) ) {
						}
						ITEM_TOOLTIP_STRING( "Open selected item(s) for editing" );
						ImGui::Unindent( 0.5f );

						ImGui::EndPopup();
					}
					if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNone ) ) {
						if ( ImGui::BeginTooltip() ) {
							ImGui::SetWindowFontScale( ImGui::GetFont()->Scale * 1.90f );
							ImGui::TextUnformatted( pFilePath );
							ImGui::SetWindowFontScale( ImGui::GetFont()->Scale * 1.25f );

							ImGui::Separator();

							ImGui::Text( "Path: %s", pFilePath );
							ImGui::Text( "Resource Filepath Length: %lu/%lu", strlen( pFilePath ), MAX_RESOURCE_PATH );
							ImGui::TextUnformatted( "Content Type: " );
							ImGui::SameLine();
							switch ( nType ) {
							case EContentType::JsonFile:
								ImGui::TextUnformatted( "JSon Data" );
								break;
							case EContentType::XmlFile:
								ImGui::TextUnformatted( "XML Data" );
								break;
							case EContentType::IniFile:
								ImGui::TextUnformatted( "Ini Data" );
								break;
							case EContentType::CsvFile:
								ImGui::TextUnformatted( "CSV Data" );
								break;
							case EContentType::Audio:
								ImGui::TextUnformatted( "Audio" );
								break;
							case EContentType::ScriptClass:
								ImGui::TextUnformatted( "Script Class" );
								break;
							case EContentType::File:
								ImGui::TextUnformatted( "Misc. File" );
								break;
							case EContentType::Animation:
								ImGui::TextUnformatted( "Animation" );
								break;
							};
							ImGui::Text( "Disk Size: %lu", nDiskSize );
							ImGui::EndTooltip();
						}
					}
					ImGui::TextWrapped( "%s", pFilePath );
					ImGui::PopID();
					ImGui::TableNextColumn();
				}
				for ( auto& directory : pDirectory->Directories ) {
					ImGui::PushID( directory.Path.c_str() );
					if ( ImGui::ImageButtonEx( ImGui::GetID( CContentBrowser::Get().m_pDirectoryIcon ),
						IMGUI_TEXTURE_ID( CContentBrowser::Get().m_pDirectoryIcon ),
						{ ThumbnailSize.GetValue(), ThumbnailSize.GetValue() }, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 },
						ImGuiButtonFlags_PressedOnDoubleClick ) )
					{
						SIRENGINE_LOG( "Setting directory to \"%s\"", directory.Path.c_str() );
						m_AssetTree.SetSelectedDir( &directory );
						m_HistoryBuffer.AddHistoryData( &directory );
					}
					if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
						ImGui::OpenPopup( "Folder Utilities##ContentBrowserDirectoryUtilities" );
						m_MousePopupPosition = ImGui::GetMousePos();
						m_pPopupFile = &directory.Path;
					}
					if ( ImGui::BeginDragDropSource() ) {
						ImGui::EndDragDropSource();
					}
					if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNone ) ) {
						if ( ImGui::BeginTooltip() ) {
							ImGui::SetWindowFontScale( ImGui::GetFont()->Scale * 1.90f );
							ImGui::TextUnformatted( directory.DirectoryName.c_str() );
							ImGui::SameLine();
							ImGui::TextUnformatted( " (Folder)" );
							ImGui::SetWindowFontScale( ImGui::GetFont()->Scale * 1.25f );
							
							ImGui::Separator();

							ImGui::Text( "Path: %s", strstr( directory.Path.c_str(),
								CProjectManager::Get()->GetProject()->GetName().c_str() ) - 1 );

							ImGui::EndTooltip();
						}
					}
					if ( ImGui::BeginPopup( "Folder Utilities##ContentBrowserDirectoryUtilities" ) ) {
						ImGui::SetWindowPos( m_MousePopupPosition );
						ImGui::SetWindowFocus();

						ImGui::Indent( 0.5f );
						if ( ImGui::MenuItem( ICON_FA_FOLDER_PLUS "New Folder##NewFolder_ContentBrowserDirectoryUtilities" ) ) {
							directory.Directories.emplace_back();
							directory.Directories.back().DirectoryName = "New Folder";
							directory.Directories.back().Path = SIRENGINE_TEMP_VSTRING( "%s/New Folder", directory.Path.c_str() );
						}
						ITEM_TOOLTIP_STRING( "Creates a new folder in selected folder" );
						if ( ImGui::MenuItem( ICON_FA_FOLDER_OPEN
							"Show Folder in New Content Browser##ShowFolder_ContentBrowserDirectoryUtilities" ) )
						{
											
						}
						ITEM_TOOLTIP_STRING( "Opens a new Content Browser window set at the selected folder" );
						ImGui::Unindent( 0.5f );
				
						ImGui::SeparatorText( "Folder Actions" );
						ImGui::Indent( 0.5f );
						if ( ImGui::MenuItem( ICON_FA_STAR "Add To Favorites##AddToFav_ContentBrowserFileUtilitiesFavorite" ) ) {
							m_Favorites.push_back( { m_pPopupFile,
								m_AssetTree.GetDirectory( *m_pPopupFile ) != NULL } );
							ImGui::CloseCurrentPopup();
						}
						ITEM_TOOLTIP_STRING( "Flags selected folder as favorite" );
						if ( ImGui::MenuItem( ICON_FA_EDIT "Rename##Rename_ContentBrowserDirectoryUtilities" ) ) {
						}
						ITEM_TOOLTIP_STRING( "Rename selected folder" );
						ImGui::Unindent( 0.5f );

						ImGui::EndPopup();
					}
					ImGui::PopID();
					ImGui::TableNextColumn();
				}
			}
			ImGui::EndTable();
		}

		ImGui::EndTable();
	}

	ImGui::PopStyleVar( 2 );
	ImGui::PopStyleColor( 1 );
	ImGui::End();
}

};