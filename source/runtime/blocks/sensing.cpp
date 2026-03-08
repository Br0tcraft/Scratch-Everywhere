#include "blockUtils.hpp"
#include <input.hpp>
#include <sprite.hpp>

SCRATCH_BLOCK(sensing, resettimer) {
    BlockExecutor::timer.start();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, timer) {
    *outValue = Value(BlockExecutor::timer.getTimeMs() / 1000.0);
    return BlockResult::CONTINUE;
}