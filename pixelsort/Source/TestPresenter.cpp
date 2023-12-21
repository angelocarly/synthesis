#include "pixelsort/TestPresenter.h"

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
    ),
    mInitialImage( inImage )
{
    InitializeMaskImage( vk::Extent2D( inImage.mWidth, inImage.mHeight ) );

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
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

    WriteImage( inImage );

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

    // ImGui
    mImGuiImageDescriptorSet = ImGui_ImplVulkan_AddTexture( mSampler, mImageView, VK_IMAGE_LAYOUT_GENERAL );

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

    ImGui_ImplVulkan_RemoveTexture( mImGuiImageDescriptorSet );

    mContext.mDevice.GetVkDevice().destroy( mImageView );
    mContext.mDevice.GetVkDevice().destroy( mSampler );

    mContext.mDevice.GetVkDevice().destroy( mMaskImageView );
    mContext.mDevice.GetVkDevice().destroy( mMaskSampler );
}

void
pixelsort::TestPresenter::Compute( vk::CommandBuffer inCommandBuffer ) const
{
    return;
    if( !mCompute ) return;

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
    return;
    // Draw screen rect
    mPipeline->Bind( inCommandBuffer );

    // Push descriptor set
    auto imageInfo = vk::DescriptorImageInfo
    (
        mSampler,
        mImageView,
        vk::ImageLayout::eGeneral
    );
    if( mShowMask )
    {
        imageInfo = vk::DescriptorImageInfo
        (
            mMaskSampler,
            mMaskImageView,
            vk::ImageLayout::eGeneral
        );
    }

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
    ImGui::Begin("Pixelsort");
    {
        ImGui::Button( "Compute" );
        mCompute = ImGui::IsItemActive();
        if( ImGui::Button( "Reset" ) )
        {
            WriteImage( mInitialImage );
        }

        if( ImGui::Button( "Store mask") )
        {
            WriteMaskImage();
        }

        ImGui::Checkbox( "Show mask", & mShowMask );
        ImGui::SliderInt( "Mask Width", & mMaskWidth, 1, 800);
        ImGui::SliderInt( "Mask Height", & mMaskHeight, 1, 800);
    }
    ImGui::End();

    if (ImGui::Begin("Image inspector")) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Get the current ImGui cursor position
        ImVec2 canvas_p0   = ImGui::GetCursorScreenPos();    // ImDrawList API uses screen coordinates!
        ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // Resize canvas to what's available

        // guarantee a minimum canvas size
        canvas_size.x = std::max(canvas_size.x, 256.0f);
        canvas_size.y = std::max(canvas_size.y, 250.0f);

        ImVec2 canvas_p1 = ImVec2{canvas_p0.x + canvas_size.x, canvas_p0.y + canvas_size.y};

        ImGui::InvisibleButton("##canvas", canvas_size,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
            ImGuiButtonFlags_MouseButtonMiddle);

        const bool canvas_hovered = ImGui::IsItemHovered(); // Hovered
        const bool canvas_active  = ImGui::IsItemActive();  // Held

        // const ImVec2 canvas_p0 = ImGui::GetItemRectMin(); // alternatively we can get the rectangle like this
        // const ImVec2 canvas_p1 = ImGui::GetItemRectMax();

        // Draw border and background color
        ImGuiIO& io = ImGui::GetIO();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

        static ImVec2 scale = ImVec2( 1, 1 );
        static ImVec2 translate = ImVec2( 0, 0 );

        // TODO: shift or ctrl to slow zoom movement
        // if (canvas_active) {
        float zoom_rate   = 0.1f;
        float zoom_mouse  = io.MouseWheel * zoom_rate; //-0.1f 0.0f 0.1f
        float hzoom_mouse = zoom_mouse * 0.5f;
        float zoom_delta  = zoom_mouse * scale.x; // each step grows or shrinks image by 10%

        ImVec2 old_scale = scale;
        // on screen (top left of image)
        ImVec2 old_origin = {canvas_p0.x + translate.x,
                             canvas_p0.y + translate.y};
        // on screen (bottom right of image)
        ImVec2 old_p1 = {old_origin.x + (mImage->GetWidth() * scale.x),
                         old_origin.y + (mImage->GetHeight() * scale.y)};
        // on screen (center of what we get to see), when adjusting scale this doesn't change!
        ImVec2 old_and_new_canvas_center = {
            canvas_p0.x + canvas_size.x * 0.5f, canvas_p0.y + canvas_size.y * 0.5f};
        // in image coordinate offset of the center
        ImVec2 image_center = {
            old_and_new_canvas_center.x - old_origin.x, old_and_new_canvas_center.y - old_origin.y};

        ImVec2 old_uv_image_center = {
            image_center.x / (mImage->GetWidth() * scale.x),
            image_center.y / (mImage->GetHeight() * scale.y)};

        scale.x += zoom_delta;
        scale.y += zoom_delta;

        // 2.0f -> 2x zoom in
        // 1.0f -> normal
        // 0.5f -> 2x zoom out
        // TODO: clamp based on image size, do we go pixel level?
        scale.x = std::clamp(scale.x, 0.01f, 100.0f);
        scale.y = std::clamp(scale.y, 0.01f, 100.0f);

        // on screen new target center
        ImVec2 new_image_center = {(mImage->GetWidth() * scale.x *
                                    old_uv_image_center.x),
                                   (mImage->GetHeight() * scale.y *
                                    old_uv_image_center.y)};

        // readjust to center
        translate.x -= new_image_center.x - image_center.x;
        translate.y -= new_image_center.y - image_center.y;

        // 0 out second parameter if a context menu is open
        if (canvas_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
            translate.x += ImGui::GetIO().MouseDelta.x;
            translate.y += ImGui::GetIO().MouseDelta.y;
        }

        const ImVec2 origin(canvas_p0.x + translate.x,
            canvas_p0.y + translate.y); // Lock scrolled origin

        // we need to control the rectangle we're going to draw and the uv coordinates
        const ImVec2 image_p1 = {origin.x + (scale.x * mImage->GetWidth()),
                                 origin.y + (scale.x * mImage->GetHeight())};

        auto imio = ImGui::GetIO();
        const ImVec2 mouse_pos_in_canvas(imio.MousePos.x - origin.x, imio.MousePos.y - origin.y);

        draw_list->PushClipRect(ImVec2{canvas_p0.x + 2.0f, canvas_p0.y + 2.0f},
            ImVec2{canvas_p1.x - 2.0f, canvas_p1.y - 2.0f}, true);
        // draw things
        draw_list->AddImage(mImGuiImageDescriptorSet, origin, image_p1);
        // draw things
        draw_list->PopClipRect();
    }
    ImGui::End();
}

