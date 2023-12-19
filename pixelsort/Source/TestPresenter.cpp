#include "pixelsort/TestPresenter.h"

#include "vkt/Buffer.h"
#include "vkt/Device.h"
#include "vkt/ForwardDecl.h"
#include "vkt/GraphicsPipeline.h"
#include "vkt/RenderPass.h"
#include "vkt/Shader.h"
#include "vkt/ComputePipeline.h"

#include <imgui.h>
#include <ImGuiFileDialog.h>

#include <glm/glm.hpp>

pixelsort::TestPresenter::TestPresenter( burst::PresentContext const & inContext, burst::ImageAsset inImage )
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
    )
{
    InitializeMaskImage( vk::Extent2D( inImage.mWidth, inImage.mHeight ) );

    // Buffer
    mPointBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        sizeof( glm::vec4 ) * inImage.mWidth * inImage.mHeight,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        "PointBuffer"
    );

    // Store image data
    auto bufferData = ( std::uint32_t * ) mPointBuffer->MapMemory();
    {
        std::memcpy( bufferData, inImage.mPixels.data(), sizeof( glm::vec4 ) * inImage.mWidth * inImage.mHeight );
//        std::memcpy( bufferData, inImage.mPixels.data(), sizeof( glm::vec4 ) * inImage.mWidth * 800  );
    }
    mPointBuffer->UnMapMemory();

    // Image
    mImage = vkt::ImageFactory( mContext.mDevice ).CreateImage
    (
        inImage.mWidth,
        inImage.mHeight,
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vma::AllocationCreateFlagBits::eDedicatedMemory,
        "Test Image",
        1
    );

    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        // Image layout
        mImage->MemoryBarrier
        (
            commandBuffer,
            vk::ImageLayout::eGeneral,
            vk::AccessFlagBits::eNone,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::DependencyFlagBits::eByRegion
        );

        // Copy the staging buffer into the image
        commandBuffer.copyBufferToImage
        (
            mPointBuffer->GetVkBuffer(),
            mImage->GetVkImage(),
            vk::ImageLayout::eGeneral,
            vk::BufferImageCopy
            (
                0,
                0,
                0,
                vk::ImageSubresourceLayers
                (
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1
                ),
                vk::Offset3D( 0, 0, 0 ),
                vk::Extent3D( inImage.mWidth, inImage.mHeight, 1 )
            )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    // Sampler
    mSampler = mContext.mDevice.GetVkDevice().createSampler
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
    mImageView = mContext.mDevice.GetVkDevice().createImageView
    (
        vk::ImageViewCreateInfo
        (
            vk::ImageViewCreateFlags(),
            mImage->GetVkImage(),
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

pixelsort::TestPresenter::~TestPresenter()
{
    mContext.mDevice.GetVkDevice().waitIdle();

    // TODO: Destroy image handles automatically
    mContext.mDevice.GetVkDevice().destroy( mImageView );
    mContext.mDevice.GetVkDevice().destroy( mSampler );
}

void
pixelsort::TestPresenter::Compute( vk::CommandBuffer inCommandBuffer ) const
{
    bool compute = false;
    ImGui::Begin("Pixelsort");
    {
        ImGui::Button( "Compute" );
        compute = ImGui::IsItemActive();
    }
    ImGui::End();

    if( !compute ) return;

    // Begin pipeline
    mComputePipeline->Bind( inCommandBuffer );

    // Push image descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mSampler,
        mImageView,
        vk::ImageLayout::eGeneral
    );

    auto theWriteDescriptorSet = vk::WriteDescriptorSet();
    theWriteDescriptorSet.setDstBinding( 0 );
    theWriteDescriptorSet.setDstArrayElement( 0 );
    theWriteDescriptorSet.setDescriptorType( vk::DescriptorType::eStorageImage );
    theWriteDescriptorSet.setDescriptorCount( 1 );
    theWriteDescriptorSet.setPImageInfo( & imageInfo );

    mComputePipeline->BindPushDescriptorSet( inCommandBuffer, theWriteDescriptorSet );

    // Push image mask descriptor set
    auto maskImageInfo = vk::DescriptorImageInfo
    (
        mMaskSampler,
        mMaskImageView,
        vk::ImageLayout::eGeneral
    );

    auto writeDescriptorSet = vk::WriteDescriptorSet();
    writeDescriptorSet.setDstBinding( 1 );
    writeDescriptorSet.setDstArrayElement( 0 );
    writeDescriptorSet.setDescriptorType( vk::DescriptorType::eStorageImage );
    writeDescriptorSet.setDescriptorCount( 1 );
    writeDescriptorSet.setPImageInfo( & maskImageInfo );

    mComputePipeline->BindPushDescriptorSet( inCommandBuffer, writeDescriptorSet );

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
        std::uint32_t( mImage->GetWidth() ),
        std::uint32_t( mImage->GetHeight() ),
        sortEven
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
    inCommandBuffer.dispatch( ceil( mImage->GetWidth() / groupsize ), 1, 1 );
}

void
pixelsort::TestPresenter::Present( vk::CommandBuffer inCommandBuffer ) const
{
    // Draw screen rect
    mPipeline->Bind( inCommandBuffer );

    // Push descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mSampler,
        mImageView,
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
pixelsort::TestPresenter::Update( float inDelta )
{
}

void pixelsort::TestPresenter::InitializeMaskImage( vk::Extent2D inExtent )
{
    // Buffer
    auto stagingBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        sizeof( glm::vec4 ) * inExtent.width * inExtent.height,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        "MaskStagingBuffer"
    );

    // Store image data
    auto bufferData = ( std::uint8_t * ) stagingBuffer->MapMemory();
    {
        std::size_t i = 0;
        for( std::size_t y = 0; y < inExtent.height; y++ )
        {
            for( std::size_t x = 0; x < inExtent.width; x++ )
            {
                bufferData[ i + 0 ] = 255;
                bufferData[ i + 1 ] = 0;
                bufferData[ i + 2 ] = 0;
                bufferData[ i + 3 ] = 255;
                i += 4;
            }
        }
    }
    stagingBuffer->UnMapMemory();

    // Image
    mMaskImage = vkt::ImageFactory( mContext.mDevice ).CreateImage
    (
        inExtent.width,
        inExtent.height,
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vma::AllocationCreateFlagBits::eDedicatedMemory,
        "Test Image",
        1
    );

    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        // Image layout
        mMaskImage->MemoryBarrier
        (
            commandBuffer,
            vk::ImageLayout::eGeneral,
            vk::AccessFlagBits::eNone,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::DependencyFlagBits::eByRegion
        );

        // Clear the mask image
        commandBuffer.copyBufferToImage
        (
            stagingBuffer->GetVkBuffer(),
            mMaskImage->GetVkImage(),
            vk::ImageLayout::eGeneral,
            vk::BufferImageCopy
            (
                0,
                0,
                0,
                vk::ImageSubresourceLayers
                (
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1
                ),
                vk::Offset3D( 0, 0, 0 ),
                vk::Extent3D( inExtent.width, inExtent.height, 1 )
            )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    // Sampler
    mMaskSampler = mContext.mDevice.GetVkDevice().createSampler
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
    mMaskImageView = mContext.mDevice.GetVkDevice().createImageView
    (
        vk::ImageViewCreateInfo
        (
            vk::ImageViewCreateFlags(),
            mMaskImage->GetVkImage(),
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

}


