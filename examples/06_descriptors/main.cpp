#include "camera.hpp"
#include "cie.hpp"
#include "graphics/graphics.hpp"
#include "input.hpp"
#include "mesh.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QStyleFactory>
#include <QWidget>
#include <QWindow>
#include <gfx/log.hpp>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

class worker
{
public:
    template<typename Fun>
    worker(Fun&& fun)
          : _stopped(false), _worker_thread([&] {
              while (!_stopped && fun(*this))
                  ;
          })
    {}
    ~worker()
    {
        _stopped = true;
        _worker_thread.join();
    }

    const std::atomic_bool& has_stopped() const noexcept { return _stopped; }
    void                    trigger_stop() noexcept { _stopped = true; }

private:
    std::atomic_bool _stopped;
    std::thread      _worker_thread;
};

gfx::exp::image load_cubemap(gfx::device& gpu, const std::filesystem::path& root);

int main(int argc, char** argv)
{
    using gfx::u32;
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("fusion"));
    QFont defaultFont = QApplication::font();
    defaultFont.setPointSize(defaultFont.pointSize() + 2);
    app.setFont(defaultFont);
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
    darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Dark, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    app.setPalette(darkPalette);
    app.setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");

    QMainWindow win;
    win.resize(1280, 720);
    QMenuBar* menuBar  = new QMenuBar();
    QMenu*    fileMenu = new QMenu("File");
    menuBar->addMenu(fileMenu);
    fileMenu->addAction("Open", [] { gfx::ilog << "Open..."; }, QKeySequence::Open);
    fileMenu->addAction("Save", [] { gfx::ilog << "Save..."; }, QKeySequence::Save);
    fileMenu->addAction("Save As", [] { gfx::ilog << "Save as..."; }, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("Close", [&win] { win.close(); }, QKeySequence::Close);
    fileMenu->addAction("Quit", [&win] { QCoreApplication::quit(); }, QKeySequence::Quit);

    QSplitter* mainLayout = new QSplitter;
    win.setCentralWidget(mainLayout);
    mainLayout->setContentsMargins(QMargins(8, 8, 8, 8));
    win.setMenuBar(menuBar);
    QFrame* render_frame = new QFrame;
    render_frame->setFrameStyle(QFrame::Shadow_Mask);
    render_frame->setFrameShadow(QFrame::Sunken);
    QVBoxLayout* render_frame_layout = new QVBoxLayout;
    render_frame->setLayout(render_frame_layout);
    render_frame->setStyleSheet("background-color: black");
    render_frame_layout->setContentsMargins(QMargins(1, 1, 1, 1));
    QWidget* render_surface = new QWidget;
    render_frame_layout->addWidget(render_surface);
    render_surface->setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    QVBoxLayout* right_panel_layout = new QVBoxLayout;
    QWidget*     right_panel        = new QWidget;
    mainLayout->addWidget(right_panel);
    right_panel->setLayout(right_panel_layout);

    QTabWidget*  tabWidget      = new QTabWidget;
    QWidget*     general        = new QWidget;
    QVBoxLayout* general_layout = new QVBoxLayout;
    general->setLayout(general_layout);
    QGroupBox*   gfx_info        = new QGroupBox("Graphics");
    QFormLayout* gfx_info_layout = new QFormLayout;
    gfx_info->setLayout(gfx_info_layout);
    QGroupBox*   perf_info        = new QGroupBox("Performance");
    QFormLayout* perf_info_layout = new QFormLayout;
    perf_info->setLayout(perf_info_layout);

    general_layout->addWidget(gfx_info);
    general_layout->addWidget(perf_info);
    general_layout->addWidget(new QWidget, 1);

    tabWidget->addTab(general, "General");
    tabWidget->addTab(new QWidget, "Permissions");
    right_panel_layout->addWidget(tabWidget);
    right_panel_layout->setContentsMargins(QMargins(0, 0, 0, 0));
    win.setStatusBar(new QStatusBar);
    mainLayout->addWidget(render_frame);

    QVBoxLayout* left_panel_layout = new QVBoxLayout;
    QWidget*     left_panel        = new QWidget;
    mainLayout->addWidget(left_panel);
    left_panel->setLayout(left_panel_layout);
    left_panel_layout->addWidget(new QPushButton("Press me, I'm Qt"));
    left_panel_layout->setContentsMargins(QMargins(0, 0, 0, 0));

    QGroupBox*   inputs        = new QGroupBox("Inputs");
    QFormLayout* inputs_layout = new QFormLayout;
    inputs->setLayout(inputs_layout);
    left_panel_layout->addWidget(inputs);
    QSlider* slider_r = new QSlider(Qt::Horizontal);
    QSlider* slider_g = new QSlider(Qt::Horizontal);
    QSlider* slider_b = new QSlider(Qt::Horizontal);
    slider_r->setRange(0, 255);
    slider_g->setRange(0, 255);
    slider_b->setRange(0, 255);
    inputs_layout->addRow(new QLabel("Red"), slider_r);
    inputs_layout->addRow(new QLabel("Green"), slider_g);
    inputs_layout->addRow(new QLabel("Blue"), slider_b);

    mainLayout->setSizes(QList<int>({250, 1280, 250}));

    std::array<float, 4> clear_color = {0.f, 0.f, 0.f, 1.f};
    win.connect(slider_r, &QSlider::valueChanged, [&](int val) { clear_color[0] = val / 255.f; });
    win.connect(slider_g, &QSlider::valueChanged, [&](int val) { clear_color[1] = val / 255.f; });
    win.connect(slider_b, &QSlider::valueChanged, [&](int val) { clear_color[2] = val / 255.f; });
    win.show();

    gfx::key_event_filter* keys = new gfx::key_event_filter(render_surface);
    render_surface->installEventFilter(keys);

    gfx::instance              my_app("Application", gfx::version_t(1, 0, 0), false, true);
    gfx::surface               surf1(my_app, render_surface);
    gfx::device                gpu(my_app, gfx::device_target::gpu, {1.f, 0.5f}, 1.f, surf1);
    gfx::swapchain             chain(gpu, surf1);
    std::vector<gfx::commands> gpu_cmd = gpu.allocate_graphics_commands(chain.count());
    gfx::semaphore             acquire_image_signal(gpu);
    gfx::semaphore             render_finish_signal(gpu);
    std::vector<gfx::fence>    cmd_fences(gpu_cmd.size(), gfx::fence(gpu, true));

    gfx::mapped<float> my_floats(gpu);
    for (int i = 0; i < 10; ++i) my_floats.emplace_back(i);
    my_floats.insert(my_floats.begin(), {0.f, 1.f, 0.3f});
    my_floats.erase(my_floats.begin() + 4);

    gfx::buffer<float> tbuf(gpu, my_floats);
    gfx::buffer<float> tbuf2 = tbuf;

    const auto  props       = gpu.get_physical_device().getProperties2();
    const char* device_type = [&] {
        using dt = vk::PhysicalDeviceType;
        switch (props.properties.deviceType)
        {
        case dt::eCpu: return "CPU";
        case dt::eDiscreteGpu: return "Discrete GPU";
        case dt::eIntegratedGpu: return "iGPU";
        case dt::eOther: return "Unknown";
        case dt::eVirtualGpu: return "Virtual GPU";
        }
    }();
    gfx_info_layout->addRow("Renderer", new QLabel("Vulkan"));
    gfx_info_layout->addRow("Version", new QLabel(QString::fromStdString(to_string(gfx::to_version(props.properties.apiVersion)))));
    gfx_info_layout->addRow("Device", new QLabel(props.properties.deviceName));
    gfx_info_layout->addRow("Type", new QLabel(device_type));
    gfx_info_layout->addRow("Driver", new QLabel(QString::fromStdString(to_string(gfx::to_version(props.properties.driverVersion)))));

    QLabel* fps_counter = new QLabel("waiting...");
    QLabel* ftm_counter = new QLabel("waiting...");
    perf_info_layout->addRow("Framerate", fps_counter);
    perf_info_layout->addRow("Frametime", ftm_counter);


    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		Output Images
    ////
    ////////////////////////////////////////////////////////////////////////////
    vk::ImageCreateInfo color_accum_create;
    color_accum_create.arrayLayers   = 1;
    color_accum_create.extent        = vk::Extent3D(chain.extent().width, chain.extent().height, 1);
    color_accum_create.format        = vk::Format::eR32G32B32A32Sfloat;
    color_accum_create.imageType     = vk::ImageType::e2D;
    color_accum_create.initialLayout = vk::ImageLayout::eUndefined;
    color_accum_create.mipLevels     = 1;
    color_accum_create.samples       = vk::SampleCountFlagBits::e1;
    color_accum_create.sharingMode   = vk::SharingMode::eExclusive;
    color_accum_create.tiling        = vk::ImageTiling::eOptimal;
    color_accum_create.usage =
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
    gfx::exp::image color_accum(gpu, color_accum_create);
	gfx::exp::image bounce_accum(gpu, color_accum_create);
	gfx::exp::image positions_bounce(gpu, color_accum_create);
	gfx::exp::image directions_sample(gpu, color_accum_create);

    vk::ImageViewCreateInfo color_accum_view_create;
    color_accum_view_create.format           = color_accum_create.format;
    color_accum_view_create.image            = color_accum.get_image();
    color_accum_view_create.viewType         = vk::ImageViewType::e2D;
    color_accum_view_create.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    vk::UniqueImageView color_accum_view     = gpu.get_device().createImageViewUnique(color_accum_view_create);
    color_accum_view_create.image            = bounce_accum.get_image();
    vk::UniqueImageView bounce_accum_view     = gpu.get_device().createImageViewUnique(color_accum_view_create);
    color_accum_view_create.image            = positions_bounce.get_image();
    vk::UniqueImageView positions_bounce_view     = gpu.get_device().createImageViewUnique(color_accum_view_create);
    color_accum_view_create.image            = directions_sample.get_image();
    vk::UniqueImageView directions_sample_view     = gpu.get_device().createImageViewUnique(color_accum_view_create);

    gfx::commands switch_layout = gpu.allocate_transfer_command();
    switch_layout.cmd().begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    {
        vk::ImageMemoryBarrier attachment_barrier;
		attachment_barrier.oldLayout           = vk::ImageLayout::eUndefined;
		attachment_barrier.srcAccessMask       = {};
		attachment_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		attachment_barrier.newLayout           = vk::ImageLayout::eGeneral;
		attachment_barrier.dstAccessMask       = {};
		attachment_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		attachment_barrier.image               = color_accum.get_image();
		attachment_barrier.subresourceRange    = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier attachment_barrier_bounce = attachment_barrier;
		attachment_barrier_bounce.image               = bounce_accum.get_image();
		vk::ImageMemoryBarrier positions_bounce_barrier = attachment_barrier;
		positions_bounce_barrier.image = positions_bounce.get_image();
		vk::ImageMemoryBarrier directions_sample_barrier = attachment_barrier;
		directions_sample_barrier.image = directions_sample.get_image();
        switch_layout.cmd().pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe,
                                            vk::DependencyFlagBits::eByRegion, {}, {}, { attachment_barrier, attachment_barrier_bounce, positions_bounce_barrier, directions_sample_barrier });
    }
    switch_layout.cmd().end();
    gpu.transfer_queue().submit({switch_layout}, {}, {});


    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		Framebuffer/Renderpass
    ////
    ////////////////////////////////////////////////////////////////////////////
    struct att_desc
    {
        enum
        {
            color,
            accum,
            bounce,
            pos_bounce,
            dir_sample,
			_count
        };
    };
    vk::AttachmentDescription attachment_descriptions[att_desc::_count];
    attachment_descriptions[att_desc::color].initialLayout  = vk::ImageLayout::eUndefined;
    attachment_descriptions[att_desc::color].finalLayout    = vk::ImageLayout::ePresentSrcKHR;
    attachment_descriptions[att_desc::color].format         = chain.format();
    attachment_descriptions[att_desc::color].loadOp         = vk::AttachmentLoadOp::eClear;
    attachment_descriptions[att_desc::color].storeOp        = vk::AttachmentStoreOp::eStore;
    attachment_descriptions[att_desc::color].stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
    attachment_descriptions[att_desc::color].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachment_descriptions[att_desc::color].samples        = vk::SampleCountFlagBits::e1;

	attachment_descriptions[att_desc::accum].initialLayout  = vk::ImageLayout::eGeneral;
	attachment_descriptions[att_desc::accum].finalLayout    = vk::ImageLayout::eGeneral;
	attachment_descriptions[att_desc::accum].format         = vk::Format::eR32G32B32A32Sfloat;
	attachment_descriptions[att_desc::accum].loadOp         = vk::AttachmentLoadOp::eLoad;
	attachment_descriptions[att_desc::accum].storeOp        = vk::AttachmentStoreOp::eStore;
	attachment_descriptions[att_desc::accum].stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
	attachment_descriptions[att_desc::accum].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachment_descriptions[att_desc::accum].samples        = vk::SampleCountFlagBits::e1;

	attachment_descriptions[att_desc::bounce] = attachment_descriptions[att_desc::accum];
	attachment_descriptions[att_desc::pos_bounce] = attachment_descriptions[att_desc::accum];
	attachment_descriptions[att_desc::dir_sample] = attachment_descriptions[att_desc::accum];

    const vk::AttachmentReference color_attachments[] = {vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
                                                         vk::AttachmentReference(1, vk::ImageLayout::eGeneral),
														 vk::AttachmentReference(2, vk::ImageLayout::eGeneral),
														 vk::AttachmentReference(3, vk::ImageLayout::eGeneral),
														 vk::AttachmentReference(4, vk::ImageLayout::eGeneral) };

    vk::SubpassDescription main_subpass;
    main_subpass.colorAttachmentCount = u32(std::size(color_attachments));
    main_subpass.pColorAttachments    = std::data(color_attachments);
    main_subpass.pipelineBindPoint    = vk::PipelineBindPoint::eGraphics;

    vk::SubpassDependency main_subpass_dep;
    main_subpass_dep.srcSubpass      = VK_SUBPASS_EXTERNAL;
    main_subpass_dep.srcAccessMask   = {};
    main_subpass_dep.srcStageMask    = vk::PipelineStageFlagBits::eBottomOfPipe;
    main_subpass_dep.dstSubpass      = 0;
    main_subpass_dep.dstAccessMask   = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    main_subpass_dep.dstStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    main_subpass_dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::RenderPassCreateInfo rp_info;
    rp_info.attachmentCount = u32(std::size(attachment_descriptions));
    rp_info.pAttachments    = std::data(attachment_descriptions);
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &main_subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &main_subpass_dep;
    const auto pass         = gpu.get_device().createRenderPassUnique(rp_info);

    std::vector<vk::UniqueImageView>   imvs;
    std::vector<vk::UniqueFramebuffer> fbos;

    const auto build_fbos = [&] {
        fbos.clear();
        imvs.clear();
        for (size_t i = 0; i < chain.count(); ++i)
        {
            vk::ImageViewCreateInfo imv_create;
            imv_create.format           = chain.format();
            imv_create.image            = chain.imgs()[i];
            imv_create.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
            imv_create.viewType         = vk::ImageViewType::e2D;
            imv_create.components.r     = vk::ComponentSwizzle::eIdentity;
            imvs.emplace_back(gpu.get_device().createImageViewUnique(imv_create));

			const auto attachments = { imvs[i].get(), color_accum_view.get(), bounce_accum_view.get(), positions_bounce_view.get(), directions_sample_view.get() };

            vk::FramebufferCreateInfo fbo_create;
            fbo_create.attachmentCount = u32(std::size(attachments));
            fbo_create.renderPass      = pass.get();
            fbo_create.width           = chain.extent().width;
            fbo_create.height          = chain.extent().height;
            fbo_create.layers          = 1;
            fbo_create.pAttachments    = std::data(attachments);
            fbos.emplace_back(gpu.get_device().createFramebufferUnique(fbo_create));
        }
    };
    build_fbos();

    const gfx::shader vert(gpu, "postfx/screen.vert.vk.spv");
    const gfx::shader frag(gpu, "06_descriptors/spectral.frag.vk.spv");

    vk::GraphicsPipelineCreateInfo pipe_info;
    pipe_info.subpass    = 0;
    pipe_info.renderPass = pass.get();
    const auto stages    = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vert.get_module(), "main"),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, frag.get_module(), "main"),
    };
    pipe_info.stageCount = gfx::u32(std::size(stages));
    pipe_info.pStages    = std::data(stages);

    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		Descriptor Pool
    ////
    ////////////////////////////////////////////////////////////////////////////
    using dct                                  = vk::DescriptorType;
    vk::DescriptorPoolSize       dpool_sizes[] = {{dct::eUniformBuffer, 1}, {dct::eStorageBuffer, 8}};
    vk::DescriptorPoolCreateInfo dpool_info(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 3, u32(std::size(dpool_sizes)),
                                            std::data(dpool_sizes));
    vk::UniqueDescriptorPool     dpool = gpu.get_device().createDescriptorPoolUnique(dpool_info);

    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		ECS
    ////
    ////////////////////////////////////////////////////////////////////////////
    gfx::ecs::ecs           ecs;
    gfx::ecs::system_list   control_systems;
    gfx::user_camera_system camera_system(*keys);
    control_systems.add(*keys);
    control_systems.add(camera_system);
    gfx::ecs::unique_entity user_entity = ecs.create_entity_unique(gfx::camera_component(), gfx::camera_controls(),
                                                                   gfx::transform_component(), gfx::grabbed_cursor_component());
	user_entity->get<gfx::transform_component>()->value.position.z = -4.f;

    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		Meshes
    ////
    ////////////////////////////////////////////////////////////////////////////
    mesh_allocator mesh_alloc(gpu);
    mesh_handle    bunny_handle = mesh_alloc.allocate_meshes(gfx::scene_file("lens.dae"))[0];
    mesh_handle    floor_handle = mesh_alloc.allocate_meshes(gfx::scene_file("floor.dae"))[0];
	mesh_handle    box_handle = mesh_alloc.allocate_meshes(gfx::scene_file("box.dae"))[0];

    mesh_alloc.clear_instances_of(bunny_handle);
    mesh_alloc.add_instance(bunny_handle, gfx::transform({ 0, 2.5f, 0 }, { 1, 1.f, 1.f }, glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0))), glm::vec4(1, 1, 1, 1), 0.0001f, 1.f);
    mesh_alloc.add_instance(box_handle, gfx::transform({ 0, 0.9f, 0 }, { 1, 1, 1 }, glm::angleAxis(glm::radians(-90.f), glm::vec3(1, 0, 0))), glm::vec4(1, 1, 1, 1), 0.0001f, 0.f);
	mesh_alloc.add_instance(floor_handle, gfx::transform({0, -1.1f, 0}, {1, 1, 1}, glm::angleAxis(glm::radians(90.f), glm::vec3(1, 0, 0))), glm::vec4(1, 1, 1, 1), 0.0001f, 0.f);

    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		Environment
    ////
    ////////////////////////////////////////////////////////////////////////////
	gfx::exp::image cubemap = load_cubemap(gpu, "moulton_station_train_tunnel_west_16k.hdr/hdr");
	vk::ImageViewCreateInfo cubemap_view_create;
	cubemap_view_create.image = cubemap.get_image();
	cubemap_view_create.format = vk::Format::eR32G32B32A32Sfloat;
	cubemap_view_create.viewType = vk::ImageViewType::eCube;
	cubemap_view_create.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6);
	vk::UniqueImageView cubemap_view = gpu.get_device().createImageViewUnique(cubemap_view_create);
	vk::SamplerCreateInfo cubemap_sampler_create;
	cubemap_sampler_create.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	cubemap_sampler_create.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	cubemap_sampler_create.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	cubemap_sampler_create.anisotropyEnable = true;
	cubemap_sampler_create.maxAnisotropy = 16.f;
	cubemap_sampler_create.compareEnable = false;
	cubemap_sampler_create.magFilter = vk::Filter::eLinear;
	cubemap_sampler_create.minFilter = vk::Filter::eLinear;
	cubemap_sampler_create.mipmapMode = vk::SamplerMipmapMode::eLinear;
	vk::UniqueSampler cubemap_sampler = gpu.get_device().createSamplerUnique(cubemap_sampler_create);

    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		CIE
    ////
    ////////////////////////////////////////////////////////////////////////////
    vk::ImageCreateInfo cie_spectrum_create;
    cie_spectrum_create.arrayLayers   = 1;
    cie_spectrum_create.extent        = vk::Extent3D(u32(std::size(gfx::cie_curves)), 1, 1);
    cie_spectrum_create.format        = vk::Format::eR32G32B32A32Sfloat;
    cie_spectrum_create.imageType     = vk::ImageType::e1D;
    cie_spectrum_create.initialLayout = vk::ImageLayout::eUndefined;
    cie_spectrum_create.mipLevels     = 1;
    cie_spectrum_create.samples       = vk::SampleCountFlagBits::e1;
    cie_spectrum_create.sharingMode   = vk::SharingMode::eExclusive;
    cie_spectrum_create.tiling        = vk::ImageTiling::eOptimal;
    cie_spectrum_create.usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    gfx::exp::image cie_spectrum(gpu, cie_spectrum_create);

    gfx::mapped<glm::vec4> cie_values(gpu, gfx::cie_curves);
    gfx::commands           transfer_cie = gpu.allocate_transfer_command();
    transfer_cie.cmd().begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    {
        vk::ImageMemoryBarrier cie_barrier;
        cie_barrier.oldLayout           = vk::ImageLayout::eUndefined;
        cie_barrier.srcAccessMask       = {};
        cie_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        cie_barrier.newLayout           = vk::ImageLayout::eTransferDstOptimal;
        cie_barrier.dstAccessMask       = vk::AccessFlagBits::eMemoryWrite;
        cie_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        cie_barrier.image               = cie_spectrum.get_image();
        cie_barrier.subresourceRange    = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        transfer_cie.cmd().pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer,
                                           vk::DependencyFlagBits::eByRegion, {}, {}, cie_barrier);

        vk::BufferImageCopy cie_copy;
        cie_copy.imageExtent      = cie_spectrum_create.extent;
        cie_copy.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        transfer_cie.cmd().copyBufferToImage(cie_values.get_buffer(), cie_spectrum.get_image(), vk::ImageLayout::eTransferDstOptimal,
                                             cie_copy);

        cie_barrier.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
        cie_barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
        cie_barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        cie_barrier.dstAccessMask = {};
        transfer_cie.cmd().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe,
                                           vk::DependencyFlagBits::eByRegion, {}, {}, cie_barrier);
    }
    transfer_cie.cmd().end();
    gpu.transfer_queue().submit({transfer_cie}, {}, {});

    vk::ImageViewCreateInfo cie_spectrum_view_create;
    cie_spectrum_view_create.format           = cie_spectrum_create.format;
    cie_spectrum_view_create.image            = cie_spectrum.get_image();
    cie_spectrum_view_create.viewType         = vk::ImageViewType::e1D;
    cie_spectrum_view_create.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    vk::UniqueImageView cie_spectrum_view     = gpu.get_device().createImageViewUnique(cie_spectrum_view_create);

    vk::SamplerCreateInfo cie_sampler_create;
    cie_sampler_create.addressModeU     = vk::SamplerAddressMode::eClampToEdge;
    cie_sampler_create.addressModeV     = vk::SamplerAddressMode::eClampToEdge;
    cie_sampler_create.addressModeW     = vk::SamplerAddressMode::eClampToEdge;
    cie_sampler_create.anisotropyEnable = false;
    cie_sampler_create.compareEnable    = false;
    cie_sampler_create.magFilter        = vk::Filter::eLinear;
    cie_sampler_create.minFilter        = vk::Filter::eLinear;
    cie_sampler_create.mipmapMode       = vk::SamplerMipmapMode::eNearest;
    vk::UniqueSampler cie_sampler       = gpu.get_device().createSamplerUnique(cie_sampler_create);

    ////////////////////////////////////////////////////////////////////////////
    ////
    ////		Descriptor Set Layouts
    ////
    ////////////////////////////////////////////////////////////////////////////
    vk::DescriptorSetLayoutBinding    mat_binding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAll);
    vk::DescriptorSetLayoutCreateInfo dset_create({}, 1, &mat_binding);
    vk::UniqueDescriptorSetLayout     mat_set_layout = gpu.get_device().createDescriptorSetLayoutUnique(dset_create);

    vk::DescriptorSetLayoutBinding mesh_bindings[] = {
        {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment},    // BVH Nodes
        {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment},    // Vertices
        {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment},    // Indices
        {3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment},    // Instances
    };
    vk::DescriptorSetLayoutCreateInfo mesh_set_create({}, u32(std::size(mesh_bindings)), std::data(mesh_bindings));
    vk::UniqueDescriptorSetLayout     mesh_set_layout = gpu.get_device().createDescriptorSetLayoutUnique(mesh_set_create);

    vk::DescriptorSetLayoutBinding globals_bindings[] = {
        {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment},           // Globals
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},    // CIE values
        {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},    // Cubemap
        {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},    // Accum
        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},    // Bounce
		{5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},    // Pos + bounce
		{6, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},    // Dir + sample
    };
    vk::DescriptorSetLayoutCreateInfo globals_set_create({}, u32(std::size(globals_bindings)), std::data(globals_bindings));
    vk::UniqueDescriptorSetLayout     globals_set_layout = gpu.get_device().createDescriptorSetLayoutUnique(globals_set_create);

    const auto                   used_layouts = {mat_set_layout.get(), mesh_set_layout.get(), globals_set_layout.get()};
    vk::PipelineLayoutCreateInfo pipe_layout_info({}, u32(std::size(used_layouts)), std::data(used_layouts));

    vk::UniquePipelineLayout pipe_layout = gpu.get_device().createPipelineLayoutUnique(pipe_layout_info);
    pipe_info.layout                     = pipe_layout.get();

    vk::PipelineViewportStateCreateInfo vp_state({}, 1, nullptr, 1, nullptr);
    pipe_info.pViewportState = &vp_state;
    const auto dyn_states    = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dyn_state({}, gfx::u32(std::size(dyn_states)), std::data(dyn_states));
    pipe_info.pDynamicState = &dyn_state;

    vk::PipelineVertexInputStateCreateInfo vert_info;
    pipe_info.pVertexInputState = &vert_info;

    vk::PipelineRasterizationStateCreateInfo rst_state({}, 0, 0, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
                                                       vk::FrontFace::eCounterClockwise, false, 0.f, 0.f, 0.f, 1.f);
    pipe_info.pRasterizationState = &rst_state;

    vk::PipelineInputAssemblyStateCreateInfo inp_state({}, vk::PrimitiveTopology::eTriangleList, false);
    pipe_info.pInputAssemblyState = &inp_state;

    vk::PipelineDepthStencilStateCreateInfo dep_state;
    pipe_info.pDepthStencilState = &dep_state;

    vk::PipelineMultisampleStateCreateInfo msaa_state;
    pipe_info.pMultisampleState = &msaa_state;

    vk::PipelineColorBlendAttachmentState blend_col_atts[5];
	blend_col_atts[0].blendEnable = false;
	blend_col_atts[0].colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	blend_col_atts[1] = blend_col_atts[0];
	blend_col_atts[2] = blend_col_atts[0];
	blend_col_atts[3] = blend_col_atts[0];
	blend_col_atts[4] = blend_col_atts[0];
    vk::PipelineColorBlendStateCreateInfo bln_state({}, false, {}, u32(std::size(blend_col_atts)), std::data(blend_col_atts));
    pipe_info.pColorBlendState = &bln_state;

    vk::UniquePipeline                pipe = gpu.get_device().createGraphicsPipelineUnique(nullptr, pipe_info);
    gfx::mapped<gfx::camera_matrices> buf(gpu, {*gfx::get_camera_info(*user_entity)});

    vk::DescriptorSetAllocateInfo mat_set_alloc(dpool.get(), 1, &mat_set_layout.get());
    vk::UniqueDescriptorSet       mat_set = std::move(gpu.get_device().allocateDescriptorSetsUnique(mat_set_alloc)[0]);

    vk::DescriptorSetAllocateInfo mesh_set_alloc(dpool.get(), 1, &mesh_set_layout.get());
    vk::UniqueDescriptorSet       mesh_set = std::move(gpu.get_device().allocateDescriptorSetsUnique(mesh_set_alloc)[0]);

    vk::DescriptorSetAllocateInfo globals_set_alloc(dpool.get(), 1, &globals_set_layout.get());
    vk::UniqueDescriptorSet       globals_set = std::move(gpu.get_device().allocateDescriptorSetsUnique(globals_set_alloc)[0]);

    vk::DescriptorBufferInfo mat_buf_info(buf.get_buffer(), 0, sizeof(gfx::camera_matrices));
    vk::WriteDescriptorSet   mat_write(mat_set.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &mat_buf_info);
    gpu.get_device().updateDescriptorSets(mat_write, {});

    vk::DescriptorBufferInfo mesh_buf_info0(mesh_alloc.bvh_buffer().get_buffer(), 0,
                                            mesh_alloc.bvh_buffer().size() * sizeof(gfx::bvh<3>::node));
    vk::WriteDescriptorSet   mesh_write0(mesh_set.get(), 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &mesh_buf_info0);
    vk::DescriptorBufferInfo mesh_buf_info1(mesh_alloc.vertex_buffer().get_buffer(), 0,
                                            mesh_alloc.vertex_buffer().size() * sizeof(gfx::vertex3d));
    vk::WriteDescriptorSet   mesh_write1(mesh_set.get(), 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &mesh_buf_info1);
    vk::DescriptorBufferInfo mesh_buf_info2(mesh_alloc.index_buffer().get_buffer(), 0,
                                            mesh_alloc.index_buffer().size() * sizeof(gfx::index32));
    vk::WriteDescriptorSet   mesh_write2(mesh_set.get(), 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &mesh_buf_info2);
    vk::DescriptorBufferInfo mesh_buf_info3(mesh_alloc.instances().get_buffer(), 0,
                                            mesh_alloc.instances().size() * sizeof(mesh_allocator::instance));
    vk::WriteDescriptorSet   mesh_write3(mesh_set.get(), 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &mesh_buf_info3);
    gpu.get_device().updateDescriptorSets({mesh_write0, mesh_write1, mesh_write2, mesh_write3}, {});

    struct globals
    {
        glm::ivec2 viewport;
        float      random;
        int        rendered_count;
    };
    gfx::buffer<globals>                  globals_buffer(gpu, {globals{}});
    std::mt19937                          gen;
    std::uniform_real_distribution<float> dist;
    vk::DescriptorBufferInfo              globals_buf_info0(globals_buffer.get_buffer(), 0, sizeof(globals));
    vk::WriteDescriptorSet  globals_write0(globals_set.get(), 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &globals_buf_info0);
    vk::DescriptorImageInfo globals_buf_info1(cie_sampler.get(), cie_spectrum_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet  globals_write1(globals_set.get(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &globals_buf_info1);
	vk::DescriptorImageInfo globals_buf_info2(cubemap_sampler.get(), cubemap_view.get(), vk::ImageLayout::eGeneral);
	vk::WriteDescriptorSet  globals_write2(globals_set.get(), 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &globals_buf_info2);
    vk::DescriptorImageInfo globals_buf_info3(cie_sampler.get(), color_accum_view.get(), vk::ImageLayout::eGeneral);
    vk::WriteDescriptorSet  globals_write3(globals_set.get(), 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &globals_buf_info3);
    vk::DescriptorImageInfo globals_buf_info4(cie_sampler.get(), bounce_accum_view.get(), vk::ImageLayout::eGeneral);
    vk::WriteDescriptorSet  globals_write4(globals_set.get(), 4, 0, 1, vk::DescriptorType::eCombinedImageSampler, &globals_buf_info4);
	vk::DescriptorImageInfo globals_buf_info5(cie_sampler.get(), positions_bounce_view.get(), vk::ImageLayout::eGeneral);
	vk::WriteDescriptorSet  globals_write5(globals_set.get(), 5, 0, 1, vk::DescriptorType::eCombinedImageSampler, &globals_buf_info5);
	vk::DescriptorImageInfo globals_buf_info6(cie_sampler.get(), directions_sample_view.get(), vk::ImageLayout::eGeneral);
	vk::WriteDescriptorSet  globals_write6(globals_set.get(), 6, 0, 1, vk::DescriptorType::eCombinedImageSampler, &globals_buf_info6);
    gpu.get_device().updateDescriptorSets({globals_write0, globals_write1, globals_write2, globals_write3, globals_write4, globals_write5, globals_write6 }, {});

    std::chrono::duration<double> time_sec    = std::chrono::duration<double>::zero();
    std::chrono::duration<double> delta       = std::chrono::duration<double>::zero();
    std::chrono::duration<double> delta_frame = std::chrono::duration<double>::zero();
    size_t                        frame_count = 0;
    std::chrono::time_point       begin       = std::chrono::steady_clock::now();
    std::chrono::time_point       frame_begin = std::chrono::steady_clock::now();
    std::chrono::time_point       last_time   = std::chrono::steady_clock::now();

	gfx::transform last_transform;
    globals ng;
    ng.rendered_count = 0;
    worker render_thread([&](worker& self) {
        delta_frame = std::chrono::steady_clock::now() - last_time;
        last_time   = std::chrono::steady_clock::now();

        ecs.update(delta_frame.count(), control_systems);
        keys->post_update();

        using namespace std::chrono_literals;
        time_sec = std::chrono::steady_clock::now() - begin;
        ++frame_count;
        delta = std::chrono::steady_clock::now() - frame_begin;

        if (delta > 1s)
        {
            fps_counter->setText(QString::fromStdString(std::to_string(frame_count / delta.count())));
            ftm_counter->setText(QString::fromStdString(std::to_string(1'000.0 * delta.count() / frame_count) + "ms"));
            frame_begin = std::chrono::steady_clock::now();
            frame_count = 0;
            delta       = std::chrono::duration<double>::zero();
        }

        const auto [img, acquire_error] = chain.next_image(acquire_image_signal);
        if (acquire_error && (*acquire_error == gfx::acquire_error::out_of_date || *acquire_error == gfx::acquire_error::suboptimal))
        {
            if (!chain.recreate())
            {
                gfx::ilog << "Could not recreate swapchain. Exiting.";
                return false;
            }
            build_fbos();
        }

        gpu.wait_for({cmd_fences[img]});
        gpu.reset_fences({cmd_fences[img]});
        gpu_cmd[img].cmd().reset({});
        gpu_cmd[img].cmd().begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});

        user_entity->get<gfx::camera_component>()->projection.perspective().screen_width  = chain.extent().width;
        user_entity->get<gfx::camera_component>()->projection.perspective().screen_height = chain.extent().height;
        const auto data                                                                   = {*gfx::get_camera_info(*user_entity)};
        gpu_cmd[img].cmd().updateBuffer(buf.get_buffer(), 0ull, std::size(data) * sizeof(gfx::camera_matrices), std::data(data));

        if (keys->key_down(Qt::Key_R) || last_transform != user_entity->get<gfx::transform_component>()->value)
        {
			last_transform = user_entity->get<gfx::transform_component>()->value;
            gpu_cmd[img].cmd().clearColorImage(color_accum.get_image(), vk::ImageLayout::eGeneral,
                                               vk::ClearColorValue(std::array{0.f, 0.f, 0.f, 0.f}),
                                               vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
            ng.rendered_count = 0;
        }

        ng.random      = dist(gen);
        ng.viewport[0] = chain.extent().width;
        ng.viewport[1] = chain.extent().height;
        ng.rendered_count++;
        gpu_cmd[img].cmd().updateBuffer(globals_buffer.get_buffer(), 0ull, 1 * sizeof(globals), &ng);

        vk::ClearValue          clr(vk::ClearColorValue{clear_color});
        vk::RenderPassBeginInfo beg;
        beg.clearValueCount = 1;
        beg.pClearValues    = &clr;
        beg.framebuffer     = fbos[img].get();
        beg.renderPass      = pass.get();
        beg.renderArea      = vk::Rect2D({0, 0}, chain.extent());
        gpu_cmd[img].cmd().beginRenderPass(beg, vk::SubpassContents::eInline);

        gpu_cmd[img].cmd().setViewport(0, vk::Viewport(0.f, 0.f, chain.extent().width, chain.extent().height, 0.f, 1.f));
        gpu_cmd[img].cmd().setScissor(0, vk::Rect2D({0, 0}, chain.extent()));
        gpu_cmd[img].cmd().bindPipeline(vk::PipelineBindPoint::eGraphics, pipe.get());

        gpu_cmd[img].cmd().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipe_layout.get(), 0,
                                              {mat_set.get(), mesh_set.get(), globals_set.get()}, nullptr);
        gpu_cmd[img].cmd().draw(3, 1, 0, 0);

        gpu_cmd[img].cmd().endRenderPass();
        gpu_cmd[img].cmd().end();

        gpu.graphics_queue().submit({gpu_cmd[img]}, {acquire_image_signal}, {render_finish_signal}, cmd_fences[img]);
        gpu.graphics_queue().wait();
        const auto present_error = gpu.present_queue().present({{img, chain}}, {render_finish_signal});
        if (present_error)
        {
            if (!chain.recreate())
            {
                gfx::ilog << "Could not recreate swapchain. Exiting.";
                return false;
            }
            build_fbos();
        }
        return true;
    });

    app.exec();
}

gfx::exp::image load_cubemap(gfx::device& gpu, const std::filesystem::path& root)
{
	gfx::image_file posx(root / "posx.hdr", gfx::bits::b32, 4);
	gfx::image_file negx(root / "negx.hdr", gfx::bits::b32, 4);
	gfx::image_file posy(root / "posy.hdr", gfx::bits::b32, 4);
	gfx::image_file negy(root / "negy.hdr", gfx::bits::b32, 4);
	gfx::image_file posz(root / "posz.hdr", gfx::bits::b32, 4);
	gfx::image_file negz(root / "negz.hdr", gfx::bits::b32, 4);

	vk::ImageCreateInfo cube_create;
	cube_create.flags = vk::ImageCreateFlagBits::eCubeCompatible;
	cube_create.arrayLayers = 6;
	cube_create.extent = vk::Extent3D(posx.width, posx.height, 1);
	cube_create.format = vk::Format::eR32G32B32A32Sfloat;
	cube_create.imageType = vk::ImageType::e2D;
	cube_create.mipLevels = 1;// log2(std::max(posx.width, posx.height)) + 1;
	cube_create.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
	gfx::exp::image cube(gpu, cube_create);

	const auto ptr = [](const gfx::image_file& f) { return static_cast<glm::vec4*>(f.bytes()); };
	const auto count = posx.width * posx.height;
	gfx::mapped<glm::vec4> data(gpu);
	data.insert(data.end(), ptr(posx), ptr(posx) + count);
	data.insert(data.end(), ptr(negx), ptr(negx) + count);
	data.insert(data.end(), ptr(posy), ptr(posy) + count);
	data.insert(data.end(), ptr(negy), ptr(negy) + count);
	data.insert(data.end(), ptr(posz), ptr(posz) + count);
	data.insert(data.end(), ptr(negz), ptr(negz) + count);

	gfx::commands copy = gpu.allocate_graphics_command();

	copy.cmd().begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	vk::ImageMemoryBarrier img_barrier;
	img_barrier.oldLayout = vk::ImageLayout::eUndefined;
	img_barrier.srcAccessMask = {};
	img_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	img_barrier.dstAccessMask = vk::AccessFlagBits::eMemoryWrite;
	img_barrier.image = cube.get_image();
	img_barrier.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6);
	copy.cmd().pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, {}, {}, img_barrier);

	vk::BufferImageCopy buf_copy;
	buf_copy.imageExtent = cube_create.extent;
	buf_copy.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 6);
	copy.cmd().copyBufferToImage(data.get_buffer(), cube.get_image(), vk::ImageLayout::eTransferDstOptimal, buf_copy);

    // gen mipmaps.
	img_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	img_barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
	img_barrier.newLayout = vk::ImageLayout::eGeneral;
	img_barrier.dstAccessMask = {};
	img_barrier.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6);
	copy.cmd().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, {}, {}, img_barrier);

    copy.cmd().end();
	gpu.graphics_queue().submit({ copy }, {}, {});
	gpu.graphics_queue().wait();
	return cube;
}