#pragma once

#include <blockExecutor.hpp>
#include <runtime.hpp>
#include <sprite.hpp>

BlockResult nopBlock(Block &block, Sprite *sprite, ScriptThread *thread);

/**
 * @brief Defines and registers a block
 *
 * This macro uses static variables to automatically register the defined block when the runtime is loaded. The category and the id are concatenated with an underscore separator to form the opcode.
 * When using this macro you do not need a separate declaration or header file since the macro automatically handles the registration with BlockExecutor.
 *
 * @param category The category this block is in.
 * This forms the first half of the block's opcode.
 * @param id The id of the block without the category.
 * This forms the second half of the block's opcode.
 *
 * @section Handler Function Definition
 * The code block directly following the macro is the body of the handler function.
 *
 * @param block A reference to the Block being ran. This is used to fetch inputs/fields and track repeats.
 * @param sprite A pointer to the Sprite the block is being ran in. This is used for actions like motion and data changes.
 * @param withoutScreenRefresh A pointer to a boolean representing if the block is running without refreshing the screen.
 * @param fromRepeat A boolean representing whether or not the block being ran is inside of a repeat.
 *
 * @return BlockResult
 *
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK(category, id)                                                                                                    \
    BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue);                                        \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::getHandlers()[#category "_" #id] = block_##category##_##id##_, 0); \
    BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue)

#define SCRATCH_BLOCK_WITHOUT_ID(category)                                                                                                    \
    BlockResult block_##category##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue);                                        \
    static uint8_t block_##category##_reg_ = (BlockExecutor::getHandlers()[#category] = block_##category##_, 0); \
    BlockResult block_##category##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue)