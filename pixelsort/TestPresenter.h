#ifndef pixelsort_TestPresenter_h
#define pixelsort_TestPresenter_h

#include "burst/Presenter.h"

#include "vkt/Device.h"
#include "vkt/ForwardDecl.h"
#include "vkt/Image.h"
#include "burst/AssetLoader.h"

#include <chrono>

namespace pixelsort
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
            void InitializeMaskImage( vk::Extent2D inExtent );

            burst::PresentContext const & mContext;

            vkt::ImagePtr mImage;
            vk::Sampler mSampler;
            vk::ImageView mImageView;

            vkt::ImagePtr mMaskImage;
            vk::Sampler mMaskSampler;
            vk::ImageView mMaskImageView;

            vkt::BufferPtr mPointBuffer;

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
    };
}

#endif
