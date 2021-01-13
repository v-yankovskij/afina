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
    }//определение границ стека
    auto stack_size = ctx.Hight - ctx.Low;//вычисление размера стека
    if (stack_size > std::get<1>(ctx.Stack) /*если имеющееся количество выделенной внутри памяти слишком мало чтобы сохранить стек*/ || stack_size * 2 < std::get<1>(ctx.Stack) /*или если можно обойтись значительно меньшим объемом памяти для этой цели*/) {
            delete[] std::get<0>(ctx.Stack);//освобождение прошлого куска выделенной памяти
            std::get<0>(ctx.Stack) = new char[stack_size];//выделение нового куска памяти
            std::get<1>(ctx.Stack) = stack_size;//подходящего размера
    }
    std::memcpy(std::get<0>(ctx.Stack), ctx.Low, stack_size);//сохранение стека в выделенной памяти
}

void Engine::Restore(context &ctx) {
    char Stack;
    while (ctx.Low <= &Stack && &Stack <= ctx.Hight) {
        Restore(ctx);
    }//многократно рекурсивно вызывая функцию сдвигаем вершину общего стека за пределы верхней границы памяти которую пытаемся восстановить (дабы не испортить её вызовом longjmp)
    std::memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.Hight - ctx.Low););//восстанавливаем стек по сохраненной копии 
    std::longjmp(ctx.Environment, 1);//восстанавливаем регистры микропроцессора
}

void Engine::yield() {
    auto it = alive;
    if (it && it == cur_routine) {
        it = it->next;
    }
    if (it) {
        sched(it);
    }
}//переходит к произвольной другой активной корутине

void Engine::sched(void *routine_) {
    if (!routine_ ) {
        return yield();
    }//если routine_ == nullptr, то вызываем yield
    if (routine_ == cur_routine) {
        return;
    }//если указанная корутина – текущая, то не делаем ничего
    if (cur_routine) {
        if (setjmp(cur_routine->Environment)/* сохранение регистров микропроцессора*/) {
            return;
        }//если мы вернулись сюда с помощью longjmp, то setjmp выдаёт не 0 и мы не делаем ничего
        Store(*cur_routine));//сохранение стека текущей корутины
    }
    cur_routine = (context *)routine_;//смена корутины 
    Restore(*(context *)routine_);//на указанную
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
} // стандартное удаление элемента из двусвязного списка

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
}// стандартное добавление элемента в начало двусвязного списка

void Engine::block(void *coro) {
    auto blocking = (context *)coro;
    if (coro == nullptr) {
        blocking = cur_routine;
    }
    remove(alive, blocking);//удаление корутины из списка активных
    add(blocked, blocking);//и добавление в список заблокированных
    if (blocking == cur_routine) {
        Restore(*idle_ctx);
    }//если была заблокирована текущая корутина переходим к рутине по умолчанию
}

void Engine::unblock(void *coro) {
    auto unblocking = (context *)coro;
    if (unblocking == nullptr){
        return;
    }
    remove(blocked, unblocking);
    add(alive, unblocking);
}//удаление корутины из списка заблокированных и добавление её в список активных

} // namespace Coroutine
} // namespace Afina
