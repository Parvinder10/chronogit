#include "chronogit/core/sha256.hpp"
#include "chronogit/core/error.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#endif

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace chronogit {

struct SHA256::Impl {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg_handle = nullptr;
    BCRYPT_HASH_HANDLE hash_handle = nullptr;

    Impl() {
        NTSTATUS status = BCryptOpenAlgorithmProvider(&alg_handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (status < 0) {
            throw ChronoGitException("Failed to open BCrypt SHA256 provider");
        }
        status = BCryptCreateHash(alg_handle, &hash_handle, nullptr, 0, nullptr, 0, 0);
        if (status < 0) {
            BCryptCloseAlgorithmProvider(alg_handle, 0);
            throw ChronoGitException("Failed to create BCrypt SHA256 hash");
        }
    }

    ~Impl() {
        if (hash_handle) {
            BCryptDestroyHash(hash_handle);
        }
        if (alg_handle) {
            BCryptCloseAlgorithmProvider(alg_handle, 0);
        }
    }

    void update(const uint8_t* data, size_t length) {
        if (length == 0) return;
        NTSTATUS status = BCryptHashData(hash_handle, const_cast<PUCHAR>(data), static_cast<ULONG>(length), 0);
        if (status < 0) {
            throw ChronoGitException("BCryptHashData failed");
        }
    }

    ObjectId finalize() {
        uint8_t digest[32];
        NTSTATUS status = BCryptFinishHash(hash_handle, digest, sizeof(digest), 0);
        if (status < 0) {
            throw ChronoGitException("BCryptFinishHash failed");
        }
        std::ostringstream oss;
        for (int i = 0; i < 32; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return oss.str();
    }
#endif
};

SHA256::SHA256() : impl_(std::make_unique<Impl>()) {}
SHA256::~SHA256() = default;
SHA256::SHA256(SHA256&&) noexcept = default;
SHA256& SHA256::operator=(SHA256&&) noexcept = default;

void SHA256::update(std::span<const uint8_t> data) {
    impl_->update(data.data(), data.size());
}

void SHA256::update(std::string_view data) {
    impl_->update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

ObjectId SHA256::finalize() {
    return impl_->finalize();
}

ObjectId SHA256::hash(std::span<const uint8_t> data) {
    SHA256 hasher;
    hasher.update(data);
    return hasher.finalize();
}

ObjectId SHA256::hash(std::string_view data) {
    SHA256 hasher;
    hasher.update(data);
    return hasher.finalize();
}

} // namespace chronogit
