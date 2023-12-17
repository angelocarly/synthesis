#include <iostream>

#include "burst/AssetLoader.h"
#include "burst/Engine.h"

#include "TestPresenter.h"

#include <imgui.h>
#include <ImGuiFileDialog.h>

class ExampleEngine
:
    public burst::Engine
{
    public:
        ExampleEngine( std::size_t inWidth, std::size_t inHeight, const char * inTitle )
        :
            burst::Engine( inWidth, inHeight, inTitle ),
            mEmptyPresenter()
        {
        }

        virtual void Update( float inDelta ) override
        {
            ImGui::BeginMainMenuBar();
            if (ImGui::MenuItem("Open Image.."))
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose Image", ".png", "/Users/angelo/Projects/nel.re/static/images/.");

            // display
            if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
            {
                // action if OK
                if (ImGuiFileDialog::Instance()->IsOk())
                {
                    std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                    std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

                    // Do something with the image
                    auto imageResource = burst::AssetLoader::LoadImage( filePathName );
                    mPresenter = std::make_shared< pixelsort::TestPresenter >( GetPresentContext(), imageResource );
                }

                // close
                ImGuiFileDialog::Instance()->Close();
            }
            ImGui::EndMainMenuBar();

            if( mPresenter ) mPresenter->Update( inDelta );
        }

        virtual burst::Presenter & GetPresenter() const override
        {
            if( mPresenter ) return ( burst::Presenter & ) * mPresenter;
            return ( burst::Presenter & ) mEmptyPresenter;
        }

    private:
        std::shared_ptr< pixelsort::TestPresenter > mPresenter;

        class EmptyPresenter
        :
            public burst::Presenter
        {
            public:
                void Compute( vk::CommandBuffer inCommandBuffer ) const
                {}
                void Present( vk::CommandBuffer inCommandBuffer ) const
                {}
        } mEmptyPresenter;
};

int main()
{
    auto engine = ExampleEngine( 900, 900, "Pixel sort" );
    engine.Run();
}