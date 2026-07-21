#include "resource.h"

#include "pphp/pphp.h"

static void iterator_destroy(presource *resource) {
    parray_iterator *iterator = (parray_iterator *)resource;
    pv_release(pv_heap(PT_ARRAY, &iterator->array->header));
    pphp_free(iterator);
}

parray_iterator *pa_iterator_new(parray *array) {
    parray_iterator *iterator;
    if (array == NULL) return NULL;
    iterator = pphp_alloc(sizeof(*iterator));
    if (iterator == NULL) return NULL;
    iterator->resource.header.refcnt = 1U;
    iterator->resource.header.type = PT_RESOURCE;
    iterator->resource.header.flags = 0U;
    iterator->resource.destroy = iterator_destroy;
    iterator->array = array;
    iterator->position = 0U;
    pv_retain(pv_heap(PT_ARRAY, &array->header));
    return iterator;
}

int pa_iterator_next(parray_iterator *iterator, pvalue *key, pvalue *value) {
    return iterator != NULL &&
           pa_entry_at(iterator->array, iterator->position, key, value,
                       &iterator->position);
}

void presource_destroy(presource *resource) {
    if (resource != NULL && resource->destroy != NULL) {
        resource->destroy(resource);
    }
}
