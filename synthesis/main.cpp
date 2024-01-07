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
            mEmptyPresenter(),
            mPresenter( std::make_shared< synthesis::TestPresenter >( GetPresentContext(), vk::Extent2D( inWidth, inHeight ) ) )
        {
        }

        virtual void Update( float inDelta ) override
        {
            ImGui::BeginMainMenuBar();
//            if (ImGui::MenuItem("Open Image.."))
//                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose Image", ".png", "/Users/angelo/Projects/nel.re/static/images/.");

            ImGui::EndMainMenuBar();

            if( mPresenter ) mPresenter->Update( inDelta );
        }

        virtual burst::Presenter & GetPresenter() const override
        {
            if( mPresenter ) return ( burst::Presenter & ) * mPresenter;
            return ( burst::Presenter & ) mEmptyPresenter;
        }

    private:
        std::shared_ptr< synthesis::TestPresenter > mPresenter;

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
    auto engine = ExampleEngine( 1600, 900, "Synthesis" );
    engine.Run();
}