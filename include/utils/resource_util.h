#ifndef RESOURCE_UTIL_H
#define RESOURCE_UTIL_H

#include "krtl/common.h"

template <class _Ty, void (*_Dx)(_Ty)>
class ResourceGuard {
   public:
    explicit ResourceGuard(_Ty resource = _Ty{})
        : resource_(resource) {}

    ~ResourceGuard() {
        if (resource_ != _Ty{}) _Dx(resource_);
    }

    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;

    ResourceGuard(ResourceGuard&& other) noexcept
        : resource_(other.resource_) {
        other.resource_ = _Ty{};
    }

    ResourceGuard& operator=(ResourceGuard&& other) noexcept {
        if (this != &other) {
            reset();
            resource_ = other.resource_;
            other.resource_ = T{};
        }
        return *this;
    }

    void reset(_Ty new_resource_ = _Ty{}) {
        if (resource_ != _Ty{}) _Dx(resource_);
        resource_ = new_resource_;
    }

    _Ty get() const { return resource_; }
    explicit operator bool() const { return resource_ != _Ty{}; }
    operator const _Ty&() const { return resource_; }
    _Ty* operator&() { return &resource_; }

   private:
    _Ty resource_;
};

#endif