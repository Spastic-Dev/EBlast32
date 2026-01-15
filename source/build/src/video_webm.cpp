// video_webm.cpp - minimal WEBM video playback using libwebm + libvpx + SDL2

#include "video_webm.h"
#include "crc32.h"      // optional
#include "cache1d.h"    // findfrompath / openfrompath

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

static bool webm_library_initialized = false;

bool webm_init(void)
{
    if (webm_library_initialized)
        return true;

    // We assume SDL2 is already initialized by the engine

    webm_library_initialized = true;
    return true;
}

void webm_shutdown(void)
{
    // Usually nothing needed - codecs clean up on close
}

static bool webm_init_decoder(WebMVideo* vid)
{
    vpx_codec_iface_t* decoder = nullptr;

    if (vid->video_track->codec_id() == mkvparser::VideoTrack::kVP8)
        decoder = vpx_codec_vp8_dx();
    else if (vid->video_track->codec_id() == mkvparser::VideoTrack::kVP9)
        decoder = vpx_codec_vp9_dx();
    else
    {
        initprintf("WEBM: Unsupported codec (only VP8/VP9 supported)\n");
        return false;
    }

    vpx_codec_err_t res = vpx_codec_dec_init(&vid->vpx_codec, decoder, nullptr, 0);
    if (res != VPX_CODEC_OK)
    {
        initprintf("WEBM: vpx_codec_dec_init failed: %s\n", vpx_codec_err_to_string(res));
        return false;
    }

    return true;
}

WebMVideo* webm_open(const char* filename)
{
    if (!webm_library_initialized)
        webm_init();

    auto* vid = (WebMVideo*)calloc(1, sizeof(WebMVideo));
    if (!vid) return nullptr;

    // EDuke32 style path finding
    int fil = openfrompath(filename, BO_RDONLY|BO_BINARY, 0644);
    if (fil < 0)
    {
        initprintf("WEBM: Cannot open %s\n", filename);
        free(vid);
        return nullptr;
    }

    // Very simple reader using EDuke32 file API
    class EDukeFileReader : public mkvparser::IMkvReader
    {
    public:
        int fil;
        EDukeFileReader(int f) : fil(f) {}
        virtual ~EDukeFileReader() { if (fil >= 0) close(fil); }

        virtual int Read(long long pos, long len, unsigned char* buf) override
        {
            if (lseek(fil, pos, SEEK_SET) == -1) return -1;
            ssize_t r = read(fil, buf, len);
            return (r == len) ? 0 : -1;
        }

        virtual long long GetLength() override
        {
            off_t cur = lseek(fil, 0, SEEK_CUR);
            off_t end = lseek(fil, 0, SEEK_END);
            lseek(fil, cur, SEEK_SET);
            return end;
        }
    };

    vid->reader = new EDukeFileReader(fil);

    mkvparser::EBMLHeader ebmlHeader;
    long long pos = 0;

    if (ebmlHeader.Parse(vid->reader, pos) < 0)
        goto fail;

    vid->segment = new mkvparser::Segment(vid->reader, pos);
    if (!vid->segment)
        goto fail;

    long status = vid->segment->Load();
    if (status < 0)
        goto fail;

    const mkvparser::Tracks* tracks = vid->segment->GetTracks();
    if (!tracks)
        goto fail;

    // Find first video track
    vid->video_track = nullptr;
    for (unsigned i = 0; i < tracks->GetTracksCount(); ++i)
    {
        const mkvparser::Track* track = tracks->GetTrack(i);
        if (track && track->GetType() == mkvparser::Track::kVideo)
        {
            vid->video_track = static_cast<const mkvparser::VideoTrack*>(track);
            break;
        }
    }

    if (!vid->video_track)
    {
        initprintf("WEBM: No video track found in %s\n", filename);
        goto fail;
    }

    if (!webm_init_decoder(vid))
        goto fail;

    vid->duration_ns = vid->segment->GetDuration();
    vid->tex_w = vid->video_track->GetWidth();
    vid->tex_h = vid->video_track->GetHeight();

    // Safety clamp
    if (vid->tex_w > WEBM_MAX_WIDTH || vid->tex_h > WEBM_MAX_HEIGHT)
    {
        initprintf("WEBM: Video too large (%dx%d)\n", vid->tex_w, vid->tex_h);
        goto fail;
    }

    // Create output texture (YUV420)
    vid->texture = SDL_CreateTexture(
        getrendermethod() == REND_OPENGL ? glinfo.sdl_context : nullptr,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        vid->tex_w, vid->tex_h);

    if (!vid->texture)
    {
        initprintf("WEBM: Cannot create SDL texture\n");
        goto fail;
    }

    initprintf("WEBM: Opened %s  %dx%d  %.2fs\n",
               filename, vid->tex_w, vid->tex_h,
               vid->duration_ns / 1e9);

    return vid;

fail:
    webm_close(vid);
    return nullptr;
}

