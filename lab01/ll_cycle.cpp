#include "ll_cycle.h"

bool ll_has_cycle(node *head) {
    /* Ваш код должен быть написан только внутри этой функции */
    if (!head)
    	return false;

    node *hare = head, *turtle = head;
    //do a flip!
   	do
   	{
   		turtle = turtle->next;
   		hare = hare->next;
   		if (!turtle || !hare)
   			return false;
   		hare = hare->next;
   		if (!hare)
   			return false;
   	} while (hare != turtle);
    return true;
}
