#include "blockExecutor.hpp"
#include "math.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <input.hpp>
#include <iterator>
#include <os.hpp>
#include <ratio>
#include <render.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>
#include <utility>
#include <vector>

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>

extern std::unique_ptr<MistConnection> cloudConnection;
#endif

Timer BlockExecutor::timer;
int BlockExecutor::dragPositionOffsetX;
int BlockExecutor::dragPositionOffsetY;

std::unordered_map<std::string, BlockFunc> &BlockExecutor::getHandlers() {
    static std::unordered_map<std::string, BlockFunc> handlers;
    return handlers;
}

ScriptThread *BlockExecutor::startThread(Sprite *sprite, Block *block) {
    ScriptThread *newThread = nullptr;
    if (Pools::threads.empty()) newThread = new ScriptThread();
    else {
        newThread = Pools::threads.back();
        Pools::threads.pop_back();
    }
    newThread->blockHat = block;
    newThread->nextBlock = block;
    newThread->finished = false;
    sprite->pendingThreads.push_back(newThread);
    return newThread;
}

void BlockExecutor::runThreads() {
    if (!Scratch::pendingSprites.empty()) {
        for (auto &sprite : Scratch::pendingSprites) {
            Scratch::addCloneBehind(sprite.second, sprite.first);
        }
        Scratch::pendingSprites.clear();
    }
    Scratch::sprites.erase(
        std::remove_if(Scratch::sprites.begin(), Scratch::sprites.end(),
                       [](Sprite *s) { return s->toDelete; }),
        Scratch::sprites.end());
    for (auto &sprite : Scratch::sprites) {
        if (!sprite->pendingThreads.empty()) {
            sprite->threads.insert(sprite->threads.end(), sprite->pendingThreads.begin(), sprite->pendingThreads.end());
            sprite->pendingThreads.clear();
        }
        auto it = sprite->threads.begin();
        while (it != sprite->threads.end()) {
            ScriptThread *thread;
            BlockResult var;
            thread = *it;

            if (thread->finished) {
                thread->clear();
                Pools::threads.push_back(thread);
                it = sprite->threads.erase(it);
                continue;
            }
            var = runThread(*thread, *sprite, nullptr);
            if (Scratch::shouldStop) return;
            ++it;
        }
    }
    return;
}

BlockResult BlockExecutor::runThread(ScriptThread &thread, Sprite &sprite, Value *outValue) {
    if (thread.nextBlock == nullptr) return BlockResult::RETURN;
    BlockResult var = BlockResult::CONTINUE;
    do {
        Block *currentBlock = thread.nextBlock;
        thread.nextBlock = currentBlock->nextBlock;

        var = currentBlock->blockFunction(currentBlock, &thread, &sprite, outValue);//runBlock(currentBlock, thread, sprite, outValue);
        if (var == BlockResult::REPEAT) thread.nextBlock = currentBlock;

    } while ((var == BlockResult::CONTINUE_IMIDIATELY || (thread.withoutScreenRefresh && var == BlockResult::CONTINUE)) && !thread.finished && thread.nextBlock != nullptr && !Scratch::shouldStop);
    return var;
}

//old thingy  
/*BlockResult BlockExecutor::runBlock(Block *block, ScriptThread &thread, Sprite &sprite, Value *outValue) {
    bool finished = true;
    BlockResult executedBlock;
    do {
        bool finished = true;
        for (auto &input : block->inputs) {
            if (input.second.needed && !input.second.calculated) {

                if (Scratch::getInput(block, input.first, &thread, &sprite, input.second.value)) {
                    input.second.calculated = true;
                    input.second.needed = false;
                } else {
                    finished = false;
                }
                if (Scratch::shouldStop) return BlockResult::CONTINUE;
            }
            if (!finished) return BlockResult::REPEAT;
        }

    } while ((executedBlock = block->blockFunction(block, &thread, &sprite, outValue)) == BlockResult::GET_INPUTS);
    return executedBlock;
}*/

