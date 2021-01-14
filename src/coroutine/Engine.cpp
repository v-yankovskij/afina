#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

__attribute__((no_sanitize_address))
void Engine::Store(context &ctx) {
    char Stack = 0;
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

__attribute__((no_sanitize_address))
void Engine::Restore(context &ctx) {
    char Stack;
    while (ctx.Low <= &Stack && &Stack <= ctx.Hight) {
        Restore(ctx);
    }
    std::memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.Hight - ctx.Low);
    std::longjmp(ctx.Environment, 1);
}

__attribute__((no_sanitize_address))
void Engine::yield() {
    auto it = alive;
    if (it && it == cur_routine) {
        it = it->next;
    }
    if (it) {
        sched(it);
    }
}

__attribute__((no_sanitize_address))
void Engine::sched(void *routine_) {
    if (!routine_ || routine_ == cur_routine) {
        return yield();
    }
    if (cur_routine && cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment)) {
            return;
        }
        Store(*cur_routine);
    }
    cur_routine = (context *)routine_;
    Restore(*(context *)routine_);
}

__attribute__((no_sanitize_address))
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

__attribute__((no_sanitize_address))
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

__attribute__((no_sanitize_address))
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

__attribute__((no_sanitize_address))
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
