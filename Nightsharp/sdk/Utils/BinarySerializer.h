#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace SDK::Utils::BinarySerializer {

class BinaryWriter {
public:
    void WriteBytes(const void* data, size_t size) {
        if (!data || size == 0) {
            return;
        }

        const auto* first = static_cast<const uint8_t*>(data);
        m_bytes.insert(m_bytes.end(), first, first + size);
    }

    template<typename T>
    void WritePod(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "BinaryWriter::WritePod requires trivially copyable types.");
        WriteBytes(&value, sizeof(T));
    }

    void WriteString(const std::string& value) {
        const uint32_t size = static_cast<uint32_t>(value.size());
        WritePod(size);
        WriteBytes(value.data(), value.size());
    }

    const std::vector<uint8_t>& Bytes() const {
        return m_bytes;
    }

    std::vector<uint8_t> TakeBytes() {
        return std::move(m_bytes);
    }

private:
    std::vector<uint8_t> m_bytes = {};
};

class BinaryReader {
public:
    explicit BinaryReader(const std::vector<uint8_t>& bytes)
        : m_bytes(bytes) {}

    bool ReadBytes(void* dest, size_t size) {
        if (!dest || size == 0 || (m_offset + size) > m_bytes.size()) {
            return false;
        }

        std::memcpy(dest, m_bytes.data() + m_offset, size);
        m_offset += size;
        return true;
    }

    template<typename T>
    bool ReadPod(T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "BinaryReader::ReadPod requires trivially copyable types.");
        return ReadBytes(&value, sizeof(T));
    }

    bool ReadString(std::string& value) {
        uint32_t size = 0;
        if (!ReadPod(size) || (m_offset + size) > m_bytes.size()) {
            value.clear();
            return false;
        }

        value.assign(reinterpret_cast<const char*>(m_bytes.data() + m_offset), size);
        m_offset += size;
        return true;
    }

    size_t Remaining() const {
        return m_offset <= m_bytes.size() ? (m_bytes.size() - m_offset) : 0;
    }

    bool EndOfStream() const {
        return Remaining() == 0;
    }

private:
    const std::vector<uint8_t>& m_bytes;
    size_t m_offset = 0;
};

template<typename T, typename = void>
struct has_member_serialize : std::false_type {};

template<typename T>
struct has_member_serialize<T, std::void_t<decltype(std::declval<const T&>().Serialize(std::declval<BinaryWriter&>()))>>
    : std::true_type {};

template<typename T, typename = void>
struct has_member_deserialize : std::false_type {};

template<typename T>
struct has_member_deserialize<T, std::void_t<decltype(std::declval<T&>().Deserialize(std::declval<BinaryReader&>()))>>
    : std::true_type {};

template<typename T>
inline void SerializeValue(BinaryWriter& writer, const T& value);

template<typename T>
inline bool DeserializeValue(BinaryReader& reader, T& value);

template<typename T>
inline void SerializeValue(BinaryWriter& writer, const std::vector<T>& values) {
    const uint32_t count = static_cast<uint32_t>(values.size());
    writer.WritePod(count);
    for (const auto& value : values) {
        SerializeValue(writer, value);
    }
}

template<typename T>
inline bool DeserializeValue(BinaryReader& reader, std::vector<T>& values) {
    uint32_t count = 0;
    if (!reader.ReadPod(count)) {
        values.clear();
        return false;
    }

    values.clear();
    values.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        T value{};
        if (!DeserializeValue(reader, value)) {
            values.clear();
            return false;
        }
        values.push_back(std::move(value));
    }
    return true;
}

template<typename T>
inline void SerializeValue(BinaryWriter& writer, const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        writer.WriteString(value);
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        writer.WritePod(static_cast<Underlying>(value));
    } else if constexpr (std::is_trivially_copyable_v<T>) {
        writer.WritePod(value);
    } else if constexpr (has_member_serialize<T>::value) {
        value.Serialize(writer);
    } else {
        static_assert(std::is_trivially_copyable_v<T> || has_member_serialize<T>::value,
            "BinarySerializer::SerializeValue requires a trivially copyable type or member Serialize(BinaryWriter&).");
    }
}

template<typename T>
inline bool DeserializeValue(BinaryReader& reader, T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return reader.ReadString(value);
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        Underlying raw{};
        if (!reader.ReadPod(raw)) {
            value = static_cast<T>(0);
            return false;
        }
        value = static_cast<T>(raw);
        return true;
    } else if constexpr (std::is_trivially_copyable_v<T>) {
        return reader.ReadPod(value);
    } else if constexpr (has_member_deserialize<T>::value) {
        value.Deserialize(reader);
        return true;
    } else {
        static_assert(std::is_trivially_copyable_v<T> || has_member_deserialize<T>::value,
            "BinarySerializer::DeserializeValue requires a trivially copyable type or member Deserialize(BinaryReader&).");
        return false;
    }
}

template<typename T>
inline std::vector<uint8_t> Serialize(const T& value) {
    BinaryWriter writer;
    SerializeValue(writer, value);
    return writer.TakeBytes();
}

template<typename T>
inline T Deserialize(const std::vector<uint8_t>& bytes) {
    BinaryReader reader(bytes);
    T value{};
    DeserializeValue(reader, value);
    return value;
}

template<typename T>
inline bool TryDeserialize(const std::vector<uint8_t>& bytes, T& value) {
    BinaryReader reader(bytes);
    return DeserializeValue(reader, value);
}

inline std::vector<uint8_t> SerializeString(const std::string& value) {
    return Serialize(value);
}

inline std::string DeserializeString(const std::vector<uint8_t>& bytes) {
    return Deserialize<std::string>(bytes);
}

template<typename T>
inline bool SerializeToFile(const std::string& path, const T& value) {
    const auto bytes = Serialize(value);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.good()) {
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return stream.good();
}

template<typename T>
inline bool DeserializeFromFile(const std::string& path, T& value) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.good()) {
        return false;
    }

    stream.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes(size);
    if (size != 0) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    if (!stream.good() && !stream.eof()) {
        return false;
    }
    return TryDeserialize(bytes, value);
}

} // namespace SDK::Utils::BinarySerializer
