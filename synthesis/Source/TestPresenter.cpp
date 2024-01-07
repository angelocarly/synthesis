#include "synthesis/TestPresenter.h"

#include "vkt/Buffer.h"
#include "vkt/Device.h"
#include "vkt/ForwardDecl.h"
#include "vkt/GraphicsPipeline.h"
#include "vkt/RenderPass.h"
#include "vkt/Shader.h"
#include "vkt/ComputePipeline.h"
#include "imgui_impl_vulkan.h"

#include <imgui.h>
#include <ImGuiFileDialog.h>

#include <glm/glm.hpp>

#include <spdlog/spdlog.h>
#include <iostream>

synthesis::TestPresenter::TestPresenter( burst::PresentContext const & inContext, vk::Extent2D inExtent )
:
    mContext( inContext ),
    mComputeDescriptorSetLayout(
        vkt::DescriptorSetLayoutBuilder( mContext.mDevice )
        .AddLayoutBinding( 0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute )
        .AddLayoutBinding( 1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute )
        .Build( vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR )
    ),
    mGraphicsDescriptorSetLayout(
        vkt::DescriptorSetLayoutBuilder( mContext.mDevice )
        .AddLayoutBinding( 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment )
        .Build( vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR )
    ),
    mDisplayImage( CreateImageData( vk::Extent2D( inExtent.width, inExtent.height ) ) ),
    mDisplayInspector(
        "Display",
        vk::Extent2D( mDisplayImage.mImage->GetWidth(), mDisplayImage.mImage->GetHeight() ),
        {
            { mDisplayImage.mSampler, mDisplayImage.mImageView },
        },
        [this]( glm::vec2 inPos ){}
    )
{
    // Compute shader
    auto computeShader = vkt::Shader::CreateVkShaderModule( mContext.mDevice, "resources/shaders/Compute.comp" );

    std::vector< vk::PushConstantRange > pushConstants =
    {
        vk::PushConstantRange
        (
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof( PushConstants )
        )
    };

    mComputePipeline = vkt::ComputePipelineBuilder( mContext.mDevice )
        .SetComputeShader( computeShader )
        .SetDescriptorSetLayouts( mComputeDescriptorSetLayout )
        .SetPushConstants( pushConstants )
        .Build();

    mContext.mDevice.GetVkDevice().destroy( computeShader );

    // Present shader
    auto vertexShader = vkt::Shader::CreateVkShaderModule( mContext.mDevice, "resources/shaders/ScreenRect.vert" );
    auto fragmentShader = vkt::Shader::CreateVkShaderModule( mContext.mDevice, "resources/shaders/Sampler.frag" );

    mPipeline = vkt::GraphicsPipelineBuilder( mContext.mDevice )
        .SetDescriptorSetLayouts( mGraphicsDescriptorSetLayout )
        .SetVertexShader( vertexShader )
        .SetFragmentShader( fragmentShader )
        .SetRenderPass( mContext.mRenderPass->GetVkRenderPass() )
        .Build();

    mContext.mDevice.GetVkDevice().destroy( vertexShader );
    mContext.mDevice.GetVkDevice().destroy( fragmentShader );

}

synthesis::TestPresenter::~TestPresenter()
{
    mContext.mDevice.GetVkDevice().waitIdle();

    DestroyImageData( mDisplayImage );
}

void
synthesis::TestPresenter::Compute( vk::CommandBuffer inCommandBuffer ) const
{
    // Begin pipeline
    mComputePipeline->Bind( inCommandBuffer );

    // Push image descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mDisplayImage.mSampler,
        mDisplayImage.mImageView,
        vk::ImageLayout::eGeneral
    );

    auto theWriteDescriptorSet = vk::WriteDescriptorSet();
    theWriteDescriptorSet.setDstBinding( 0 );
    theWriteDescriptorSet.setDstArrayElement( 0 );
    theWriteDescriptorSet.setDescriptorType( vk::DescriptorType::eStorageImage );
    theWriteDescriptorSet.setDescriptorCount( 1 );
    theWriteDescriptorSet.setPImageInfo( & imageInfo );

    mComputePipeline->BindPushDescriptorSet( inCommandBuffer, theWriteDescriptorSet );

    // Push constants
    auto time = std::chrono::duration_cast<std::chrono::microseconds>
    (
        std::chrono::system_clock::now().time_since_epoch()
    );
    static bool sortEven = false;
    sortEven = !sortEven;
    PushConstants thePushConstants
    {
        ( float ) ( mStartTime - time ).count() / 1000.0f,
        std::uint32_t( mDisplayImage.mImage->GetWidth() ),
        std::uint32_t( mDisplayImage.mImage->GetHeight() ),
        mInput1
    };
    inCommandBuffer.pushConstants
    (
        mComputePipeline->GetVkPipelineLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof( PushConstants ),
        & thePushConstants
    );

    int groupsize = 16;
    inCommandBuffer.dispatch
    (
        ceil( mDisplayImage.mImage->GetWidth() / groupsize ),
        ceil( mDisplayImage.mImage->GetHeight() / groupsize ),
        1
    );
}

