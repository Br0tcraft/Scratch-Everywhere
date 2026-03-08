#include "blockUtils.hpp"
#include <render.hpp>
#include <sprite.hpp>
#include <value.hpp>

constexpr unsigned int MAX_LIST_ITEMS = 200000;

SCRATCH_BLOCK(data, setvariableto) {
    Value input;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, input)) return BlockResult::REPEAT;
    BlockExecutor::setVariableValue(Scratch::getFieldId(*block, "VARIABLE"), input, sprite);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, changevariableby) {
    Value input;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, input)) return BlockResult::REPEAT;
    const std::string varId = Scratch::getFieldId(*block, "VARIABLE");
    BlockExecutor::setVariableValue(varId, input + BlockExecutor::getVariableValue(varId, sprite), sprite);
    Scratch::resetInput(block, "VALUE");
    return BlockResult::CONTINUE;
}