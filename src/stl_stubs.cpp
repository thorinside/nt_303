/*
 * Minimal std::list node implementations for bare-metal ARM
 * MIT License - Copyright (c) 2025
 */

#include <cmath>

extern "C" double sinh(double x) {
    double ex = exp(x);
    return (ex - 1.0/ex) * 0.5;
}

extern "C" double tanh(double x) {
    double ex = exp(2.0 * x);
    return (ex - 1.0) / (ex + 1.0);
}

struct ListNodeBase {
    ListNodeBase* next;
    ListNodeBase* prev;
};

extern "C" {

void _ZNSt8__detail15_List_node_base7_M_hookEPS0_(ListNodeBase* self, ListNodeBase* pos) {
    self->next = pos;
    self->prev = pos->prev;
    pos->prev->next = self;
    pos->prev = self;
}

void _ZNSt8__detail15_List_node_base11_M_transferEPS0_S1_(ListNodeBase* self, ListNodeBase* first, ListNodeBase* last) {
    if (self == last) return;
    last->prev->next = self;
    first->prev->next = last;
    self->prev->next = first;
    ListNodeBase* tmp = self->prev;
    self->prev = last->prev;
    last->prev = first->prev;
    first->prev = tmp;
}

void _ZNSt8__detail15_List_node_base4swapERS0_S1_(ListNodeBase* a, ListNodeBase* b) {
    if (a->next != a) {
        if (b->next != b) {
            ListNodeBase* tmp = a->next;
            a->next = b->next;
            b->next = tmp;
            tmp = a->prev;
            a->prev = b->prev;
            b->prev = tmp;
            a->next->prev = a;
            a->prev->next = a;
            b->next->prev = b;
            b->prev->next = b;
        } else {
            b->next = a->next;
            b->prev = a->prev;
            b->next->prev = b;
            b->prev->next = b;
            a->next = a;
            a->prev = a;
        }
    } else if (b->next != b) {
        a->next = b->next;
        a->prev = b->prev;
        a->next->prev = a;
        a->prev->next = a;
        b->next = b;
        b->prev = b;
    }
}

}
