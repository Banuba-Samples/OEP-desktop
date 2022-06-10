#include <camera.hpp>
#include "camera_device.hpp"

#include <iostream>

namespace bnb::oep
{

    struct camera::impl
    {
        impl(const push_frame_cb_t& cb, size_t index)
        {
            CoInitialize(nullptr);
            ComPtr<IMFMediaSource> ptr_media_source;
            HRESULT hr = CreateVideoDeviceSource(&ptr_media_source, index);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to crate media source");
            }
            wrapped = std::make_unique<VideoCaptureDeviceMFWin>(ptr_media_source);
            m_device_index = index;
            m_push_frame_cb = cb;
        }

        std::unique_ptr<VideoCaptureDeviceMFWin> wrapped = nullptr;
        size_t m_device_index;
        push_frame_cb_t m_push_frame_cb;
        std::atomic_bool m_camera_is_stopped = true;
    }; /* struct camera::impl */

    /* camera::camera */
    camera::camera(const push_frame_cb_t& cb, size_t index)
        : m_impl(std::make_unique<impl>(cb, index))
    {
        int camera_width = 0;
        int camera_height = 0;
        constexpr auto frames_per_second = 30;

        using resolution_size = std::pair<uint16_t, uint16_t>;
        using resolution = std::pair<std::string, resolution_size>;
        const std::list<resolution> resolutions{
            {"HD/WXGA", {1280, 720}},
            {"XGA", {1024, 768}},
            {"SVGA", {800, 600}},
            {"VGA", {640, 480}},
            {"HVGA", {480, 320}}};

        VideoPixelFormat pixel_format = PIXEL_FORMAT_NV12;

        m_impl->wrapped->Init();

        for (const auto& r : resolutions) {
            auto [width, height] = r.second;
            if (m_impl->wrapped->AllocateAndStart(width, height, frames_per_second, pixel_format) != S_OK) {
                continue;
            }
            camera_width = width;
            camera_height = height;
            std::cout << "[Camera Win]: Selected " << r.first << " camera resolution: " << width << "x" << height << std::endl;
            m_impl->m_camera_is_stopped = false;
            break;
        }

        if (camera_width == 0 || camera_height == 0) {
            throw std::runtime_error("Failed to start camera");
        }

        m_impl->wrapped->SetCallback([this, camera_width, camera_height, pixel_format](std::shared_ptr<ScopedBufferLock> lock) {
            pixel_buffer_sptr img;

            auto data = static_cast<uint8_t*>(lock->data());
            auto surface_stride = lock->pitch();
            auto y_plane_size = camera_width * camera_height;
            auto size = camera_width * camera_height * 3 / 2;
            uint8_t* y_plane_data{data};

            if (pixel_format == PIXEL_FORMAT_NV12) {
                uint8_t* uv_plane_data = y_plane_data + y_plane_size;

                using ns = bnb::oep::interfaces::pixel_buffer;
                ns::plane_data y_plane{ std::shared_ptr<uint8_t>(y_plane_data, [lock](uint8_t*) {}), 0, static_cast<int32_t>(surface_stride) };
                ns::plane_data uv_plane{std::shared_ptr<uint8_t>(uv_plane_data, [lock](uint8_t*) { }), 0, static_cast<int32_t>(surface_stride)};
                std::vector<ns::plane_data> planes{y_plane, uv_plane};

                img = ns::create(planes, bnb::oep::interfaces::image_format::nv12_bt709_full, camera_width, camera_height);
            } else if (pixel_format == PIXEL_FORMAT_I420) {
                uint8_t* u_plane_data = y_plane_data + y_plane_size;
                uint8_t* v_plane_data = u_plane_data + camera_width * camera_height / 4;

                using ns = bnb::oep::interfaces::pixel_buffer;
                ns::plane_data y_plane{std::shared_ptr<uint8_t>(y_plane_data, [lock](uint8_t*) {}), 0, static_cast<int32_t>(surface_stride)};
                ns::plane_data u_plane{std::shared_ptr<uint8_t>(u_plane_data, [lock](uint8_t*) {  }), 0, static_cast<int32_t>(surface_stride / 2)};
                ns::plane_data v_plane{std::shared_ptr<uint8_t>(v_plane_data, [lock](uint8_t*) {  }), 0, static_cast<int32_t>(surface_stride / 2)};
                std::vector<ns::plane_data> planes{y_plane, u_plane, v_plane};

                img = ns::create(planes, bnb::oep::interfaces::image_format::i420_bt709_full, camera_width, camera_height);
            } else {
                throw std::runtime_error("Unsupported format from camera");
            }

            if (m_impl->m_push_frame_cb) {
                if (!m_impl->m_camera_is_stopped) {
                    m_impl->m_push_frame_cb(img);
                }
            }
        });
    }

    /* camera::~camera */
    camera::~camera()
    {
        m_impl->m_camera_is_stopped = true;
        m_impl->wrapped->Stop();
    }

    /* camera::set_device_by_index */
    void camera::set_device_by_index(uint32_t index)
    {
        std::cout << "[Camera Win]: Only default camera device supported" << std::endl;
    }

    /* camera::set_device_by_id */
    void camera::set_device_by_id(const std::string& device_id)
    {
        std::cout << "[Camera Win]: Only default camera device supported" << std::endl;
    }

    /* camera::start */
    void camera::start()
    {
        std::cout << "[Camera Win]: Camera starts in constructor" << std::endl;
    }

    /* camera::get_current_device_index */
    size_t camera::get_current_device_index() const
    {
        return m_impl->m_device_index;
    }

} /* namespace bnb::oep */
