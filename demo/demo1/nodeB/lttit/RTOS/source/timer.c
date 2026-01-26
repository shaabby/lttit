#include "timer.h"
#include "heap.h"
#include "compare.h"
#include "macro.h"

struct timer_obj {
    struct rb_node node;
    uint32_t period;
    TimerFunction_t callback;
    uint8_t stop_flag;
};

struct rb_root clock_tree;
static uint16_t timer_check_period;
extern uint32_t NowTickCount;

static void clock_tree_add(struct timer_obj *t)
{
    uint32_t key = xEnterCritical();
    t->node.value = NowTickCount + t->period;
    rb_insert_node(&clock_tree, &t->node);
    xExitCritical(key);
}

static void clock_tree_remove(struct timer_obj *t)
{
    uint32_t key = xEnterCritical();
    rb_remove_node(&clock_tree, &t->node);
    xExitCritical(key);
}

static void timer_check(void)
{
    for (;;) {
        TaskEnter();

        struct rb_node *n = clock_tree.first_node;
        struct rb_node *next;

        while (n && compare_before_eq(n->value, NowTickCount)) {
            next = rb_next(n);

            struct timer_obj *t =
            container_of(n, struct timer_obj, node);

            t->callback(t);

            clock_tree_remove(t);

            if (t->stop_flag == run)
                clock_tree_add(t);

            n = next;
        }

        TaskExit();
    }
}

TaskHandle_t TimerInit(uint16_t stack, uint16_t period,
                       uint8_t respond_line, uint32_t deadline,
                       uint8_t check_period)
{
    TaskHandle_t self = NULL;

    rb_root_init(&clock_tree);

    TaskCreate((TaskFunction_t)timer_check,
               stack,
               NULL,
               period,
               respond_line,
               deadline,
               &self);

    timer_check_period = NowTickCount + check_period;

    return self;
}

TimerHandle TimerCreat(TimerFunction_t cb, uint32_t period, uint8_t flag)
{
    struct timer_obj *t = heap_malloc(sizeof(*t));
    if (!t)
        return NULL;

    *t = (struct timer_obj){
            .period = period,
            .callback = cb,
            .stop_flag = flag
    };

    rb_node_init(&t->node);
    clock_tree_add(t);

    return t;
}

void TimerDelete(TimerHandle t)
{
    heap_free(t);
}
