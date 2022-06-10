#pragma once

#include <interfaces/pixel_buffer.hpp>

namespace bnb::oep
{
	class camera;
}

using camera_sptr = std::shared_ptr<bnb::oep::camera>;

namespace bnb::oep
{

    class camera
    {
    public:
        using push_frame_cb_t = std::function<void(pixel_buffer_sptr)>;

    public:
        explicit camera(const push_frame_cb_t& cb, size_t index);

        ~camera();

        void set_device_by_index(uint32_t index);

        void set_device_by_id(const std::string& device_id);

        void start();

        size_t get_current_device_index() const;

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    }; /* class camera */

} /* namespace bnb::oep */
