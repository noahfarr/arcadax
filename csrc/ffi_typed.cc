#include <cstdint>

#include "xla/ffi/api/ffi.h"

extern "C" {
#include "vecenv.h"
}

namespace ffi = xla::ffi;

static ffi::Error StepImpl(ffi::Buffer<ffi::S64> handle,
                           ffi::Buffer<ffi::S32> actions,
                           ffi::Result<ffi::Buffer<ffi::S8>> obs,
                           ffi::Result<ffi::Buffer<ffi::F32>> reward,
                           ffi::Result<ffi::Buffer<ffi::PRED>> terminated,
                           ffi::Result<ffi::Buffer<ffi::PRED>> truncated,
                           ffi::Result<ffi::Buffer<ffi::S32>> level,
                           ffi::Result<ffi::Buffer<ffi::S32>> score) {
    VecEnv *vec = reinterpret_cast<VecEnv *>(
        static_cast<intptr_t>(handle.typed_data()[0]));
    vecenv_step(vec, actions.typed_data(), obs->typed_data(),
                reward->typed_data(),
                reinterpret_cast<uint8_t *>(terminated->typed_data()),
                reinterpret_cast<uint8_t *>(truncated->typed_data()),
                level->typed_data(), score->typed_data());
    return ffi::Error::Success();
}

XLA_FFI_DEFINE_HANDLER_SYMBOL(ArcadaxStep, StepImpl,
                              ffi::Ffi::Bind()
                                  .Arg<ffi::Buffer<ffi::S64>>()
                                  .Arg<ffi::Buffer<ffi::S32>>()
                                  .Ret<ffi::Buffer<ffi::S8>>()
                                  .Ret<ffi::Buffer<ffi::F32>>()
                                  .Ret<ffi::Buffer<ffi::PRED>>()
                                  .Ret<ffi::Buffer<ffi::PRED>>()
                                  .Ret<ffi::Buffer<ffi::S32>>()
                                  .Ret<ffi::Buffer<ffi::S32>>());

static ffi::Error ResetImpl(ffi::Buffer<ffi::S64> handle,
                            ffi::Result<ffi::Buffer<ffi::S8>> obs) {
    VecEnv *vec = reinterpret_cast<VecEnv *>(
        static_cast<intptr_t>(handle.typed_data()[0]));
    vecenv_reset(vec, obs->typed_data());
    return ffi::Error::Success();
}

XLA_FFI_DEFINE_HANDLER_SYMBOL(ArcadaxReset, ResetImpl,
                              ffi::Ffi::Bind()
                                  .Arg<ffi::Buffer<ffi::S64>>()
                                  .Ret<ffi::Buffer<ffi::S8>>());
