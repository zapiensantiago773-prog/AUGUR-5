#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace augur::tools
{

// Writes mono 32-bit float WAV (WAVE_FORMAT_IEEE_FLOAT). Offline use only.
// Assumes a little-endian host (x86-64 and arm64 both are).
inline bool writeWavFloatMono (const std::string& path, const std::vector<float>& samples, std::uint32_t sampleRate)
{
    std::ofstream file (path, std::ios::binary);
    if (! file)
        return false;

    const auto put32 = [&] (std::uint32_t v) { file.write (reinterpret_cast<const char*> (&v), 4); };
    const auto put16 = [&] (std::uint16_t v) { file.write (reinterpret_cast<const char*> (&v), 2); };

    const std::uint16_t channels = 1, bitsPerSample = 32, formatFloat = 3;
    const std::uint32_t dataBytes = static_cast<std::uint32_t> (samples.size() * sizeof (float));
    const std::uint32_t blockAlign = channels * bitsPerSample / 8u;

    file.write ("RIFF", 4);
    put32 (36u + dataBytes);
    file.write ("WAVE", 4);
    file.write ("fmt ", 4);
    put32 (16u);
    put16 (formatFloat);
    put16 (channels);
    put32 (sampleRate);
    put32 (sampleRate * blockAlign);
    put16 (static_cast<std::uint16_t> (blockAlign));
    put16 (bitsPerSample);
    file.write ("data", 4);
    put32 (dataBytes);
    file.write (reinterpret_cast<const char*> (samples.data()), static_cast<std::streamsize> (dataBytes));

    return static_cast<bool> (file);
}

} // namespace augur::tools
