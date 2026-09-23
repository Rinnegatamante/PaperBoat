#include "TitleDataFactory.h"
#include "Companion.h"
#include <unordered_map>
#include <sstream>
#include "utils/TextureUtils.h"
#include "utils/Decompressor.h"
#include "spdlog/spdlog.h"

// PM64 title data factory
// Decompresses the Yay0-compressed "title_data" blob and extracts a sub-image
// at the specified sub_offset with the specified size.
//
// Sub-images are byte-addressed (IA8 = 1 byte/pixel, RGBA32 = 4 bytes/pixel)
// so no byte-swapping is needed.

std::optional<std::shared_ptr<IParsedData>> PM64TitleDataFactory::parse(std::vector<uint8_t>& buffer,
                                                                        YAML::Node& node) {
    auto offset = GetSafeNode<uint32_t>(node, "offset");
    auto subOffset = GetSafeNode<uint32_t>(node, "sub_offset");
    auto size = GetSafeNode<uint32_t>(node, "size");

    // Decompress Yay0
    auto decoded = Decompressor::Decode(buffer, offset, CompressionType::YAY0);
    if (!decoded || decoded->size == 0) {
        SPDLOG_ERROR("PM64:TITLE_DATA: Failed to decompress YAY0 at offset 0x{:X}", offset);
        return std::nullopt;
    }

    if (subOffset + size > decoded->size) {
        SPDLOG_ERROR("PM64:TITLE_DATA: Sub-image at 0x{:X} + {} exceeds decompressed size {}", subOffset, size,
                     decoded->size);
        return std::nullopt;
    }

    // Extract the sub-image (no byte-swap needed for IA8/RGBA32)
    std::vector<uint8_t> result(size);
    std::memcpy(result.data(), decoded->data + subOffset, size);

    return std::make_shared<RawBuffer>(result);
}

// Writes a Texture resource next to the resource being exported, so image data that
// the game keeps as a blob can also be addressed by name (a replacement keys on it).
static void RegisterCompanionTexture(const std::string& name, uint32_t type, uint32_t w, uint32_t h, const uint8_t* px,
                                     size_t bytes) {
    auto writer = LUS::BinaryWriter();
    BaseExporter::WriteHeader(writer, Torch::ResourceType::Texture, 0);
    writer.Write(type);
    writer.Write(w);
    writer.Write(h);
    writer.Write((uint32_t)bytes);
    writer.Write((char*)px, bytes);
    std::stringstream ss;
    writer.Finish(ss);
    std::string str = ss.str();
    Companion::Instance->RegisterCompanionFile(name, std::vector<char>(str.begin(), str.end()));
}

static void RegisterImageCompanions(YAML::Node& node, const std::string& entryName, const std::vector<uint8_t>& data) {
    std::string base = entryName;
    const auto slash = base.rfind('/');
    if (slash != std::string::npos) {
        base = base.substr(slash + 1);
    }
    if (node["img_format"]) {
        static const std::unordered_map<std::string, std::pair<TextureType, double>> kFormats = {
            { "RGBA32", { TextureType::RGBA32bpp, 4.0 } },         { "RGBA16", { TextureType::RGBA16bpp, 2.0 } },
            { "CI4", { TextureType::Palette4bpp, 0.5 } },          { "CI8", { TextureType::Palette8bpp, 1.0 } },
            { "I4", { TextureType::Grayscale4bpp, 0.5 } },         { "I8", { TextureType::Grayscale8bpp, 1.0 } },
            { "IA4", { TextureType::GrayscaleAlpha4bpp, 0.5 } },   { "IA8", { TextureType::GrayscaleAlpha8bpp, 1.0 } },
            { "IA16", { TextureType::GrayscaleAlpha16bpp, 2.0 } },
        };
        const auto fmt = GetSafeNode<std::string>(node, "img_format");
        const auto w = GetSafeNode<uint32_t>(node, "img_width");
        const auto h = GetSafeNode<uint32_t>(node, "img_height");
        const auto it = kFormats.find(fmt);
        if (it != kFormats.end()) {
            const size_t bytes = (size_t)(w * h * it->second.second);
            if (bytes <= data.size()) {
                RegisterCompanionTexture(base + "_img", (uint32_t)it->second.first, w, h, data.data(), bytes);
            }
        }
    }
    if (node["tlut_colors"]) {
        const auto colors = GetSafeNode<uint32_t>(node, "tlut_colors");
        std::string img = base;
        for (const char* suffix : { "_palette", "_pal" }) {
            const std::string s = suffix;
            if (img.size() > s.size() && img.compare(img.size() - s.size(), s.size(), s) == 0) {
                img = img.substr(0, img.size() - s.size());
                break;
            }
        }
        if ((size_t)colors * 2 <= data.size()) {
            RegisterCompanionTexture(img + "_img_tlut", (uint32_t)TextureType::RGBA16bpp, colors, 1, data.data(),
                                     (size_t)colors * 2);
        }
    }
}

ExportResult PM64TitleDataBinaryExporter::Export(std::ostream& write, std::shared_ptr<IParsedData> raw,
                                                 std::string& entryName, YAML::Node& node, std::string* replacement) {
    auto writer = LUS::BinaryWriter();
    auto data = std::static_pointer_cast<RawBuffer>(raw)->mBuffer;

    WriteHeader(writer, Torch::ResourceType::Blob, 0);
    writer.Write(static_cast<uint32_t>(data.size()));
    writer.Write(reinterpret_cast<char*>(data.data()), data.size());
    writer.Finish(write);

    RegisterImageCompanions(node, entryName, data);
    return std::nullopt;
}

ExportResult PM64TitleDataHeaderExporter::Export(std::ostream& write, std::shared_ptr<IParsedData> raw,
                                                 std::string& entryName, YAML::Node& node, std::string* replacement) {
    const auto symbol = GetSafeNode(node, "symbol", entryName);

    if (Companion::Instance->IsOTRMode()) {
        write << "static const ALIGN_ASSET(2) char " << symbol << "[] = \"__OTR__" << (*replacement) << "\";\n\n";
        return std::nullopt;
    }

    write << "extern u8 " << symbol << "[];\n";
    return std::nullopt;
}
