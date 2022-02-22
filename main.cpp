#include <interfaces/offscreen_effect_player.hpp>
#include <interfaces/camera.hpp>

#include "render_context.hpp"
#include "effect_player.hpp"

#include "oep/opengl/yuv_converter.hpp"

#define BNB_CLIENT_TOKEN <#Place your token here#>

enum class show_plane
{
    show_original,
    show_y_plane,
    show_u_plane,
    show_v_plane
};

int main()
{
    // Frame size
    constexpr int32_t oep_width = 1280;
    constexpr int32_t oep_height = 720;

    std::shared_ptr<glfw_window> window = nullptr; // Should be declared here to destroy in the last turn

    // Create instance of render_context.
    auto rc = bnb::oep::interfaces::render_context::create();

    // Create an instance of our offscreen_render_target implementation, you can use your own.
    // pass render_context
    auto ort = bnb::oep::interfaces::offscreen_render_target::create(rc);

    // Create an instance of effect_player implementation with cpp api, pass path to location of
    // effects and client token
    auto ep = bnb::oep::interfaces::effect_player::create({BNB_RESOURCES_FOLDER}, BNB_CLIENT_TOKEN);

    // Create instance of offscreen_effect_player, pass effect_player, offscreen_render_target
    // and dimension of processing frame (for best performance it is better to coincide
    // with camera frame dimensions)
    auto oep = bnb::oep::interfaces::offscreen_effect_player::create(ep, ort, oep_width, oep_height);

    static show_plane show {show_plane::show_original}; /* what texture to draw */
    static bnb::oep::converter::yuv_converter* converter {nullptr}; /* converter */
    static bnb::oep::converter::yuv_converter::yuv_data yuv_data; /* save converted data to this variable */

    // Make glfw_window and render_thread only for show result of OEP
    // We want to share resources between context, we know that render_context is based on
    // GLFW and returned context is GLFWwindow
    window = std::make_shared<glfw_window>("OEP Example", reinterpret_cast<GLFWwindow*>(rc->get_sharing_context()));
    render_t_sptr render_t = std::make_shared<bnb::render::render_thread>(window->get_window(), oep_width, oep_height);
    auto key_func = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action != GLFW_PRESS) {
            return;
        } else if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_O) {
            show = show_plane::show_original;
        } else if (key == GLFW_KEY_Y) {
            show = show_plane::show_y_plane;
        } else if (key == GLFW_KEY_U) {
            show = show_plane::show_u_plane;
        } else if (key == GLFW_KEY_V) {
            show = show_plane::show_v_plane;
        }
    };
    glfwSetKeyCallback(window->get_window(), key_func);

    oep->load_effect("effects/Afro");

    // Create and run instance of camera, pass callback for frames
    // Callback for received frame from the camera
    auto ef_cb = [&oep, render_t](pixel_buffer_sptr image) {
        // Callback for received pixel buffer from the offscreen effect player
        auto get_pixel_buffer_callback = [render_t](image_processing_result_sptr result) {
            if (result != nullptr) {
                // Callback for update data in render thread
                auto render_callback = [render_t](std::optional<rendered_texture_t> texture_id) {
                    if (texture_id.has_value()) {
                        auto gl_texture = static_cast<GLuint>(reinterpret_cast<int64_t>(*texture_id));

                        if (show == show_plane::show_original) {
                            render_t->update_data(gl_texture);
                            return;
                        }

                        /* create yuv converter once */
                        if (converter == nullptr) {
                            converter = new bnb::oep::converter::yuv_converter(
                                bnb::oep::converter::yuv_converter::standard::bt601,
                                bnb::oep::converter::yuv_converter::range::video_range,
                                bnb::oep::converter::yuv_converter::rotation::deg_0,
                                true,
                                bnb::oep::converter::yuv_converter::yuv_data_layout::planar_layout);
                        }

                        /* convert to yuv */
                        GLint gl_texture_width;
                        GLint gl_texture_height;
                        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &gl_texture_width);
                        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &gl_texture_height);
                        converter->convert(gl_texture, gl_texture_width, gl_texture_height, yuv_data);

                        /* get converted yuv planes */
                        uint8_t* y_plane_data = yuv_data.y_plane_data;
                        uint8_t* u_plane_data = yuv_data.u_plane_data;
                        uint8_t* v_plane_data = yuv_data.v_plane_data;
                        int y_plane_stride = yuv_data.y_plane_stride;
                        int u_plane_stride = yuv_data.u_plane_stride;
                        int v_plane_stride = yuv_data.v_plane_stride;

                        /* calculate size of planes */
                        int y_plane_width = converter->get_width();
                        int y_plane_height = converter->get_height();
                        int u_plane_width = converter->get_width() / 2;
                        int u_plane_height = converter->get_height() / 2;
                        int v_plane_width = converter->get_width() / 2;
                        int v_plane_height = converter->get_height() / 2;

                        /* initialize OpenGL texture once */
                        static GLuint show_texture = 0;
                        if (static bool texture_is_init = false; !texture_is_init) {
                            glGenTextures(1, &show_texture);
                            texture_is_init = true;
                        }

                        /* load Y, U or V plane to OpenGL texture */
                        switch (show) {
                            case show_plane::show_y_plane:
                                glBindTexture(GL_TEXTURE_2D, show_texture);
                                glPixelStorei(GL_UNPACK_ROW_LENGTH, y_plane_stride);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, y_plane_width, y_plane_height, 0, GL_RED, GL_UNSIGNED_BYTE, y_plane_data);
                                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                break;

                            case show_plane::show_u_plane:
                                glBindTexture(GL_TEXTURE_2D, show_texture);
                                glPixelStorei(GL_UNPACK_ROW_LENGTH, u_plane_stride);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, u_plane_width, u_plane_height, 0, GL_RED, GL_UNSIGNED_BYTE, u_plane_data);
                                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                break;

                            case show_plane::show_v_plane:
                                glBindTexture(GL_TEXTURE_2D, show_texture);
                                glPixelStorei(GL_UNPACK_ROW_LENGTH, v_plane_stride);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, v_plane_width, v_plane_height, 0, GL_RED, GL_UNSIGNED_BYTE, v_plane_data);
                                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                break;

                            default:
                                break;
                        }

                        render_t->update_data(show_texture);
                    }
                };
                // Get texture id from shared context and render it
                result->get_texture(render_callback);
            }
        };

        oep->process_image_async(image, bnb::oep::interfaces::rotation::deg0, get_pixel_buffer_callback, bnb::oep::interfaces::rotation::deg180);
    };
    auto m_camera_ptr = bnb::oep::interfaces::camera::create(ef_cb, 0);

    std::weak_ptr<bnb::oep::interfaces::offscreen_effect_player> oep_w = oep;
    render_t_wptr r_w = render_t;

    window->set_resize_callback([oep_w, r_w](int32_t w, int32_t h, int32_t w_glfw_buffer, int32_t h_glfw_buffer) {
        if (auto r_s = r_w.lock()) {
            r_s->surface_changed(w_glfw_buffer, h_glfw_buffer);
        }
        if (auto oep_s = oep_w.lock()) {
            oep_s->surface_changed(w, h);
        }
    });
    window->show(oep_width, oep_height);
    window->run_main_loop();

    return 0;
}