void
pixelsort::TestPresenter::InitializeMaskImage( vk::Extent2D inExtent )
{
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

void
pixelsort::TestPresenter::WriteImage( burst::ImageAsset inImage )
{
    // Buffer
    auto stagingBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        sizeof( glm::vec4 ) * inImage.mWidth * inImage.mHeight,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        "PointBuffer"
    );

    // Store image data
    auto bufferData = ( std::uint32_t * ) stagingBuffer->MapMemory();
    {
        std::memcpy( bufferData, inImage.mPixels.data(), sizeof( std::uint8_t ) * 4  * inImage.mWidth * inImage.mHeight );
    }
    stagingBuffer->UnMapMemory();


    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
        // Copy the staging buffer into the image
        commandBuffer.copyBufferToImage
        (
            stagingBuffer->GetVkBuffer(),
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
}

void
pixelsort::TestPresenter::WriteMaskImage()
{
    // Buffer
    auto stagingBuffer = vkt::BufferFactory( mContext.mDevice ).CreateBuffer
    (
        sizeof( glm::vec4 ) * mMaskImage->GetWidth() * mMaskImage->GetHeight(),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        "MaskStagingBuffer"
    );

    // Store image data
    auto bufferData = ( std::uint8_t * ) stagingBuffer->MapMemory();
    {
        std::size_t i = 0;
        for( std::size_t y = 0; y < mMaskImage->GetWidth(); y++ )
        {
            for( std::size_t x = 0; x < mMaskImage->GetWidth(); x++ )
            {
                bufferData[ i + 0 ] = 0;
                bufferData[ i + 1 ] = 0;
                bufferData[ i + 2 ] = 0;
                bufferData[ i + 3 ] = 255;
                i += 4;
            }
        }

        for( std::size_t i = 0; i < 200; i++ )
        {
            std::size_t x = rand() % 400 - 800 + mMaskImage->GetWidth() / 2;
            std::size_t y = rand() % 400 - 700 + mMaskImage->GetHeight() / 2;

            for( int a = 0; a < mMaskWidth; a++ )
            {
                x++;
                y += rand() % 80 - 40;
                std::size_t sortHeight = rand() % mMaskHeight + 50;

                std::uint32_t weight = 255;
                for (int h = 0; h < sortHeight; h++)
                {
                    if( rand() % 500 < 2 ) weight = rand() % 255;
                    if (y + h >= mMaskImage->GetHeight()) break;

                    std::size_t index = ((y + h) * mMaskImage->GetWidth() + x) * 4;
                    bufferData[index + 0] = weight;
                    bufferData[index + 1] = 0;
                    bufferData[index + 2] = 0;
                    bufferData[index + 3] = 255;
                }
            }
        }
    }
    stagingBuffer->UnMapMemory();

    auto commandBuffer = mContext.mDevice.BeginSingleTimeCommands();
    {
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
                vk::Extent3D( mMaskImage->GetWidth(), mMaskImage->GetHeight(), 1 )
            )
        );
    }
    mContext.mDevice.EndSingleTimeCommands( commandBuffer );

}


