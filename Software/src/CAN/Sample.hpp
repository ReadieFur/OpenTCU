#pragma once

namespace ReadieFur::OpenTCU::CAN
{
    template <typename T, typename TAverage = T>
    class Sample
    {
    private:
        size_t _capacity;
        T* _samples;
        size_t _index = 0;

    public:
        Sample(size_t capacity) : _capacity(capacity), _samples(new T[capacity]) {}

        ~Sample()
        {
            delete[] _samples;
        }

        inline void Add(T sample)
        {
            // Add samples to the array, and remove the oldest sample if the array is full.
            if (_index >= _capacity)
            {
                for (size_t i = 1; i < _capacity; i++)
                    _samples[i - 1] = _samples[i];
                _samples[_capacity - 1] = sample;
            }
            else
            {
                _samples[_index++] = sample;
            }
        }

        T Average()
        {
            TAverage sum = 0;
            for (size_t i = 0; i < _index; i++)
                sum += _samples[i];
            if (sum == 0)
                return 0;
            return sum / _index;
        }

        T Latest()
        {
            if (_index == 0)
                return 0;
            return _samples[_index - 1];
        }
    };
};
