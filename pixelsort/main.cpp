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
            mPresenter( GetPresentContext() )
        {
        }

        virtual void Update( float inDelta ) override
        {
            ImGui::BeginMainMenuBar();
            if (ImGui::MenuItem("Open Image.."))
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose Image", ".png", ".");

            // display
            if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
            {
                // action if OK
                if (ImGuiFileDialog::Instance()->IsOk())
                {
                    std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                    std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

                    // Do something with the image
                    auto theImageResource = burst::AssetLoader::LoadImage( filePathName );
                }

                // close
                ImGuiFileDialog::Instance()->Close();
            }
            ImGui::EndMainMenuBar();

            mPresenter.Update( inDelta );
        }

        virtual burst::Presenter & GetPresenter() const override
        {
            return ( burst::Presenter & ) mPresenter;
        }

    private:
        pixelsort::TestPresenter mPresenter;
};

int main()
{
    auto engine = ExampleEngine( 900, 900, "Pixel sort" );
    engine.Run();
}