#ifndef synthesis_TestPresenter_h
#define synthesis_TestPresenter_h

#include "burst/Presenter.h"
#include "burst/AssetLoader.h"
#include "burst/Gui.h"

#include "vkt/Device.h"
#include "vkt/ForwardDecl.h"
#include "vkt/Image.h"
#include "imgui.h"

#include <glm/glm.hpp>

#include <chrono>

namespace synthesis
{
    class TestPresenter
    :
        public burst::Presenter
    {
        public:
            TestPresenter( burst::PresentContext const & inContext, burst::ImageAsset inImage );
            ~TestPresenter();

            void Update( float inDelta );
            void Compute( vk::CommandBuffer inCommandBuffer ) const override;
            void Present( vk::CommandBuffer inCommandBuffer ) const override;

        private:
            void WriteImage( burst::ImageAsset inImage );
            void WriteMaskImage();
            void ClearPaintImage();

            void PaintDrawImage( const glm::vec2 inpos );

            burst::PresentContext const & mContext;
            burst::ImageAsset mInitialImage;

            struct ImageData
            {
                vkt::ImagePtr mImage;
                vk::Sampler mSampler;
                vk::ImageView mImageView;
            };
            ImageData CreateImageData( vk::Extent2D inExtent );
            void DestroyImageData( ImageData const & inImageData );
            void SaveImage();

            ImageData mDisplayImage;
            ImageData mMaskImage;
            ImageData mDrawImage;

            burst::gui::ImageInspector mDisplayInspector;

            vkt::DescriptorSetLayoutsPtr mComputeDescriptorSetLayout;
            vkt::ComputePipelinePtr mComputePipeline;

            vkt::DescriptorSetLayoutsPtr mGraphicsDescriptorSetLayout;
            vkt::GraphicsPipelinePtr mPipeline;

            std::chrono::microseconds mStartTime = std::chrono::duration_cast<std::chrono::microseconds>
            (
                std::chrono::system_clock::now().time_since_epoch()
            );

            struct PushConstants
            {
                float mTime;
                std::uint32_t mWidth;
                std::uint32_t mHeight;
                bool mEvenSort;
            };

            bool mCompute = false;
            bool mShowMask = true;
            bool mShowDraw = true;
            bool mBlendEdges = false;
            int mBlendRange = 100;
            int mPencilSize = 20;
            int mMaskChance = 100;
    };
}

#endif