void webm_close(WebMVideo* vid)
{
    if (!vid) return;

    if (vid->vpx_codec.decoder)
        vpx_codec_destroy(&vid->vpx_codec);

    if (vid->texture)
        SDL_DestroyTexture(vid->texture);

    if (vid->segment)
        delete vid->segment;

    if (vid->reader)
        delete vid->reader;

    if (vid->frame_buffer)
        free(vid->frame_buffer);

    free(vid);
}

// Very simplified version - no proper timestamp handling, no audio sync
void webm_update(WebMVideo* vid)
{
    if (!vid || !vid->playing || !vid->segment)
        return;

    if (!vid->cluster)
        vid->cluster = vid->segment->GetFirst();

    while (vid->cluster && !vid->cluster->EOS())
    {
        if (!vid->block)
            vid->block = vid->cluster->GetFirst();

        if (!vid->block || vid->block->EOS())
        {
            vid->cluster = vid->segment->GetNext(*vid->cluster);
            vid->block = nullptr;
            continue;
        }

        const mkvparser::BlockEntry* entry = vid->block;
        const mkvparser::Block* block = entry->GetBlock();

        if (block->GetTrackNumber() == vid->video_track->GetNumber())
        {
            long len;
            const unsigned char* data;
            bool is_key = block->IsKey();

            if (block->GetFrame(0)->GetData(data, len) == 0)
            {
                vpx_codec_err_t res = vpx_codec_decode(
                    &vid->vpx_codec,
                    data, len,
                    nullptr, 0);

                if (res == VPX_CODEC_OK)
                {
                    vpx_codec_iter_t iter = nullptr;
                    vpx_image_t* img = nullptr;

                    while ((img = vpx_codec_get_frame(&vid->vpx_codec, &iter)) != nullptr)
                    {
                        // We got a frame!
                        if (!vid->frame_buffer)
                        {
                            size_t y_size = img->d_w * img->d_h;
                            size_t uv_size = (img->d_w/2) * (img->d_h/2);
                            vid->frame_buffer_size = y_size + uv_size*2;
                            vid->frame_buffer = (uint8_t*)malloc(vid->frame_buffer_size);
                        }

                        // Copy YUV planes
                        uint8_t* dst = vid->frame_buffer;
                        for (unsigned plane = 0; plane < 3; ++plane)
                        {
                            uint8_t* src = img->planes[plane];
                            int stride = img->stride[plane];
                            int w = (plane == 0) ? img->d_w : img->d_w/2;
                            int h = (plane == 0) ? img->d_h : img->d_h/2;

                            for (int y = 0; y < h; ++y)
                            {
                                memcpy(dst, src, w);
                                dst += w;
                                src += stride;
                            }
                        }

                        // Upload to texture
                        SDL_UpdateYUVTexture(vid->texture, nullptr,
                            vid->frame_buffer,
                            img->stride[0],
                            vid->frame_buffer + (img->d_w * img->d_h),
                            img->stride[1],
                            vid->frame_buffer + (img->d_w * img->d_h) + ((img->d_w/2)*(img->d_h/2)),
                            img->stride[2]);

                        vid->frame_count++;
                    }
                }
            }
        }

        // Advance
        vid->block = vid->cluster->GetNext(*vid->block);
        if (!vid->block)
            vid->cluster = vid->segment->GetNext(*vid->cluster);
    }

    // Simple loop
    if (vid->loop && vid->cluster && vid->cluster->EOS())
    {
        vid->cluster = vid->segment->GetFirst();
        vid->block = nullptr;
    }
}

void webm_render(WebMVideo* vid, int x, int y, int w, int h)
{
    if (!vid || !vid->texture || !vid->playing)
        return;

    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(sdl_getrend(), vid->texture, nullptr, &dst);
}

bool webm_start(WebMVideo* vid, bool should_loop)
{
    if (!vid) return false;
    vid->playing = true;
    vid->loop = should_loop;
    vid->cluster = nullptr;
    vid->block = nullptr;
    return true;
}

void webm_stop(WebMVideo* vid)
{
    if (vid) vid->playing = false;
}

bool webm_is_playing(const WebMVideo* vid)
{
    return vid && vid->playing;
}

double webm_get_progress(const WebMVideo* vid)
{
    if (!vid || vid->duration_ns <= 0) return 0.0;
    // Very rough
    return double(vid->frame_count) / (vid->duration_ns / 16666666LL); // ~60fps assumption
}
