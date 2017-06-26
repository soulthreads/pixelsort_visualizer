#ifndef PULSE_INPUT_H
#define PULSE_INPUT_H

#include <string>
#include <thread>
#include <cstdint>

class Input
{
public:
    Input(const std::string& source_name);
    ~Input();

    void start();
    void stop();

    uint8_t level() const { return _level; };

private:
    struct pa_simple* _s = nullptr;
    bool _running = false;
    std::thread _thread;
    uint8_t _level = 0;

    void loop();
};

#endif // PULSE_INPUT_H