void
synthesis::TestPresenter::Present( vk::CommandBuffer inCommandBuffer ) const
{
    return;
    // Draw screen rect
    mPipeline->Bind( inCommandBuffer );

    // Push descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mDisplayImage.mSampler,
        mDisplayImage.mImageView,
        vk::ImageLayout::eGeneral
    );

    auto theWriteDescriptorSet = vk::WriteDescriptorSet();
    theWriteDescriptorSet.setDstBinding( 0 );
    theWriteDescriptorSet.setDstArrayElement( 0 );
    theWriteDescriptorSet.setDescriptorType( vk::DescriptorType::eCombinedImageSampler );
    theWriteDescriptorSet.setDescriptorCount( 1 );
    theWriteDescriptorSet.setPImageInfo( & imageInfo );

    mPipeline->BindPushDescriptorSet( inCommandBuffer, theWriteDescriptorSet );

    inCommandBuffer.draw( 3, 1, 0, 0 );
}

void
synthesis::TestPresenter::Update( float inDelta )
{

    ImGui::Begin( "Synthesis" );
    {
        // Fill an array of contiguous float values to plot
        // Tip: If your float aren't contiguous but part of a structure, you can pass a pointer to your first float
        // and the sizeof() of your structure in the "stride" parameter.
        static float values[90] = {};
        static int values_offset = 0;
        static double time = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() ).count() / 1000.0;
        double duration = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() ).count() / 1000.0 - time;
        {
            mInput1 = abs( sin( duration ) );
            values[values_offset] = mInput1;
            values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
        }

        ImGui::PlotLines("Lines", values, IM_ARRAYSIZE(values), values_offset, "", -1.0f, 1.0f, ImVec2(0, 80.0f));
    }

    ImGui::End();

    mDisplayInspector.Update( { true } );
}

synthesis::TestPresenter::ImageData
synthesis::TestPresenter::CreateImageData( vk::Extent2D inExtent )
{
    ImageData data;

    // Image
    data.mImage = vkt::ImageFactory( mContext.mDevice ).CreateImage
    (
        inExtent.width,
        inExtent.height,
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eDedicatedMemory,
        "Test Image",
        1
    );

    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        // Image layout
        data.mImage->MemoryBarrier
        (
            commandBuffer,
            vk::ImageLayout::eGeneral,
            vk::AccessFlagBits::eNone,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::DependencyFlagBits::eByRegion
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    // Sampler
    data.mSampler = mContext.mDevice.GetVkDevice().createSampler
    (
        vk::SamplerCreateInfo
        (
            vk::SamplerCreateFlags(),
            vk::Filter::eNearest,
            vk::Filter::eNearest,
            vk::SamplerMipmapMode::eLinear,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            0.0f,
            VK_FALSE,
            16.0f,
            VK_FALSE,
            vk::CompareOp::eAlways,
            0.0f,
            0.0f,
            vk::BorderColor::eIntOpaqueBlack,
            VK_FALSE
        )
    );

    // Image view
    data.mImageView = mContext.mDevice.GetVkDevice().createImageView
    (
        vk::ImageViewCreateInfo
        (
            vk::ImageViewCreateFlags(),
            data.mImage->GetVkImage(),
            vk::ImageViewType::e2D,
            vk::Format::eR8G8B8A8Unorm,
            vk::ComponentMapping(),
            vk::ImageSubresourceRange
            (
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1
            )
        )
    );

    return data;
}

void
synthesis::TestPresenter::DestroyImageData( const synthesis::TestPresenter::ImageData & inImageData )
{
    mContext.mDevice.GetVkDevice().destroy( inImageData.mImageView );
    mContext.mDevice.GetVkDevice().destroy( inImageData.mSampler );
}

void
synthesis::TestPresenter::SaveImage()
{
    // Create a buffer to store/read the image
    std::size_t byteSize = mDisplayImage.mImage->GetWidth() * mDisplayImage.mImage->GetHeight() * 4;
    auto imageBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        byteSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::AllocationCreateFlagBits::eHostAccessRandom,
        "MaskStagingBuffer"
    );

    // Copy the image
    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        commandBuffer.copyImageToBuffer
        (
            mDisplayImage.mImage->GetVkImage(),
            vk::ImageLayout::eGeneral,
            imageBuffer->GetVkBuffer(),
            vk::BufferImageCopy
            (
                0,
                mDisplayImage.mImage->GetWidth(),
                mDisplayImage.mImage->GetHeight(),
                vk::ImageSubresourceLayers
                (
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1
                ),
                vk::Offset3D( 0, 0, 0 ),
                vk::Extent3D( mDisplayImage.mImage->GetWidth(), mDisplayImage.mImage->GetHeight(), 1 )
            )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    burst::ImageAsset imageAsset = burst::ImageAsset();
    imageAsset.mWidth = mDisplayImage.mImage->GetWidth();
    imageAsset.mHeight = mDisplayImage.mImage->GetHeight();

    imageAsset.mPixels.resize( byteSize );

    std::uint8_t * bufferData = ( std::uint8_t * ) imageBuffer->MapMemory();
    std::memcpy( imageAsset.mPixels.data(), bufferData, byteSize );
    imageBuffer->UnMapMemory();

    burst::AssetLoader::SaveImage( imageAsset, "output.png" );
}



