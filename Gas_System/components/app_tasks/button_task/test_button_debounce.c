#include "button_debounce.h"

#include <assert.h>
#include <stdio.h>

static uint8_t update(button_debouncer *filter, uint8_t mask, uint64_t time_ms)
{
    uint8_t edges = 0;
    assert(button_debounce_update(filter, mask, time_ms, &edges));
    return edges;
}

static void test_short_bounce_is_ignored(void)
{
    button_debouncer filter = { 0 };
    assert(update(&filter, 0, 0) == 0);
    assert(update(&filter, 1, 10) == 0);
    assert(update(&filter, 0, 20) == 0);
    assert(update(&filter, 1, 30) == 0);
    assert(update(&filter, 1, 59) == 0);
    assert(update(&filter, 1, 60) == 1);
}

static void test_held_button_only_emits_one_press(void)
{
    button_debouncer filter = { 0 };
    assert(update(&filter, 0, 0) == 0);
    assert(update(&filter, 1, 1) == 0);
    assert(update(&filter, 1, 31) == 1);
    assert(update(&filter, 1, 100) == 0);
    assert(update(&filter, 0, 101) == 0);
    assert(update(&filter, 0, 131) == 0);
    assert(update(&filter, 1, 132) == 0);
    assert(update(&filter, 1, 162) == 1);
}

int main(void)
{
    test_short_bounce_is_ignored();
    test_held_button_only_emits_one_press();
    puts("PASS: button interrupt debounce behavior");
    return 0;
}
