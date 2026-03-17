// Demonstrates chaining multiple hooks with priority to build a
// transformation pipeline. Each hook transforms the data before
// it reaches the next.
//
// This pattern is useful when you want composable, independently
// registered transformations rather than one monolithic function.
// Each transformation is unaware of the others and can be added,
// removed, or reordered without touching any other code.

#include <algorithm>
#include <cmath>
#include <print>
#include <splice/splice.hpp>
#include <string>

class AudioProcessor
{
public:
  [[= splice::hook::hookable { }]] float processSample(float sample) { return sample; }
};

SPLICE_HOOK_REGISTRY(AudioProcessor, g_audio);

void setupPipeline()
{
  // Stage 1 (runs first: Highest priority): clamp input to valid range.
  g_audio->modify_arg<^^AudioProcessor::processSample, 0>(
      [](float sample) -> float
      {
        float clamped = std::clamp(sample, -1.0f, 1.0f);
        std::println("  [clamp]      {:.4f} -> {:.4f}", sample, clamped);
        return clamped;
      },
      splice::hook::Priority::Highest);

  // Stage 2: apply a simple gain.
  g_audio->modify_arg<^^AudioProcessor::processSample, 0>(
      [](float sample) -> float
      {
        constexpr float gain = 0.8f;
        float gained = sample * gain;
        std::println("  [gain]       {:.4f} -> {:.4f}", sample, gained);
        return gained;
      },
      splice::hook::Priority::High);

  // Stage 3: apply soft clipping to smooth out peaks.
  g_audio->modify_arg<^^AudioProcessor::processSample, 0>(
      [](float sample) -> float
      {
        float clipped = std::tanh(sample);
        std::println("  [soft clip]  {:.4f} -> {:.4f}", sample, clipped);
        return clipped;
      },
      splice::hook::Priority::Normal);

  // Stage 4 (runs last: on Return): normalise the output.
  g_audio->modify_return<^^AudioProcessor::processSample>(
      [](float result) -> float
      {
        float normalised = result * 0.95f;
        std::println("  [normalise]  {:.4f} -> {:.4f}", result, normalised);
        return normalised;
      },
      splice::hook::Priority::Lowest);
}

int main()
{
  setupPipeline();

  AudioProcessor processor;

  std::println("--- sample within range ---");
  float out1 = g_audio->dispatch<^^AudioProcessor::processSample>(&processor, 0.5f);
  std::println("  final: {:.4f}\n", out1);

  std::println("--- sample out of range ---");
  float out2 = g_audio->dispatch<^^AudioProcessor::processSample>(&processor, 2.5f);
  std::println("  final: {:.4f}", out2);
}
