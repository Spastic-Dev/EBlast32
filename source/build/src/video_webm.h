// video_webm.h
#ifndef VIDEO_WEBM_H_
#define VIDEO_WEBM_H_

#include "build.h"      // EDuke32 build.h
#include "compat.h"
#include "baselayer.h"  // for window info

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <libwebm/mkvparser.hpp>
#include <libwebm/mkvreader.hpp>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

#ifdef __cplusplus
}
#endif

#define WEBM_MAX_WIDTH  1920
#define WEBM_MAX_HEIGHT 1080

struct WebMVideo
{
    // Core WebM/Matroska parser
    mkvparser::IMkvReader*      reader;
    mkvparser::Segment*         segment;
    const mkvparser::Cluster*   cluster;
    const mkvparser::Block*     block;

    // Video track
    const mkvparser::VideoTrack* video_track;
    vpx_codec_ctx_t             vpx_codec;

    // Current state
    int64_t     current_time_ns;    // presentation timestamp
    int64_t     duration_ns;
    bool        playing;
    bool        loop;
    bool        has_audio;          // (placeholder)

    // Output texture (SDL)
    SDL_Texture*    texture;
    int             tex_w;
    int             tex_h;
    uint8_t*        frame_buffer;   // YUV420 buffer
    size_t          frame_buffer_size;

    // Stats / debug
    int         frame_count;
    double      last_fps_time;
    int         frames_this_second;
};

bool  webm_init(void);
void  webm_shutdown(void);

WebMVideo*  webm_open(const char* filename);
void        webm_close(WebMVideo* vid);

bool  webm_start(WebMVideo* vid, bool should_loop = false);
void  webm_stop(WebMVideo* vid);
void  webm_update(WebMVideo* vid);     // call every frame
void  webm_render(WebMVideo* vid, int x, int y, int w, int h);

bool  webm_is_playing(const WebMVideo* vid);
double webm_get_progress(const WebMVideo* vid); // 0.0 → 1.0

#endif // VIDEO_WEBM_H_
