#include "pulse_input.h"
#include <cstdio>
#include <cmath>

#include <pulse/simple.h>
#include <pulse/error.h>

#include <algorithm>

constexpr int k_samplerate = 44100;
constexpr int k_channels = 2;
constexpr size_t k_bufsize = k_samplerate * k_channels * 2 * 0.016;

Input::Input(const std::string& source_name)
{
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = k_samplerate;
    ss.channels = k_channels;

    int error;
    _s = pa_simple_new(
        nullptr,
        "whatever",
        PA_STREAM_RECORD,
        source_name.c_str(),
        "Pixel Sort Recording",
        &ss,
        nullptr,
        nullptr,
        &error
    );
    if (_s == nullptr)
    {
        throw std::runtime_error(pa_strerror(error));
    }
}

Input::~Input()
{
    stop();
    if (_s)
        pa_simple_free(_s);
}

void Input::start()
{
    stop();

    _running = true;
    _thread = std::thread(&Input::loop, this);
}

void Input::stop()
{
    _running = false;
    if (_thread.joinable())
        _thread.join();
}

void Input::loop()
{
    uint8_t buf[k_bufsize];
    int error;
    while(_running)
    {
        if (pa_simple_read(_s, buf, k_bufsize, &error) < 0)
        {
            throw std::runtime_error(pa_strerror(error));
        }
        // Do something with this data;
        auto max = std::max_element((int16_t*)(buf), (int16_t*)(buf+k_bufsize));
        // double dB = 20 * log10(*max / 32767.0);
        // _level = std::max(0.0, std::min((dB + 25) * 10.2, 255.0));
        _level = (*max / 32767.0) * 255;
    }
}
