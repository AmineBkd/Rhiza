#pragma once

namespace Rhiza
{

enum class TextureFilter
{
    Linear,
    Nearest,
};

enum class TextureWrap
{
    Clamp,
    Repeat,
};

struct TextureDesc
{
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Clamp;
};

}  // namespace Rhiza
