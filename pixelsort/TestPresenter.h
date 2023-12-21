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
            void WriteImage( burst::ImageAsset inImage );
            void WriteMaskImage();

            burst::PresentContext const & mContext;
            burst::ImageAsset mInitialImage;

            vkt::ImagePtr mImage;
            vk::Sampler mSampler;
            vk::ImageView mImageView;
            VkDescriptorSet mImGuiImageDescriptorSet;

            vkt::ImagePtr mMaskImage;
            vk::Sampler mMaskSampler;
            vk::ImageView mMaskImageView;

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
            bool mShowMask = false;
            int mMaskWidth = 100;
            int mMaskHeight = 100;
    };
}

#endif
