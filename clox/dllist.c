#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
  char *string;
  struct Node *prev;
  struct Node *next;

} Node;

Node *create_node(const char *str) {
  Node *node = malloc(sizeof(Node));
  if (!node)
    return NULL; // or handle error

  node->next = NULL;
  node->prev = NULL;

  node->string = strdup(str); // duplicate string for safety
  if (!node->string) {
    free(node);
    return NULL;
  }

  return node;
}

void free_node(Node *node) {
  free(node->string);
  free(node);
}

typedef struct {
  struct Node *head;
  struct Node *tail;
} List;

bool has_next(Node *node) { return node != NULL && node->next != NULL; };

bool is_empty(List *list) { return list->head == NULL; };

void list_init(List *list) {
  list->head = NULL;
  list->tail = NULL;
}

void insert_node_front(List *list, const char *str) {
  Node *node = create_node(str);

  if (is_empty(list)) {
    list->head = node;
    list->tail = node;
  } else {
    node->next = list->head;
    list->head->prev = node;
    list->head = node;
  }
};

void insert_node_back(List *list, const char *str) {
  Node *node = create_node(str);
  if (is_empty(list)) {
    list->head = node;
    list->tail = node;
  } else {
    node->prev = list->tail;
    list->tail->next = node;
    list->tail = node;
  }
};

void delete_node_front(List *list) {
  if (is_empty(list))
    return;
  if (!has_next(list->head)) {
    free_node(list->head);
    list->head = NULL;
    list->tail = NULL;
  } else {
    Node *old_head = list->head;
    list->head = list->head->next;
    list->head->prev = NULL;
    free_node(old_head);
  }
}

void delete_node_back(List *list) {
  if (is_empty(list))
    return;
  if (!has_next(list->head)) {
    free_node(list->head);
    list->head = NULL;
    list->tail = NULL;
  } else {
    Node *old_tail = list->tail;
    list->tail = list->tail->prev;
    list->tail->next = NULL;
    free_node(old_tail);
  }
}

Node *find(List *list, const char *str) {
  Node *current = list->head;
  while (current != NULL) {
    if (strcmp(str, current->string) == 0) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

bool list_remove(List *list, const char *str) {
  if (is_empty(list))
    return false;

  Node *node = find(list, str);
  if (node == NULL) {
    return false;
  } else {
    if (!has_next(list->head)) {
      list->head = NULL;
      list->tail = NULL;
      free_node(node);
      return true;
    }

    if (node->prev == NULL) {
      node->next->prev = NULL;
      list->head = node->next;
      free_node(node);
      return true;
    } else if (node->next == NULL) {
      node->prev->next = NULL;
      list->tail = node->prev;
      free_node(node);
      return true;
    } else {
      node->prev->next = node->next;
      node->next->prev = node->prev;
      free_node(node);
      return true;
    }
  }
}

void print_list(List *list) {
  if (is_empty(list)) {
    return;
  }
  Node *curr = list->head;

  while (curr != NULL) {
    printf("%s ", curr->string);
    curr = curr->next;
  }
}

void destroy_list(List *list) {
  if (is_empty(list))
    return;
  Node *curr = list->head;
  while (curr != NULL) {
    Node *node = curr;
    curr = curr->next;
    free_node(node);
    list->head = NULL;
    list->tail = NULL;
  }
}

int main() {

  List list;

  list_init(&list);

  printf("=== Insert Back ===\n");
  insert_node_back(&list, "Alice");
  insert_node_back(&list, "Bob");
  insert_node_back(&list, "Charlie");
  print_list(&list);

  printf("\n=== Insert Front ===\n");
  insert_node_front(&list, "Zara");
  insert_node_front(&list, "Mike");
  print_list(&list);

  printf("\n=== Remove Middle (Bob) ===\n");
  list_remove(&list, "Bob");
  print_list(&list);

  printf("\n=== Remove Head (Mike) ===\n");
  list_remove(&list, "Mike");
  print_list(&list);

  printf("\n=== Remove Tail (Charlie) ===\n");
  list_remove(&list, "Charlie");
  print_list(&list);

  printf("\n=== Cleanup ===\n");
  print_list(&list);
  destroy_list(&list);

  return 0;
}