void BlockExecutor::runAllBlocksByOpcode(const std::string &opcode, std::vector<ScriptThread *> *out) {
    for (auto &sprite : Scratch::sprites) {
        if (sprite->hats[opcode].empty()) continue;
        for (auto &hat : sprite->hats[opcode]) {
            ScriptThread *thread = BlockExecutor::startThread(sprite, hat);
            if (out) out->push_back(thread);
        }
    }
}
void BlockExecutor::runAllBlocksByOpcodeInSprite(const std::string &opcode, Sprite *sprite, std::vector<ScriptThread *> *out) {
    if (sprite->hats[opcode].empty()) return;
    for (auto &hat : sprite->hats[opcode]) {
        ScriptThread *thread = BlockExecutor::startThread(sprite, hat);
        if (out) out->push_back(thread);
    }
}
void BlockExecutor::executeKeyHats() {
    for (const auto &key : Input::keyHeldDuration) {
        if (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), key.first) == Input::inputButtons.end()) {
            Input::keyHeldDuration[key.first] = 0;
        } else {
            Input::keyHeldDuration[key.first]++;
        }
    }

    for (const auto &key : Input::inputButtons) {
        if (Input::keyHeldDuration.find(key) == Input::keyHeldDuration.end()) Input::keyHeldDuration[key] = 1;

        if (key == "any" || Input::keyHeldDuration[key] != 1) continue;

        Input::codePressedBlockOpcodes.clear();
        std::string addKey = (key.find(' ') == std::string::npos) ? key : key.substr(0, key.find(' '));
        std::transform(addKey.begin(), addKey.end(), addKey.begin(), ::tolower);
        Input::inputBuffer.push_back(addKey);
        if (Input::inputBuffer.size() == 101) Input::inputBuffer.erase(Input::inputBuffer.begin());
    }

    const std::vector<Sprite *> sprToRun = Scratch::sprites;
    for (Sprite *currentSprite : sprToRun) {
        BlockExecutor::runAllBlocksByOpcodeInSprite("event_whenkeypressed", currentSprite);
        BlockExecutor::runAllBlocksByOpcodeInSprite("makeymakey_whenMakeyKeyPressed", currentSprite);
        // TODO: Add a way to register these with macros
        // if (data.opcode == "event_whenkeypressed") {
        //    std::string key = Scratch::getFieldValue(data, "KEY_OPTION");
        //    if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && (Input::keyHeldDuration.find(key)->second == 1 || Input::keyHeldDuration.find(key)->second > 15 * (Scratch::FPS / 30.0f)))
        //        executor.runBlock(data, currentSprite);
        //} else if (data.opcode == "makeymakey_whenMakeyKeyPressed") {
        //    std::string key = Input::convertToKey(Scratch::getInputValue(data, "KEY", currentSprite), true);
        //    if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && Input::keyHeldDuration.find(key)->second > 0)
        //        executor.runBlock(data, currentSprite);
        //}
    }
    BlockExecutor::runAllBlocksByOpcode("makeymakey_whenCodePressed");
}

void BlockExecutor::doSpriteClicking() {
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;
        bool hasClicked = false;
        for (auto &sprite : Scratch::sprites) {
            if (!sprite->visible || sprite->ghostEffect == 100.0) continue;

            // click a sprite
            if (sprite->shouldDoSpriteClick) {
                if (Input::mousePointer.heldFrames < 2 && Scratch::isColliding("mouse", sprite)) {

                    // run all "when this sprite clicked" blocks in the sprite
                    hasClicked = true;
                    BlockExecutor::runAllBlocksByOpcodeInSprite("event_whenthisspriteclicked", sprite);
                }
            }
            // start dragging a sprite
            if (Input::draggingSprite == nullptr && Input::mousePointer.heldFrames < 2 && sprite->draggable && Scratch::isColliding("mouse", sprite)) {
                Input::draggingSprite = sprite;
                dragPositionOffsetX = Input::mousePointer.x - sprite->xPosition;
                dragPositionOffsetY = Input::mousePointer.y - sprite->yPosition;
            }
            if (hasClicked) break;
        }
    } else {
        Input::mousePointer.heldFrames = 0;
    }

    // move a dragging sprite
    if (Input::draggingSprite == nullptr) return;

    if (Input::mousePointer.heldFrames == 0) {
        Input::draggingSprite = nullptr;
        return;
    }
    Input::draggingSprite->xPosition = Input::mousePointer.x - dragPositionOffsetX;
    Input::draggingSprite->yPosition = Input::mousePointer.y - dragPositionOffsetY;
}

void BlockExecutor::setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite) {
    // Set sprite variable
    const auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        it->second.value = newValue;
        return;
    }

    auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        globalIt->second.value = newValue;
#ifdef ENABLE_CLOUDVARS
        if (globalIt->second.cloud) cloudConnection->set(globalIt->second.name, globalIt->second.value.asString());
#endif
        return;
    }
}

