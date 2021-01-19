#include <stdio.h>
#include "my402list.h"
#include <stdlib.h>

int  My402ListLength(My402List* list){
    return list->num_members;
}
int  My402ListEmpty(My402List* list){
    return list->num_members == 0;
}
int  My402ListAppend(My402List* list, void* data){
    My402ListElem *elem = malloc(sizeof(My402ListElem));
    if(elem == NULL){
        return FALSE;
    }
    elem->obj = data;
    if(My402ListEmpty(list)){
        list->anchor.next = elem;
        list->anchor.prev = elem;
        elem->next = &(list->anchor);
        elem->prev = &(list->anchor);
    }else{
        elem->next = &(list->anchor);
        elem->prev = list->anchor.prev;
        list->anchor.prev->next = elem;
        list->anchor.prev = elem;
    }
    list->num_members++;
    return TRUE;
}
int  My402ListPrepend(My402List* list, void* data){
    My402ListElem *elem = malloc(sizeof(My402ListElem));
    if(elem == NULL){
        return FALSE;
    }
    elem->obj = data;
    if(My402ListEmpty(list)){
        list->anchor.next = elem;
        list->anchor.prev = elem;
        elem->next = &(list->anchor);
        elem->prev = &(list->anchor);
    }else{
        elem->prev = &(list->anchor);
        elem->next = list->anchor.next;
        list->anchor.next->prev = elem;
        list->anchor.next = elem;
    }
    list->num_members++;
    return TRUE;
}
void My402ListUnlink(My402List* list, My402ListElem* elem){
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    free(elem);
    list->num_members--;
}
void My402ListUnlinkAll(My402List* list){
    if(My402ListEmpty(list)){
        return;
    }
    My402ListElem *elem = My402ListFirst(list);
    while(elem != &(list->anchor)){
        My402ListElem *temp = elem;
        elem = elem->next;
        free(temp);
    }
    My402ListInit(list);
}
int  My402ListInsertAfter(My402List* list, void* data, My402ListElem* elem){
    if(elem == NULL){
        return My402ListAppend(list, data);
    }
    My402ListElem *newElem = malloc(sizeof(My402ListElem));
    if(newElem == NULL){
        return FALSE;
    }else{
        newElem->obj = data;
        newElem->next = elem->next;
        newElem->prev = elem;
        elem->next->prev = newElem;
        elem->next = newElem;
    }
    list->num_members++;
    return TRUE;
}
int  My402ListInsertBefore(My402List* list, void* data, My402ListElem* elem){
    if(elem == NULL){
        return My402ListPrepend(list, data);
    }
    My402ListElem *newElem = malloc(sizeof(My402ListElem));
    if(newElem == NULL){
        return FALSE;
    }else{
        newElem->obj = data;
        newElem->next = elem;
        newElem->prev = elem->prev;
        elem->prev->next = newElem;
        elem->prev = newElem;
    }
    list->num_members++;
    return TRUE;
}
My402ListElem *My402ListFirst(My402List* list){
    return My402ListEmpty(list) ? NULL : list->anchor.next;
}
My402ListElem *My402ListLast(My402List* list){
    return My402ListEmpty(list) ? NULL : list->anchor.prev;
}
My402ListElem *My402ListNext(My402List* list, My402ListElem* elem){
    return elem->next == &(list->anchor) ? NULL : elem->next;
}
My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem){
    return elem->prev == &(list->anchor) ? NULL : elem->prev;
}
My402ListElem *My402ListFind(My402List* list, void* obj){
    My402ListElem *temp = My402ListFirst(list);
    if(temp == NULL){
        return NULL;
    }
    while(temp != &(list->anchor)){
        if(temp->obj == obj){
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}
int My402ListInit(My402List* list){
    list->num_members = 0;
    list->anchor.next = &(list->anchor);
    list->anchor.prev = &(list->anchor);
    list->anchor.obj = NULL;
    return TRUE;
}
