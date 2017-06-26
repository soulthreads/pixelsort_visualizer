#include <cstdio>
#include <string>
#include <algorithm>
#include <thread>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <SDL.h>
#include "pulse_input.h"

cv::Mat gradient(const cv::Mat& source)
{
    cv::Mat result;

    cv::Mat img_blur;
    cv::GaussianBlur(source, img_blur, cv::Size(3, 3), 0, 0, cv::BORDER_DEFAULT);

    cv::Mat img_gray;
    cv::cvtColor(img_blur, img_gray, cv::COLOR_BGR2GRAY);

    cv::Mat grad_x, grad_y;
    cv::Scharr(img_gray, grad_x, CV_16S, 1, 0);
    cv::Scharr(img_gray, grad_y, CV_16S, 0, 1);

    cv::Mat abs_grad_x, abs_grad_y;
    cv::convertScaleAbs(grad_x, abs_grad_x);
    cv::convertScaleAbs(grad_y, abs_grad_y);

    cv::addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0.0, result);
    return result;
}

class ParallelSort : public cv::ParallelLoopBody
{
public:
    ParallelSort(cv::Mat& dst, const cv::Mat& mask, int idx)
        : _dst(dst), _mask(mask), _idx(idx)
    {}

    void operator()(const cv::Range& range) const override
    {
        for (int i = range.start; i < range.end; ++i)
        {
            auto p = _dst.ptr<cv::Vec3b>(i);
            auto mask_p = _mask.ptr<uchar>(i);
            int j = 0;
            while (j < _dst.cols)
            {
                int start = 0;
                int end = 0;

                j = std::find(mask_p+j, mask_p+_dst.cols, 255) - mask_p;
                if (j == _dst.cols)
                    continue;
                start = j;

                j = std::find(mask_p+start, mask_p+_dst.cols, 0) - mask_p;
                end = j;

                std::sort(
                    p+start,
                    p+end,
                    [this](auto& l, auto& r){ return l[_idx] < r[_idx]; }
                );
            }
        }
    }

    ParallelSort& operator=(const ParallelSort&)
    {
        return *this;
    }

private:
    cv::Mat& _dst;
    const cv::Mat& _mask;
    int _idx;
};

void sort_hsv(cv::Mat& mat, const cv::Mat& mask, int idx)
{
    ParallelSort psort(mat, mask, idx);
    cv::parallel_for_(cv::Range(0, mat.rows), psort, 4);
}

void pixel_sort(const cv::Mat& src, cv::Mat& dst, int low, int high, int rotation, int idx)
{
    dst = src.clone();
    if (rotation)
        cv::rotate(dst, dst, rotation-1);

    cv::Mat img_mask;
    cv::Scalar lower(0, low, 0);
    cv::Scalar higher(255, high, 255);
    cv::inRange(dst, lower, higher, img_mask);

    sort_hsv(dst, img_mask, idx);

    if (rotation)
        cv::rotate(dst, dst, 3-rotation);
}

int main(int argc, char* argv[])
{
    const char* keys =
        "{help h |      | print this message}"
        "{@image |<none>| path to image     }"
        "{@source|<none>| pulseaudio source }"
        "{r      |1     | rotate            }"
        "{key    |h     | sorting key       }"
    ;
    cv::CommandLineParser parser(argc, argv, keys);
    if (parser.has("help"))
    {
        parser.printMessage();
        return 0;
    }
    std::string filename = parser.get<std::string>(0);
    std::string source = parser.get<std::string>(1);
    int rotate = parser.get<int>("r");
    std::string key = parser.get<std::string>("key");
    int key_index = 0;
    if (key == "h")
        key_index = 0;
    else if (key == "l")
        key_index = 1;
    else if (key == "s")
        key_index = 2;

    if (!parser.check())
    {
        parser.printErrors();
        return -1;
    }

    cv::Mat img;
    img = cv::imread(filename, cv::IMREAD_COLOR);
    if (img.empty())
    {
        fprintf(stderr, "Cannot read image file\n");
        return -1;
    }
    auto grad = gradient(img);
    cv::Mat img_hsv;
    cv::cvtColor(img, img_hsv, cv::COLOR_BGR2HLS_FULL);

    // sdl part
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    if(SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "Could not init SDL: %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow(
        "Pixel Sort Visualizer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, // whatever
        480, // really
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (window == nullptr)
    {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return -1;
    }
    int r_width, r_height;
    SDL_GetRendererOutputSize(renderer, &r_width, &r_height);

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_BGR24,
        SDL_TEXTUREACCESS_STREAMING,
        img.cols,
        img.rows
    );
    if (texture == nullptr)
    {
        fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
        return -1;
    }
    SDL_UpdateTexture(texture, nullptr, img.data, img.step);

    SDL_Rect out_rect;
    if (img.cols > img.rows)
    {
        out_rect.w = r_width;
        out_rect.h = r_width * img.rows / img.cols;
        out_rect.x = 0;
        out_rect.y = (r_height - out_rect.h) / 2;
    }
    else
    {
        out_rect.w = r_height * img.cols / img.rows;
        out_rect.h = r_height;
        out_rect.x = (r_width - out_rect.w) / 2;
        out_rect.y = 0;
    }

    Input input(source);
    input.start();

    cv::Mat result;
    while (true)
    {
        SDL_Event e;
        if (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                break;
            else if (e.type == SDL_KEYUP &&
                (e.key.keysym.sym == SDLK_ESCAPE ||
                 e.key.keysym.sym == SDLK_q))
                break;
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, &out_rect);
        SDL_RenderPresent(renderer);

        pixel_sort(img_hsv, result, 0, input.level(), rotate, key_index);
        cv::cvtColor(result, result, cv::COLOR_HLS2BGR_FULL);

        void* data = nullptr;
        int pitch = 0;
        SDL_LockTexture(texture, nullptr, &data, &pitch);
        cv::Mat tex_mat(result.rows, result.cols, CV_8UC3, data, pitch);
        result.copyTo(tex_mat);
        SDL_UnlockTexture(texture);
    }

    input.stop();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}