// TODO: This absolutely needs to be revised. It's currently a mess and needs to be cleaned up.
void BlockExecutor::updateMonitors() {
    for (auto &var : Render::visibleVariables) {
        if (var.visible) {
            Sprite *sprite = nullptr;
            for (auto &spr : Scratch::sprites) {
                if (var.spriteName == "" && spr->isStage) {
                    sprite = spr;
                    break;
                }
                if (spr->name == var.spriteName && !spr->isClone) {
                    sprite = spr;
                    break;
                }
            }

            if (var.opcode == "data_variable") {
                var.value = BlockExecutor::getVariableValue(var.id, sprite);
                var.displayName = Math::removeQuotations(var.parameters["VARIABLE"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;
            } else if (var.opcode == "data_listcontents") {
                var.displayName = Math::removeQuotations(var.parameters["LIST"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;

                // Check lists
                auto listIt = sprite->lists.find(var.id);
                if (listIt != sprite->lists.end())
                    var.list = listIt->second.items;

                // Check global lists
                auto globalIt = Scratch::stageSprite->lists.find(var.id);
                if (globalIt != Scratch::stageSprite->lists.end())
                    var.list = globalIt->second.items;
            } else {
                try {
                    Value outValue;
                    Block newBlock;
                    newBlock.opcode = var.opcode;
                    newBlock.blockFunction = BlockExecutor::getHandlers()[var.opcode];
                    for (const auto &[paramName, paramValue] : var.parameters) {
                        ParsedField parsedField;
                        parsedField.value = Math::removeQuotations(paramValue);
                        newBlock.fields[paramName] = parsedField;
                    }
                    if (var.opcode == "looks_costumenumbername") {
                        var.displayName = var.spriteName + ": costume " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                        ScriptThread thread;
                        BlockResult costume = newBlock.blockFunction(&newBlock, &thread, sprite, &outValue);
                        if (costume == BlockResult::CONTINUE) {
                            var.value = outValue;
                        } else if (var.value == Value())
                            var.value = Value("Fetching...");
                    } else if (var.opcode == "looks_backdropnumbername") {
                        var.displayName = "Stage: backdrop " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                        ScriptThread thread;
                        BlockResult backdrop = newBlock.blockFunction(&newBlock, &thread, sprite, &outValue);
                        if (backdrop == BlockResult::CONTINUE)
                            var.value = outValue;
                        else if (var.value == Value())
                            var.value = Value("Fetching...");
                    } else if (var.opcode == "sensing_current") {
                        var.displayName = std::string(MonitorDisplayNames::getCurrentMenuMonitorName(Scratch::getFieldValue(newBlock, "CURRENTMENU")));
                        ScriptThread thread;
                        BlockResult current = newBlock.blockFunction(&newBlock, &thread, sprite, &outValue);
                        if (current == BlockResult::CONTINUE)
                            var.value = outValue;
                        else if (var.value == Value())
                            var.value = Value("Fetching...");
                    } else {
                        auto spriteName = MonitorDisplayNames::getSpriteMonitorName(var.opcode);
                        if (spriteName != var.opcode) {
                            var.displayName = var.spriteName + ": " + std::string(spriteName);
                        } else {
                            auto simpleName = MonitorDisplayNames::getSimpleMonitorName(var.opcode);
                            var.displayName = simpleName != var.opcode ? std::string(simpleName) : var.opcode;
                        }
                        static ScriptThread thread;
                        BlockResult result = newBlock.blockFunction(&newBlock, &thread, sprite, &outValue);
                        if (result == BlockResult::CONTINUE)
                            var.value = outValue;
                        else if (var.value == Value())
                            var.value = Value("Fetching...");
                    }
                } catch (...) {
                    var.value = Value("Unknown...");
                }
            }
        }
    }
}

Value BlockExecutor::getVariableValue(std::string variableId, Sprite *sprite) {
    // Check sprite variables
    const auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) return it->second.value;

    // Check lists
    const auto listIt = sprite->lists.find(variableId);
    if (listIt != sprite->lists.end()) {
        std::string result;
        std::string seperator = "";
        for (const auto &item : listIt->second.items) {
            if (item.asString().size() > 1 || !item.isString()) {
                seperator = " ";
                break;
            }
        }
        for (const auto &item : listIt->second.items) {
            result += item.asString() + seperator;
        }
        if (!result.empty() && !seperator.empty()) result.pop_back();
        return Value(result);
    }

    // Check global variables
    for (const auto &currentSprite : Scratch::sprites) {
        if (currentSprite->isStage) {
            const auto globalIt = currentSprite->variables.find(variableId);
            if (globalIt != currentSprite->variables.end()) return globalIt->second.value;
        }
    }

    // Check global lists
    for (const auto &currentSprite : Scratch::sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->lists.find(variableId);
            if (globalIt == currentSprite->lists.end()) continue;
            std::string result;
            std::string seperator = "";
            for (const auto &item : globalIt->second.items) {
                if (item.asString().size() > 1 || !item.isString()) {
                    seperator = " ";
                    break;
                }
            }
            for (const auto &item : globalIt->second.items) {
                result += item.asString() + seperator;
            }
            if (!result.empty() && !seperator.empty()) result.pop_back();
            return Value(result);
        }
    }

    return Value();
}

#ifdef ENABLE_CLOUDVARS
void BlockExecutor::handleCloudVariableChange(const std::string &name, const std::string &value) {
    for (const auto &currentSprite : Scratch::sprites) {
        if (currentSprite->isStage) {
            for (auto it = currentSprite->variables.begin(); it != currentSprite->variables.end(); ++it) {
                if (it->second.name != name) continue;
                it->second.value = Value(value);
                return;
            }
        }
    }
}
#endif