#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char Stack;
    if (&Stack > StackBottom) {
            ctx.Hight = &Stack;
    } else {
            ctx.Low = &Stack;
    }
    auto stack_size = ctx.Hight - ctx.Low;
    if (stack_size > std::get<1>(ctx.Stack) || stack_size * 2 < std::get<1>(ctx.Stack)) {
            delete[] std::get<0>(ctx.Stack);
            std::get<0>(ctx.Stack) = new char[stack_size];
            std::get<1>(ctx.Stack) = stack_size;
    }
    std::memcpy(std::get<0>(ctx.Stack), ctx.Low, stack_size);
}

void Engine::Restore(context &ctx) {
    char Stack;
    while (ctx.Low <= &Stack && &Stack <= ctx.Hight) {
        Restore(ctx);
    }
    std::memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.Hight - ctx.Low);
    std::longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    auto it = alive;
    if (it && it == cur_routine) {
        it = it->next;
    }
    if (it) {
        sched(it);
    }
}

void Engine::sched(void *routine_) {
    if (!routine_ || routine_ == cur_routine) {
        return yield();
    }
    if (cur_routine) {
        Store(*cur_routine);
        if (setjmp(cur_routine->Environment)) {
            return;
        }
    }
    cur_routine = (context *)routine_;
    Restore(*(context *)routine_);
}

void Engine::remove(context*& list, context*& routine_){
    if (list == nullptr){
        return;
    }
    if (list == routine_){
        list = routine_->next;
    }
    if (routine_->next != nullptr){
        routine_->next->prev = routine_->prev;
    }
    if (routine_->prev != nullptr){
        routine_->prev->next = routine_->next;
    }

}

void Engine::add(context*& list, context*& routine_){
    if (list == nullptr){
        list = routine_;
        list->next = nullptr;
        list->prev = nullptr;
    } else {
        routine_->next = list;
        routine_->prev = nullptr;
        list->prev = routine_;
        list = routine_;
    }
}

void Engine::block(void *coro) {
    auto blocking = (context *)coro;
    if (coro == nullptr) {
        blocking = cur_routine;
    }
    remove(alive, blocking);
    add(blocked, blocking);
    if (blocking == cur_routine) {
        Restore(*idle_ctx);
    }
}

void Engine::unblock(void *coro) {
    auto unblocking = (context *)coro;
    if (unblocking == nullptr){
        return;
    }
    remove(blocked, unblocking);
    add(alive, unblocking);
}

} // namespace Coroutine
} // namespace Afina
