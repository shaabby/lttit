#include "timer.h"
#include "heap.h"
#include "compare.h"
#include "port.h"
#include "macro.h"
#include "rbtree.h"
#include "schedule.h"

struct timer_obj {
    struct rb_node node;
    uint32_t period;
    TimerFunction_t callback;
    uint8_t stop_flag;
};

struct rb_root clock_tree;
extern uint32_t NowTickCount;

static void clock_tree_add(struct timer_obj *t)
{
    uint32_t key = EnterCritical();
    t->node.value = NowTickCount + t->period;
    rb_insert_node(&clock_tree, &t->node);
    ExitCritical(key);
}

static void clock_tree_remove(struct timer_obj *t)
{
    uint32_t key = EnterCritical();
    rb_remove_node(&clock_tree, &t->node);
    ExitCritical(key);
}

static void timer_check(void)
{
    for (;;) {
        task_enter();

        struct rb_node *n;
        while ((n = clock_tree.first_node) &&
               compare_before_eq(n->value, NowTickCount)) {

            struct timer_obj *t =
                    container_of(n, struct timer_obj, node);

            t->callback(t);

            clock_tree_remove(t);

            if (t->stop_flag == run)
                clock_tree_add(t);
        }

        task_exit();
    }
}

TaskHandle_t timer_init(uint16_t stack,
                        uint16_t period,
                        uint8_t respond_line,
                        uint32_t deadline)
{
    TaskHandle_t self = NULL;

    rb_root_init(&clock_tree);

    task_create((TaskFunction_t)timer_check,
                stack,
                NULL,
                period,
                respond_line,
                deadline,
                &self);

    return self;
}

TimerHandle timer_create(TimerFunction_t cb,
                         uint32_t period,
                         uint8_t flag)
{
    struct timer_obj *t = heap_malloc(sizeof(*t));
    if (!t)
        return NULL;

    *t = (struct timer_obj){
            .period = period,
            .callback = cb,
            .stop_flag = flag,
    };

    rb_node_init(&t->node);
    clock_tree_add(t);

    return t;
}

void timer_delete(TimerHandle t)
{
    heap_free(t);
}
