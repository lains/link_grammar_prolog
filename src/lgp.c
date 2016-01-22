#include <SWI-Prolog.h>
#include <stdlib.h>
#include <ctype.h>
#include "link-includes.h"
#include "constituents.h"
#include "lgp.h"

#define MAXINPUT 1024
#define DISPLAY_MAX 100

#define NB_DICTIONARIES 4 /* Number of dictionaries we allow at the same time in memory */ 
#define NB_PARSE_OPTIONS 4 /* Number of parse options sets we allow at the same time in memory */
#define NB_LINKAGE_SETS 8 /* Number of linkages we allow simultaneously in memory */
#define NB_SENTENCES 8 /* Number of sentences we allow simultaneously in memory */



/* Note: 
 * result_var_name must have been declared as int before calling this macro
 * exception_var_name must have been declared as term_t before calling this macro
 */
#define check_leak_and_raise_exceptions_macro(result_var_name, exception_var_name) {\
  result = check_leak();\
  if (result != 0) {\
    exception=PL_new_term_ref();\
    switch (result) {\
    case 1:\
      PL_unify_term(exception,\
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 1),\
                    PL_CHARS, "memory_leak");\
      break;\
    case 2:\
      PL_unify_term(exception,\
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),\
                    PL_CHARS, "dictionary",\
                    PL_CHARS, "list_corrupted");\
      break;\
    case 3:\
      PL_unify_term(exception,\
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),\
                    PL_CHARS, "parse_options",\
                    PL_CHARS, "list_corrupted");\
      break;\
    case 4:\
      PL_unify_term(exception,\
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),\
                    PL_CHARS, "sentence",\
                    PL_CHARS, "list_corrupted");\
      break;\
    default:\
      PL_unify_term(exception,\
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),\
                    PL_CHARS, PL_new_term_ref(),\
                    PL_CHARS, "list_corrupted");\
      break;\
    }\
    return PL_raise_exception(exception);\
  }\
}



/* The following structure is used in pl_get_linkage. It's a context structure used while Prolog calls a redo on pl_get_linkage. */
typedef struct {
  int                      last_handled_linkage;
  int                      num_linkages;
  unsigned int             link_handle_index; /* Note: this contains the handle to the linkage set object that is used by pl_get_linkage to create linkages from a linkage set */

  // Commented when changing pl_get_linkage to allow linkage sets to act on the context of pl_get_linkage function that are currently referring to them. Rather than having pl_get_linkage access its related sentence and parse options, pl_get_linkage will have to extract this out of the linkage set chained-object
  //  sent_linked_list_object  *sentence;
  //  opts_linked_list_object  *parse_options;
} pl_get_linkage_context;

/* The following declaration defines a chained-list for context objects. This is used in the linkage set chained list to keep a track of the contexts (and thus the current get_linkage/2 predicate) currently valid (not yet cut or failed) */
/* See the declaration for link_linked_list_object_struct for more info */
struct context_list_struct {
  struct context_list_struct *next;
  pl_get_linkage_context     *context_associated_to_link;
};
typedef struct context_list_struct context_list; /* This is the type for a chained-object in the context list, see link_linked_list_object_struct for more info */


/* The following group is a set of definitions of C structures, used to create chained-lists */
/* Each list MUST have as a first member, a pointer to the next element in the list. */
/* They also MUST have a second 'unsigned int' member, which is the number of free elements that we are skipping when jumping to the next element (this is used to manage the handle number for Prolog) */
/* The next field they MUST contain is an 'unsigned int' member, which counts how many other objects have references to the object (see below for more details about the dependencies mechanism) */
/* This is compulsory because lists are handled in a generic way and elements are thus casted to a general element type called (see below) */
/* The rest of the structure contains the actual information (payload) for every single element of the list */


struct generic_linked_list_object_struct {
  struct generic_linked_list_object_struct    *next;
  unsigned int                                nb_jumped_index;
  unsigned int                                count_references;
};
typedef struct generic_linked_list_object_struct generic_linked_list_object;
/*
From the above typedef, the type generic_linked_list_object is one generic element for a chained list
Only these fields exist in each element: a pointer to the next element in the chained list, an integer used to manage the handle indexes for Prolog, and an integer to count the references done to this object

Here are a few details about how Prolog handles are managed (this concerns the field nb_jumped_index)
Prolog handles are composed of a functor (that will tell this interface which chained-list to use) and an unsigned integer (that is used as an index to find an object in the selected list)
Once a list has been selected, using the functor, we need to associate a unique index (actually, an C integer) to every single object in the list
To do so, we could save, on a per-object basis, a field containing the handle in an unsigned int variable, altogether with the next pointer
This would however not guarantee that indexes are unique, so the coherency of the list would be up to the function that handle the chained lists
Another approach to this problem is used here.
The root of a chained-list always has the index 0
The object pointed by a pointer next (the successor of an object) will always have an index that is greater than all its predecessors
We can thus have the following list:
Root ----------------> Object_index_0.next -> Object_index_1.next -> Object_index_4.next -> Object_index_8.next -> NULL 
We don't want to save the index as an int field inside each object, however, if we assume that the list will always be accessed from the root, we can only store the increment between two succeeding objects

Root ----------------> Object_index_0     +-> Object_index_1     +-> Object_index_4     +-> Object_index_8     +-> NULL 
                       next --------------+   next --------------+   next --------------+   next --------------+        
                       nb_jumped_index=0      nb_jumped_index=2      nb_jumped_index=3      nb_jumped_index=0           
To calculate the index of the next object in the list, we just take index of the current index, add 1, and add the value of nb_jumped_index

This allows us to delete any object without loosing the coherency of the list. Here is the same list after deleting Object_index_4:
Root ----------------> Object_index_0     +-> Object_index_1     +------------------------> Object_index_8     +-> NULL 
                       next --------------+   next --------------+                          next --------------+        
                       nb_jumped_index=0      nb_jumped_index=6                             nb_jumped_index=?           

When adding new objects to the list, the best thing to do to minimize indexes is to add them in the first "empty" space in the chain:
Root ----------------> Object_index_0     +-> Object_index_1     +-> Object_index_2     +-> Object_index_8     +-> NULL 
                       next --------------+   next --------------+   next --------------+   next --------------+        
                       nb_jumped_index=0      nb_jumped_index=0      nb_jumped_index=5      nb_jumped_index=?           

Unfortunately, this doesn't allow us to delete the first element of the list!
The solution to this is to declare the root as an object as well, but not containing any object. The previous list would thus be:
Root_index_-1      +-> Object_index_0     +-> Object_index_1     +-> Object_index_2     +-> Object_index_8     +-> NULL 
next --------------+   next --------------+   next --------------+   next --------------+   next --------------+        
nb_jumped_index=0      nb_jumped_index=0      nb_jumped_index=0      nb_jumped_index=5      nb_jumped_index=?           

Now, we can easily delete the first element of the list without changing the indexes of the existing objects:
Root_index_-1      +------------------------> Object_index_1     +-> Object_index_2     +-> Object_index_8     +-> NULL                        
next --------------+                          next --------------+   next --------------+   next --------------+        
nb_jumped_index=1                             nb_jumped_index=0      nb_jumped_index=5      nb_jumped_index=?           

Note: an empty list will be:
Root_index_-1      +-> NULL 
next --------------+        
nb_jumped_index=?           

*/


/* See "http://www.eskimo.com/~scs/C-faq/q1.14.html" which explains the definition of chained-lists */
/* Type declaration for the chained-list objects containing dictionary payloads */
struct dict_linked_list_object_struct {
  struct dict_linked_list_object_struct       *next;
  unsigned int                                nb_jumped_index;
  unsigned int                                count_references;
  Dictionary                                  payload;
};
typedef struct dict_linked_list_object_struct dict_linked_list_object; /* This declares a structure for a chained-list containing dictionary payload */



/* Type declaration for the chained-list objects containing parse options payloads */
struct opts_linked_list_object_struct {
  struct opts_linked_list_object_struct       *next;
  unsigned int                                nb_jumped_index;
  unsigned int                                count_references;
  Parse_Options                               payload;
};
typedef struct opts_linked_list_object_struct opts_linked_list_object; /* This declares a structure for a chained-list of parse options payloads */



/* Declaration of the structure for sentence payloads */
typedef struct {
  Sentence                                    sentence; /* Actual payload for the sentence object */
  dict_linked_list_object                     *associated_dictionary_chained_object; /* Link to the dictionary used by this sentence object */
} sent_payload; /* This is the structure that will be put in the payload part of the sentence object in chained-list (the payload won't, indeed, be only a straightforward pointer) */

/* Type declaration for the chained-list objects containing sentence payloads */
struct sent_linked_list_object_struct {
  struct sent_linked_list_object_struct       *next;
  unsigned int                                nb_jumped_index;
  unsigned int                                count_references;
  sent_payload                                payload;
};
typedef struct sent_linked_list_object_struct sent_linked_list_object; /* This declares a structure for a chained-list of sentence payloads */



/* Declaration of the structure for linkage set payloads */
typedef struct {
  int                                         num_linkages;
  context_list                                *associated_context_list;
  sent_linked_list_object                     *associated_sentence_chained_object;
  opts_linked_list_object                     *associated_parse_options_chained_object;
} link_payload; /* This is the structure that will be put in the payload part of the linkage set object in chained-list (the payload won't, indeed, be only a straightforward pointer) */
/* Note: there is no proper linkage set pointer in the linkage set chained-list objects, because no LGP API linkage set is defined */
/* Instead of this the num_linkage value, together with the sentence and the parse options can define individual linkages (see linkage_create in the C function pl_get_linkage) */
/* For this reason, Linkage objects will be created, converted to Prolog compounds and destroyed within pl_get_linkage, without the need of anything else than the 3 fields of this payload */
/* Actually, we save the total number of linkages here, whereas the value passed to linkage_create is the number of the linkage requested within the linkage set. This is not a problem because the last created linkage is stored in the Prolog context variable, and used when redoing the predicate (see pl_get_linkage for more details) */

/* Type declaration for the chained-list objects containing linkage set payloads */
struct link_linked_list_object_struct {
  struct link_linked_list_object_struct       *next;
  unsigned int                                nb_jumped_index;
  unsigned int                                count_references;
  link_payload                                payload; /* See above for the content of the link_payload structure */
};
typedef struct link_linked_list_object_struct link_linked_list_object; /* This declares a structure for a chained-list of linkage set payloads */

/* The type for link_linked_list_object is a bit more complex than the other chained lists, because of the fact it is used by the Prolog predicate get_linkage/2 */
/* The structure is as follows: */
/* *link_linked_list_object contains: */
/* next->link_linked_list_object // pointer to the next object in the linkage set chained list */
/* count_references // number of references made to this object (always 0, because no other object can statically reference a linkage set, only context objects in get_linkage/2 reference linkage set objects, but this is done dynamically (see below) */
/* payload-> // This is the field containing the actual information for a linkage set */

/* In payload, we have the following fields: */
/* num_linkages // number of linkage that can be extracted from this linkage set object in LGP */
/* associated_context_list->context_list // starting point to the chained list gathering all the context objects using this linkage set */
/* associated_sentence_chained_object->sent_linked_list_object // pointer on the sentence object used by this linkage set */
/* associated_parse_options_chained_object->opts_linked_list_object // pointer on the parse options object used by this linkage set */

/**
 * Here is an example of the lifecycle of a link_linked_list_object:
 * Call to pl_create_linkage_set()
 * -------------------------------
 *
 * - Creation of a link_linked_list_object (let's say at address &AL)
 * - Creation of a Linkage object in LGP (let's say at address &LK)
 * - The sentence object used together with the linkage set has address &S
 * - The parse options object used together with the linkage set has address &PO
 * - The linkage set object contains 3 linkages (for example)
 * - The new link_linked_list_object object has been created with handle index 5 (for example)
 * - We have:
 * handle_index=-1 ....  5
 * root_link_list->....->&AL                                    +---------------------------------->....
 *                       (link_linked_list_object STRUCTURE)    |
 *                       (                                 )    |
 *                       (next-----------------------------)----+
 *                       (nb_jumped_index=?                )
 *                       (payload--------------------------)----+
 *                                                              |
 *                                                              |
 *                                                              |
 *                  (link_payload STRUCTURE                  )<-+
 *                  (                                        )
 *                  (associate_sentence_chained_object-------)----->&S
 *                  (associate_parse_options_chained_object--)----->&PO
 *                  (num_linkages=3                          )
 *                  (associated_context_list-----------------)-> NULL
 *
 * Call to get_linkage('$linkage'(5), Result_linkage).
 * ---------------------------------------------------
 * - Creation of a context object (let's say at address &C1)
 * - Creation of a link to the above context in the associated_context_list for the linkage set 5
 * - Setup of the fields in the context object (record associated_sentence_chained_object, associated_parse_options_chained_object and num_linkages)
 * - Setup of the field link_handle_index in the context object
 * - We now have:
 * handle_index=-1 ....  5
 * root_link_list->....->&AL                                    +---------------------------------->....
 *                       (link_linked_list_object STRUCTURE)    |
 *                       (                                 )    |
 *                       (next-----------------------------)----+
 *                       (nb_jumped_index=?                )
 *                       (payload--------------------------)----+
 *                                                              |
 *                                                              |
 *                                                              |
 *                  (link_payload STRUCTURE                  )<-+
 *                  (                                        )
 *                  (associate_sentence_chained_object-------)----->&S
 *                  (associate_parse_options_chained_object--)----->&PO
 *                  (num_linkages=3                          )
 *                  (associated_context_list-----------------)--+
 *                                                              |
 *                                                              |
 *                  (context_list_struct STRUCTURE)<------------+
 *                  (                             )
 *                  (next-------------------------)------------> NULL
 *                  (context_associated_to_link---)-----+
 *                                                      |
 *                                                      |
 *                  (pl_get_linkage_context STRUCTURE)<-+
 *                  (                                )
 *                  (last_handled_linkage=0          )
 *                  (link_handle_index=5             )
 *
 * Call to delete_linkage('$linkage'(5)).
 * --------------------------------------
 * - Deletion of the chained link_linked_list_object
 * - Update of all related context structures to have a related link_handle_index of -1
 * - We now have:
 * handle_index=-1 ....  5
 * root_link_list->....->-------------------------------------------------------------------------->....
 *
 *
 *                  (pl_get_linkage_context STRUCTURE)
 *                  (                                )
 *                  (last_handled_linkage=?          )
 *                  (link_handle_index=-1            )
 *
**/






/* Nothing is the chained lists. Direct the root to NULL */
generic_linked_list_object          *root_dict_list=NULL; /* This list will contain dict_linked_list_object objects */
generic_linked_list_object          *root_opts_list=NULL; /* This list will contain opts_linked_list_object objects */
generic_linked_list_object          *root_sent_list=NULL; /* This list will contain sent_linked_list_object objects */
generic_linked_list_object          *root_link_list=NULL; /* This list will contain link_linked_list_object objects */


/* Here is a chart describing the dependencies between dictionary, sentence, parse options and linkage set objects */
/*                                                                                                                 */
/*               Dictionary------+                                                                                 */
/*                               +------> Sentence--------+                                                        */
/*                                                        |                                                        */
/*                                                        |                                                        */
/*                                                        |                                                        */
/*                                                        +-------> Linkage Set                                    */
/*                                                        +------->                                                */
/*                                                        |                                                        */
/*               Parse_Options ---------------------------+                                                        */
/*                                                                                                                 */

/* As a result, a Dictionary cannot be deleted if at least one sentence object is still referencing it */
/* To provide a safe mechanism that prevents this from happening, each generic chained-object contains an unsigned integer named count_references. */
/* This integer contains the number of references done to the object, and deleting an object won't be allowed unless its count_references=0 */


static functor_t       FUNCTOR_dictionary1; /* This functor (with a $ as first character is used to create references to the dictionary table (dict_table)) */
static functor_t       FUNCTOR_options1; /* This functor (with a $ as first character is used to create references to the parse options table (opts_table)) */
static functor_t       FUNCTOR_linkageset1; /* This functor (with a $ as first character is used to create references to the linkages table (link_table)) */
static functor_t       FUNCTOR_sentence1; /* This functor (with a $ as first character is used to create references to the sentence table (sent_table)) */
static functor_t       FUNCTOR_list2; /* This is the ./2 functor to construct lists */
static functor_t       FUNCTOR_equals2; /* This is the =/2 functor */
static functor_t       FUNCTOR_link2; /* This is the link/2 functor used to return the result of a parsing (links) */
static functor_t       FUNCTOR_hyphen2; /* This is the -/2 functor */
static functor_t       FUNCTOR_connection3; /* This is the connection/2 functor used to gather a connector and the two words it connects */

static int max_sentence_length=70;
static int min_short_sent_len=20;










/**
 * @name static int check_leak()
 *
 * @description
 * This function will check if no leak has been detected inside the grammar language parser library.
 * The test is only done when NO dictionary, NO parse option or sentence object remains in memory.
 * The value returned by this function is the error code. The meanings are:
 * (1) There is a memory leak
 * (2) The dictionary list is corrupted
 * (3) The parse options list is corrupted
 * (4) The sentence list is corrupted
 * (0) No leak was found. The function executed successfully
 * Note: this list of value is converted into prolog exceptions by the check_leak_and_raise_exceptions_macro #define macro above. Any value added/removed from this list requires the corresponding update into the check_leak_and_raise_exceptions_macro macro
**/

static int check_leak() {

  if (root_dict_list != NULL) { /* Check if the root of the list containing the dictionaries has been initialised */
    if (root_dict_list->next != NULL) /* If yes, check if there is a first element */
      return 0; /* There is at least one dictionary object remaining in memory. Can't check leaks */
  }
  else {
    return 2;
  }

  if (root_opts_list != NULL) { /* Check if the root of the list containing the parse options has been initialised */
    if (root_opts_list->next != NULL) /* If yes, check if there is a first element */
      return 0; /* There is at least one parse options object remaining in memory. Can't check leaks */
  }
  else {
    return 3;
  }

  if (root_sent_list != NULL) { /* Check if the root of the list containing the sentences has been initialised */
    if (root_sent_list->next != NULL) /* If yes, check if there is a first element */
      return 0; /* There is at least one sentence object remaining in memory. Can't check leaks */
  }
  else {
    return 4;
  }


  /* Note: we don't test linkage sets because even if there are remaining ones, they will not use any external space */
  /* Note: anyway, if there are remaining ones, there should still be sentence and parse option objects ;-) */
  if (external_space_in_use != 0) {
    return 1;
  }
  return 0;
}


/**
 * @name static int count_objects_in_chained_list(generic_linked_list_object *root)
 *
 * @description
 * This function will return the number objects chained in the list, using the parameter root as the begining of the list
 * Note: this function can be applied to any chained list containing elements that comply with the pattern defined in generic_linked_list_object.
 * The root pointer might thus need to be casted to the generic_linked_list_object type when giving it as parameter for this function
**/

static unsigned int count_objects_in_chained_list(generic_linked_list_object *root) {

int object_count;
  
  object_count=-1;

  while (root!=NULL) {
    object_count++;
    root=root->next;
  }
  return object_count;
}


/**
 * @name static int get_object_from_handle_index_in_chained_list(generic_linked_list_object *root, unsigned int searched_handle_index, generic_linked_list_object **ref_ptr_to_result)
 *
 * @description
 * This function will browse the chained list originating from the root pointer given in argument.
 * It will look for the first element having a handle_index field equal to the one given in argument (searched_handle_index) and will return a reference as a "generic_linked_list_object pointer" to it inside the variable pointed by ref_ptr_to_result (this is thus passed as reference)
 * Note: this function can be applied to any chained list containing elements that comply with the pattern defined in generic_linked_list_object.
 * The root pointer might thus need to be casted to the generic_linked_list_object type when giving it as parameter for this function
 * The function will also return a generic_linked_list_object that will need to be casted back to the appropriate type in the code calling this function
 * Note: if no element in the chained list matches the handle_index, NULL will be returned.
 * The value returned by this function is an integer giving the status of the search:
 * (1) root == NULL
 * (2) The index could not be found in the list
 * (3) The index was -1
 * (0) indicates that the object has been found and returned in *ref_ptr_to_result
**/

static int get_object_from_handle_index_in_chained_list(generic_linked_list_object *root, unsigned int searched_handle_index, generic_linked_list_object **ref_ptr_to_result) {

unsigned int current_handle;


  current_handle = (unsigned int)-1;

  if (searched_handle_index == -1) {
    if (ref_ptr_to_result != NULL) *ref_ptr_to_result = NULL; /* root doesn't point anywhere, return NULL (1) */
    return 3;
  }

  if (root == NULL) {
    if (ref_ptr_to_result != NULL) *ref_ptr_to_result = NULL; /* root doesn't point anywhere, return NULL (1) */
    return 1;
  }

#define current_object root
/* Note: the above define directive is done to make the code easier to read. We avoid to declare another generic_linked_list_object pointer by reusing root to browse the whole list, however, using root as the variable name can be confusing, so we rename it into current_object, but the compiler will interpret it as root for the part of the code below */
/* If the compiler used doesn't like this #define directive, just replace all references to "current_object" with "root" in the following part (between #define and #undef and comment the two aforementionned directives) */
  while (current_object != NULL) {
    if (current_handle == searched_handle_index) {
      if (ref_ptr_to_result != NULL) *ref_ptr_to_result = current_object; /* We found our object in the list */
      return 0; /* (0) */
    }
    else {
      current_handle += current_object->nb_jumped_index; /* Add the amount of empty spaces available between the current object and the next object */
      current_handle++; /* Increment the handle because we are going to work on the successor in the list */
      if (current_handle > searched_handle_index) { /* We have gone too far in the chained list because the current_handle is greater than the one searched. Note: there is no risk to have current_handle=-1 because we have incremented it before */
        if (ref_ptr_to_result != NULL) *ref_ptr_to_result = NULL; /* The index could not be found in the list (2) */
        return 2;
      }
      else {
        current_object = current_object->next; /* Work on the successor */
      }
    }
  }
  if (ref_ptr_to_result != NULL) *ref_ptr_to_result = NULL; /* The index could not be found in the list (2) */
  return 2;
#undef current_object
}


/**
 * @name static int get_object_from_handle_index_in_chained_list_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, unsigned int searched_handle_index, generic_linked_list_object **ref_ptr_to_result_object)
 *
 * @description
 * This is a front-end function to get_object_from_handle_index_in_chained_list (called C subfunction in the text below)
 * This function will, in addition to call the C subfunction, handle exceptions from the int result returned by the subfunction
 * The parameters are the same as the C subfunction, except that two extra first parameter are required.
 * The first one (exception_title) holds the title of the exception (null terminated C-style string), which will be used in the exception term created if needed
 * The second one (ref_ptr_to_result) is the address to an int where the result returned by the C subfunction, in case a fine processing is needed, depending on the cause of the failure (note, if the pointer is set to NULL, no value will be stored there)
 * If this function returns with TRUE=1 (using PL_succeed) if everything was fine
 * It returns FALSE=0 (using return PL_raise_exception()), and, in order to pass the exception to Prolog, the calling function will just needs to fail.
**/

static int get_object_from_handle_index_in_chained_list_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, unsigned int searched_handle_index, generic_linked_list_object **ref_ptr_to_result_object) {

int        result;
term_t     exception;

  result = get_object_from_handle_index_in_chained_list(root, searched_handle_index, ref_ptr_to_result_object);
  if (ref_ptr_to_result_value != NULL) *ref_ptr_to_result_value = result;
  switch (result) {
  case 0: /* Function executed successfully */
    PL_succeed;
  case 2:
    exception = PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, exception_title,
                  PL_CHARS, "empty_index"); /* This index is valid but doesn't exist in the chained-list */
    return PL_raise_exception(exception);
  case 3:
    exception = PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, exception_title,
                  PL_CHARS, "bad_handle"); /* Handle with index -1 is not accepted */
    return PL_raise_exception(exception);
  case 1:
  default:
    exception = PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, exception_title,
                  PL_CHARS, "list_corrupted");
    return PL_raise_exception(exception);
  }
}


/**
 * @name static int get_handle_index_from_object_in_chained_list(generic_linked_list_object *root, unsigned int *ref_handle_index_found, generic_linked_list_object *object_searched)
 *
 * @description
 * This function will browse the chained list originating from the root pointer given in argument.
 * It will look for the first object which address corresponds to the one searched (given via the pointer object_searched). The handle index of the matching object will be returned within ref_handle_index_found which is an output of this function (thus passed as reference)
 * Note: this function can be applied to any chained list containing elements that comply with the pattern defined in generic_linked_list_object, it will not handle very well chained-list containing loops (but this is not supported by most of the functions anyway), that is to say that it will send the handle_index corresponding of the first time an object is found, even if it actually has several periodic handle indexes!
 * The root pointer might thus need to be casted to the generic_linked_list_object type when giving it as parameter for this function
 * The object_searched pointer will also need to be casted to the generic_linked_list_object type
 * Note: if no element in the chained list matches the object_searched pointer, a ref_handle_index_found=-1 will be returned.
 * The value returned by this function is an integer giving the status of the search:
 * (1) root == NULL
 * (2) The specified pointer to the object searched could not be found in the list
 * (0) indicates that the object has been found and its handle_index is returned in *ref_handle_index_found
**/

static int get_handle_index_from_object_in_chained_list(generic_linked_list_object *root, unsigned int *ref_handle_index_found, generic_linked_list_object *object_searched) {

unsigned int current_handle;


  current_handle = (unsigned int)-1;

  if (root == NULL) {
    if (ref_handle_index_found != NULL) *ref_handle_index_found = (unsigned int)-1; /* root doesn't point anywhere, return -1 (1) */
    return 1;
  }

#define current_object root
/* Note: the above define directive is done to make the code easier to read. We avoid to declare another generic_linked_list_object pointer by reusing root to browse the whole list, however, using root as the variable name can be confusing, so we rename it into current_object, but the compiler will interpret it as root for the part of the code below */
/* If the compiler used doesn't like this #define directive, just replace all references to "current_object" with "root" in the following part (between #define and #undef and comment the two aforementionned directives) */
  while (current_object != NULL) {
    if (current_object == object_searched) {
      if (ref_handle_index_found != NULL) *ref_handle_index_found = current_handle; /* We found our object in the list, return its handle_index */
      return 0; /* (0) */
    }
    else {
      current_handle += current_object->nb_jumped_index; /* Add the amount of empty spaces available between the current object and the next object */
      current_handle++; /* Increment the handle because we are going to work on the successor in the list */
      current_object = current_object->next; /* Work on the successor */
    }
  }
  if (ref_handle_index_found != NULL) *ref_handle_index_found = (unsigned int)-1; /* The object could not be found in the list (2) */
  return 2;
#undef current_object
}


/**
 * @name static int get_handle_index_from_object_in_chained_list_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, unsigned int *ref_handle_index_found, generic_linked_list_object *object_searched)
 *
 * @description
 * This is a front-end function to get_handle_index_from_object_in_chained_list (called C subfunction in the text below)
 * This function will, in addition to call the C subfunction, handle exceptions from the int result returned by the subfunction
 * The parameters are the same as the C subfunction, except that two extra first parameter are required.
 * The first one (exception_title) holds the title of the exception (null terminated C-style string), which will be used in the exception term created if needed
 * The second one (ref_ptr_to_result) is the address to an int where the result returned by the C subfunction, in case a fine processing is needed, depending on the cause of the failure (note, if the pointer is set to NULL, no value will be stored there)
 * If this function returns with TRUE=1 (using PL_succeed) if everything was fine
 * It returns FALSE=0 (using return PL_raise_exception()), and, in order to pass the exception to Prolog, the calling function will just needs to fail.
**/

static int get_handle_index_from_object_in_chained_list_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, unsigned int *ref_handle_index_found, generic_linked_list_object *object_searched) {

int        result;
term_t     exception;

  result = get_handle_index_from_object_in_chained_list(root, ref_handle_index_found, object_searched);
  if (ref_ptr_to_result_value != NULL) *ref_ptr_to_result_value = result;
  switch (result) {
  case 0: /* Function executed successfully */
    PL_succeed;
  case 2:
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, exception_title,
		  PL_CHARS, "reference_erased");
    return PL_raise_exception(exception);
  case 1:
  default:
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, exception_title,
		  PL_CHARS, "list_corrupted");
    return PL_raise_exception(exception);
  }
}


/**
 * @name static int create_object_in_chained_list(generic_linked_list_object *root, size_t object_size_t, unsigned int max_handle_index, unsigned int *ref_new_handle_index, generic_linked_list_object **ref_ptr_to_new_object)
 *
 * @description
 * This function will create a new object inside the required chained list.
 * A new object will be allocated in the memory for the new element of the list, thus, this function needs to know the size_t of objects in this list (this varies from one list to another)
 * The maximum value accepted for handle indexes has to be provided in max_handle_index (unsigned int). If max_handle_index==-1, all possible integers will be allowed.
 * The handle_index for the new element created is returned in ref_new_handle_index (unsigned int variable passed by reference)
 * In ref_ptr_to_new_object, this function will return a generic_link_list_object pointer to the new element created. This pointer will be saved in the address pointed by ref_ptr_to_new_object. This might thus need to be casted when used in the calling function
 * If NULL is returned in ref_ptr_to_new_object, the meaning will be given in the int returned by the procedure (see tags in the code):
 * (1) root == NULL
 * (2) max_handle_index == 0
 * (3) No free object with a handle <= max_handle_index
 * (4) We find a free handle for the new object but the previous_object pointer is NULL before trying to allocate the new object (this should never occur, because we have tested that root!=NULL in (1), and previous_pointer can only be update with previous_object->next, in the while loop, but the while loop first makes sure that previous_object->next!=NULL)
 * (5) Allocation for the new object failed (malloc)
 * (0) will mean that the function executed successfully
 * Note: this function is not thread-safe, which means that two call of this function at the same time may allocate two objects at the same position in the list
**/

static int create_object_in_chained_list(generic_linked_list_object *root, size_t object_size_t, unsigned int max_handle_index, unsigned int *ref_new_handle_index, generic_linked_list_object **ref_ptr_to_new_object) {

unsigned int                previous_object_handle_index; /* handle index for previous object */
generic_linked_list_object  *previous_object; /* Object preceeding the free space in the list */
generic_linked_list_object  *next_object; /* Object following the free space in the list */
generic_linked_list_object  *new_object; /* New object created in the the free space */
  

  if (root == NULL) {
    *ref_ptr_to_new_object = NULL; /* root doesn't point anywhere, return NULL (1) */
    return 1;
  }
  if (max_handle_index == 0) {
    *ref_ptr_to_new_object = NULL; /* or max_handle_index == 0, return NULL (2) */
    return 2;
  }

  previous_object_handle_index = (unsigned int)-1; /* Next possible handle is at least 0, so the previous object's handle is at least -1 */
  previous_object = root; /* Start from the root */
  next_object = NULL; /* No next object found yet */

  while (previous_object->next != NULL) { /* Loop until we find the end of the list */
    //  //@@ Breakpoint 3.1 went inside while loop\nto look for free handle indexes //Lionel!!!

    if (previous_object->nb_jumped_index != 0) { /* There is a successor (see line above) but we have an empty space between this object and the successor */
      /* The free handle corresponds to the previous_object + 1 (next space in the list, that is free). This will be done after exiting the loop (see below) */
      next_object=previous_object->next; /* Note: next can't be NULL, see while condition above */
      break; /* Go out of the while loop */
    }

    previous_object = previous_object->next;
    previous_object_handle_index++; /* Note that the handle index can only be incremented by one (we are sure that nb_jumped_index==0 because of the if statement above) */
    if (previous_object_handle_index >= max_handle_index) { /* We have reached the maximum value allowed for handle_index. Exit */
    /* Note: we don't even try to loop once more, because, even if we keep previous_object_handle_index==max_handle_index, new_handle_index will overflow when going out of the loop (see below) */
      *ref_ptr_to_new_object = NULL; /* No free object with a handle <= max_handle_index, return NULL (3) */
      return 3;
    }
  }
  *ref_new_handle_index = previous_object_handle_index + 1;
  //  //@! "Breakpoint 4 found free handle stored as %d", *ref_new_handle_index //Lionel!!!


/* When we arrive here, we have two solutions:

1) There was an empty space in the list, between two existing objects.
We then have the scenario:
Handle Index: new_handle_index-1     new_handle_index
Pointer:      previous_object        Free space         +-> next_object
Content:      next -------------------------------------+   next ---> ?
              nb_jumped_index>0 (eg =3)                     nb_jumped_index=?


2) There was no empty space in the list. We have the following scenario:
Handle Index: new_handle_index-1     new_handle_index
Pointer:      previous_object        Free space
Content:      next ----------------> NULL
              nb_jumped_index=0
and
next_object -> NULL
*/




  if ( previous_object == NULL ) { /* The previous object has not been found properly (this should never happen) */
    *ref_ptr_to_new_object=NULL; /* Return NULL (4) */
    return 4;
  }

  new_object = malloc(object_size_t); /* Allocate memory for the new object and stores the address of this element in root */

  if ( new_object == NULL ) {
    *ref_ptr_to_new_object=NULL; /* Allocation failed, return NULL (5) */
    return 5;
  }

  previous_object->next = new_object;

/* When we arrive here, we still have our two solutions:

1) There was an empty space in the list, between two existing objects.
Handle Index: new_handle_index-1     new_handle_index
Pointer:      previous_object    +-> new_object             next_object
Content:      next --------------+                          next ---> ?
              nb_jumped_index=3                             nb_jumped_index=?


2) There was no empty space in the list. We have the following scenario:
Handle Index: new_handle_index-1     new_handle_index
Pointer:      previous_object    +-> new_object
Content:      next --------------+
              nb_jumped_index=0
and
next_object -> NULL
*/




  new_object->next=next_object; /* The new object has next_object as successor in the list (next_object could also be NULL) */

  if (new_object->next == NULL) { /* Are we the tail of the list? */
    //  //@@ New object was added at tail //Lionel!!!
    new_object->nb_jumped_index=0; /* If yes, nb_jumped index has no meaning, set it to 0 by convention */
  }
  else { /* We are not at the tail of the list (there is a successor) */
    new_object->nb_jumped_index=previous_object->nb_jumped_index-1; /* Inherit nb_jumped_index from the previous object, removing one, given that new_object is not counted in the jumped objects anymore */
    //  //@! "New object was not added at tail. It's nb_jumped_index=%d", new_object->nb_jumped_index //Lionel!!!
  }
  previous_object->nb_jumped_index=0; /* The previous object will have a direct successor, which is us (we always create the new object in the very first empty space available in the list) */

/* When we arrive here, we now have two solutions:

1) There was an empty space in the list, between two existing objects.
Handle Index: new_handle_index-1     new_handle_index
Pointer:      previous_object    +-> new_object         +-> next_object
Content:      next --------------+   next --------------+   next ---> ?
              nb_jumped_index=0      nb_jumped_index=2      nb_jumped_index=?


2) There was no empty space in the list. We have the following scenario:
Handle Index: new_handle_index-1     new_handle_index
Pointer:      previous_object    +-> new_object
Content:      next --------------+   next ----------------> NULL
              nb_jumped_index=0      nb_jumped_index=0
*/


  new_object->count_references = 0; /* This is a brand new object. No reference to it exists yet */

  *ref_ptr_to_new_object=new_object; /* Return a pointer to the new object create in the list */
  return 0; /* Function executed successfully. Return 0 */

 /* Note: the value in *new_handle_index is already up to date (see above) */
}


/**
 * @name static int create_object_in_chained_list_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, size_t object_size_t, unsigned int max_handle_index, unsigned int *ref_new_handle_index, generic_linked_list_object **ref_ptr_to_new_object)
 *
 * @description
 * This is a front-end function to create_object_in_chained_list (called C subfunction in the text below)
 * This function will, in addition to call the C subfunction, handle exceptions from the int result returned by the subfunction
 * The parameters are the same as the C subfunction, except that two extra first parameter are required.
 * The first one (exception_title) holds the title of the exception (null terminated C-style string), which will be used in the exception term created if needed
 * The second one (ref_ptr_to_result) is the address to an int where the result returned by the C subfunction, in case a fine processing is needed, depending on the cause of the failure (note, if the pointer is set to NULL, no value will be stored there)
 * If this function returns with TRUE=1 (using PL_succeed) if everything was fine
 * It returns FALSE=0 (using return PL_raise_exception()), and, in order to pass the exception to Prolog, the calling function will just needs to fail.
**/

static int create_object_in_chained_list_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, size_t object_size_t, unsigned int max_handle_index, unsigned int *ref_new_handle_index, generic_linked_list_object **ref_ptr_to_new_object) {

int      result;
term_t   exception;

  result = create_object_in_chained_list(root, object_size_t, max_handle_index, ref_new_handle_index, ref_ptr_to_new_object);
  if (ref_ptr_to_result_value != NULL) *ref_ptr_to_result_value = result;
  if (result == 0 && ref_ptr_to_new_object != NULL) /* Function executed successfully */
    PL_succeed;
  
  switch (result) {
  case 1:
    exception = PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, exception_title,
                  PL_CHARS, "list_corrupted");
    return PL_raise_exception(exception);
  case 2:
  case 3:
    exception = PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, exception_title,
                  PL_CHARS, "too_many");
    return PL_raise_exception(exception);
  case 5:
    exception = PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, exception_title,
                  PL_CHARS, "not_enough_memory");
    return PL_raise_exception(exception);
  case 4:
  default:
    exception = PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, exception_title,
                  PL_CHARS, "cant_create");
    return PL_raise_exception(exception);
  }
}


/**
 * @name static int delete_object_in_chained_list_with_payload_handling(generic_linked_list_object *root, unsined int target_handle_index, void (*payload_handling_procedure)(generic_linked_list_object *))
 *
 * @description
 * This function will delete an object inside the required chained list. The object to delete is specified using its handle index * The second parameter is a C procedure that will be called with every object inside the list before they are deallocated. The only parameter will be a pointer on the generic_linked_list_object that is going to be deallocated
 * The second parameter is a C procedure that will be called with the object to delete from the list before it is deallocated. The only parameter will be a pointer on the generic_linked_list_object that is going to be deallocated
 * An int is returned to give information about the success or not of the procedure (see tags in the code). The possible values are:
 * (1) root == NULL
 * (2) Requested index does not exist
 * (3) target_handle_index == -1 (can't delete the root)
 * (4) count_references != 0 (this object is still referenced by another object somewhere)
 * (0) means that the function executed successfully
 * Note: this function is not thread-safe.
 * Warning: this function deletes an linked object in a chained list, and releases the memory for the generic_linked_list_object object. It also allows to clean the object in the list before being deleted. This is done by a call to the procedure payload_handling_procedure
 * If no payload_handling_procedure is needed because all data inside the object can be lost without problem, the procedure delete_object_in_chained_list should be call rather than delete_object_in_chained_list_with_payload_handling
**/

static int delete_object_in_chained_list_with_payload_handling(generic_linked_list_object *root, unsigned int target_handle_index, void (*payload_handling_procedure)(generic_linked_list_object *)) {

unsigned int                current_handle;
generic_linked_list_object  *previous_object; /* Used to store a pointer to each object that will be deleted */
generic_linked_list_object  *current_object; /* Object to browse the list */


  current_handle = (unsigned int)-1;

  if (root == NULL)
    return 1; /* root = NULL, can't parse the chained list, return will error (1) */

  if ( target_handle_index == (unsigned int)-1 ) /* We can't delete the root (index -1). Exit with an error (3) */
    return 3;

  current_object = root;
  previous_object = NULL; /* No previous object yet. Note, current_object is not NULL (root!=NULL as tested above), and target_handle_index != -1 (tested above as well) so at least one of the loop blocks below will be executed, previous_object will thus be updated and not stay == NULL. */

  do {
    if (current_handle == target_handle_index) {
      /* If this part is executed, we have found the requested index, its pointer is in current_object and is != NULL . We have its predecessor in the previous_object pointer */

      /* We first check that we can safely delete this object, by making sure that no other object has references to it */
      if (current_object->count_references != 0) { /* We can't delete this object yet... there are still existing references to it */
        return 4;
      }

      previous_object->next = current_object->next; /* Link the predecessor with the current successor. We have created a shortcut in the list, stripping the current object from the chain */
      previous_object->nb_jumped_index++; /* There is one more free space between the predecessor and our successor, which is the free cell that results of deleting the current_object from the chained list */
      previous_object->nb_jumped_index += current_object->nb_jumped_index; /* Add the nb_jumped_index of the object we are going to delete */
      if (payload_handling_procedure != NULL) /* Don't call the payload procedure if the reference goes nowhere (NULL) */
        payload_handling_procedure(current_object); /* Allow to clean the payload of this object first */
      free(current_object); /* Free the memory used by the current object */
      return 0; /* Deallocation was successfull */
    }
    else {
      current_handle += current_object->nb_jumped_index; /* Add the amount of empty spaces available between the current object and the next object */
      current_handle++; /* Increment the handle because we are going to work on the successor in the list */

      previous_object = current_object;
      current_object = current_object->next; /* Work on the successor */
    }
  } while (current_object != NULL);
  return 2;
}


/**
 * @name static int delete_object_in_chained_list_with_payload_handling_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, unsined int target_handle_index, void (*payload_handling_procedure)(generic_linked_list_object *))
 *
 * @description
 * This is a front-end function to delete_object_in_chained_list_with_payload_handling (called C subfunction in the text below)
 * This function will, in addition to call the C subfunction, handle exceptions from the int result returned by the subfunction
 * The parameters are the same as the C subfunction, except that two extra first parameter are required.
 * The first one (exception_title) holds the title of the exception (null terminated C-style string), which will be used in the exception term created if needed
 * The second one (ref_ptr_to_result) is the address to an int where the result returned by the C subfunction, in case a fine processing is needed, depending on the cause of the failure (note, if the pointer is set to NULL, no value will be stored there)
 * If this function returns with TRUE=1 (using PL_succeed) if everything was fine
 * It returns FALSE=0 (using return PL_raise_exception()), and, in order to pass the exception to Prolog, the calling function will just needs to fail.
**/

static int delete_object_in_chained_list_with_payload_handling_with_exception_handling(char *exception_title, int *ref_ptr_to_result_value, generic_linked_list_object *root, unsigned int target_handle_index, void (*payload_handling_procedure)(generic_linked_list_object *)) {

int      result;
term_t   exception;

  result = delete_object_in_chained_list_with_payload_handling(root, target_handle_index, payload_handling_procedure);
  if (ref_ptr_to_result_value != NULL) *ref_ptr_to_result_value = result;
  if (result == 0) { /* Function executed successfully */
    check_leak_and_raise_exceptions_macro(result, exception); /* See the declaration for this macro at the beginning of this file */
    PL_succeed;
  }

  switch (result) {
  case 1:
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, exception_title,
		  PL_CHARS, "list_corrupted");
    return PL_raise_exception(exception);
  case 2:
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, exception_title,
		  PL_CHARS, "empty_index");
    return PL_raise_exception(exception);
  case 4:
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, exception_title,
		  PL_CHARS, "still_referenced");
    return PL_raise_exception(exception);
  case 3:
  default:
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, exception_title,
		  PL_CHARS, "cant_delete");
    return PL_raise_exception(exception);
  }
}


/**
 * @name static int delete_object_in_chained_list(generic_linked_list_object *root, unsigned int target_handle_index)
 *
 * @description
 * This function will delete an object inside the required chained list. The object to delete is specified using its handle index
 * An int is returned to give information about the success or not of the procedure.
 * For more information about the return codes, see function delete_object_in_chained_list_with_payload_handling (see tags in the code)
 * Note: this function is not thread-safe because during the moment when this procedure relinks the two objects besides the one deleted, the list will not be usable
 * Warning: this function deletes an linked object in a chained list, and releases the memory for the generic_linked_list_object object, however, it's up to the caller to free up any memory related to a pointer contained in the payload of this generic_linked_list_object object.
 * For this reason, the caller needs to free up all memory spaces pointed by the content of the generic_linked_list_object object that we are going to delete here, since there won't be any way to get these pointer after executing delete_object_in_chained_list
 * An alternative will be to use the procedure delete_object_in_chained_list_with_payload_handling that allows to call a function to free up the necessary bits of memory on a per-linked-object basis
**/

static int delete_object_in_chained_list(generic_linked_list_object *root, unsigned int target_handle_index) {

  return delete_object_in_chained_list_with_payload_handling(root, target_handle_index, NULL);
}


/**
 * @name static int delete_all_objects_in_chained_list_with_payload_handling(generic_linked_list_object *root, void (*payload_handling_procedure)(generic_linked_list_object *))
 *
 * @description
 * This function will delete all objects contained in a chained list. The root parameter is a pointer on the generic_linked_list_object root for the chained list
 * The second parameter is a C procedure that will be called with every object inside the list before they are deallocated. The only parameter will be a pointer on the generic_linked_list_object that is going to be deallocated
 * An int is returned to give information about the success or not of the procedure (see tags in the code). The possible values are:
 * (1) root == NULL
 * (0) means that the function executed successfully
 * Note: this function is thread-safe
 * Warning: this function deletes an entire a chained list, and releases the memory for the generic_linked_list_object object. It also allows to clean the objects in the list before being deleted. This is done by a call to the procedure payload_handling_procedure
 * If no payload_handling_procedure is needed because all data inside the object can be lost without problem, the procedure delete_all_objects_in_chained_list should be call rather than delete_all_objects_in_chained_list_with_payload_handling
 * Warning: this function does NOT take count_references into account, which means that the normal safety mechanism of references is not guanranteed after calling this function
**/

// Not allowed in interface as this can lead to corruption in the chained-list structures. Lionel 20020206
//static int delete_all_objects_in_chained_list_with_payload_handling(generic_linked_list_object *root, void (*payload_handling_procedure)(generic_linked_list_object *)) {
//
//generic_linked_list_object  *object_to_delete; /* Used to store a pointer to each object that will be deleted */
//generic_linked_list_object  *current_object; /* Object to browse the list */
//
//
//  if (root == NULL)
//    return 1; /* root = NULL, can't parse the chained list, return will error (1) */
//  
//  current_object = root->next; /* Note: root != NULL because of the test above. This is thus legal */
//  root->next = NULL; /* Reinitialise the root to point to null. At this moment, any other function in this code can manipulate the list again safely */
//  root->nb_jumped_index = 0; /* nb_jumped_index also reinitialised to 0. The list is now completely empty (actually, the tail of the list has been unlinked from the root) */
//
///* We now go through the remaining of the list to free up the memory and objects */
//
//  object_to_delete = NULL; /* No object to delete yet. Note, current_object is not NULL (root!=NULL as tested above) */
//
//  while (current_object != NULL) { /* If there is still an object in the list */
//    object_to_delete = current_object; /* Store the pointer to the object that we're going to delete in object_to_delete */
////  //@! "Object to delete is %p", object_to_delete //Lionel!!!
//    current_object = current_object->next; /* We modify current_object to point to the next object in the chained list */
//
////  //@! "Going to call payload_handling_procedure(%p);", object_to_delete //Lionel!!!
//    if (payload_handling_procedure != NULL) /* Don't call the payload procedure if the reference goes nowhere (NULL) */
//      payload_handling_procedure(object_to_delete); /* Allow to clean the payload of the object to delete first */
////  //@! "Going to free the memory allocated at %p", object_to_delete //Lionel!!!
//    free(object_to_delete); /* Free the memory used by object_to_delete, and loop again to look for more objects in the chained list (using current_object) */
//    /* Note: this function does NOT take count_references into account, which means that the normal safety mechanism of references is not guanranteed after calling this function */
//  }
//  return 0; /* All list deleted successfully, return with error (0) */
//}


/**
 * @name static int delete_all_objects_in_chained_list(generic_linked_list_object *root)
 *
 * @description
 * This function will delete all objects contained in a chained list. The only parameter needed is a pointer on the generic_linked_list_object root for the chained list
 * An int is returned to give information about the success or not of the procedure.
 * For more information about the return codes, see function delete_all_objects_in_chained_list_with_payload_handling (see tags in the code)
 * Note: this function is thread-safe
 * Warning: this function deletes an entire a chained list, and releases the memory for the generic_linked_list_object object, however, it's up to the caller to free up any memory related to all pointers contained in all payloads of the whole generic_linked_list_object objects contained by the list.
 * For this reason, the caller needs to free up all memory spaces pointed by the content of all generic_linked_list_object objects that belong to the list we are going to delete here, since there won't be any way to get these pointer after deleting the list with delete_all_objects_in_chained_list
 * An alternative will be to use the procedure delete_all_objects_in_chained_list_with_payload_handling that allows to call a function to free up the necessary bits of memory on a per-linked-object basis
 * Warning: this function does NOT take count_references into account, which meas that the normal safety mechanism of references is not guanranteed after calling this function
**/

// Not allowed in the interface. This will lead to corruption in most of the cases because objects that are still referenced will still be removed
//static int delete_all_objects_in_chained_list(generic_linked_list_object *root) {
//
//  delete_all_objects_in_chained_list_with_payload_handling(root, NULL); /* Reuse the code of the payload handling procedure */
//}


/**
 * @name static int get_next_handle_index_in_chained_list(generic_linked_list_object *root, unsigned int previous_object_handle_index, unsigned int *ref_next_handle_index_found, generic_linked_list_object **ref_ptr_to_object_found)
 *
 * @description
 * This function searches the handle index of the object in the list that immediately follows the object which handle index has been given in previous_object_handle_index
 * The value found (unsigned int) is returned inside the next_handle_index_found, which thus needs to be passed by reference
 * An int is returned to give information about the success or not of the procedure (see tags in the code). The possible values are:
 * (1) root == NULL
 * (2) The index referenced by previous_object_handle_index could not be found in the list. The handle index returned in ref_next_handle_index_found will then be the first one that could be found with a handle index>previous_object_handle_index, and *ref_ptr_to_object_found will points to the related chained object.
 * (3) there is no further object in the list (the handle index returned is -1, and the pointer is NULL)
 * (0) means that the function executed successfully (the next handle in the chained list is returned in *ref_next_handle_index_found, and a pointer to this object can be found using *ref_ptr_to_object_found)
 * Note: this function is thread-safe
**/

static int get_next_handle_index_in_chained_list(generic_linked_list_object *root, unsigned int previous_object_handle_index, unsigned int *ref_next_handle_index_found, generic_linked_list_object **ref_ptr_to_object_found) {

unsigned int                current_handle;
generic_linked_list_object  *current_object;


  current_handle = (unsigned int)-1;

  if (root == NULL) {
    if (ref_next_handle_index_found != NULL) *ref_next_handle_index_found = (unsigned int)-1; /* Return -1 as the handle index */
    if (ref_ptr_to_object_found != NULL) *ref_ptr_to_object_found = NULL; /* root doesn't point anywhere, return NULL (1) */
    return 1;
  }
  current_object = root; /* Start from the root */

  while (current_object != NULL) {
    if (current_handle == previous_object_handle_index) {
      //  //@! "Breakpoint 3 handle %d found", previous_object_handle_index //Lionel!!!
      /* We found the object with the index corresponding to previous_object_handle_index */
      if (current_object->next != NULL) { /* There is an object after... which is the one we want to return */
        current_handle += current_object->nb_jumped_index;
        current_handle++; /* current_handle now contains the handle of the next object in the chained-list */
	//  //@! "Breakpoint 4.1 next object has handle %d", current_handle //Lionel!!!
        if (ref_next_handle_index_found != NULL) *ref_next_handle_index_found = current_handle; /* Return this handle index if ref_next_handle_index_found is a valid reference to an int */
        if (ref_ptr_to_object_found != NULL) *ref_ptr_to_object_found = current_object->next; /* We found the next object in the list, so return it */
        return 0; /* (0) */
      }
      else { /* We have found the object referenced by previous_object_handle_index but it's the last one of the list. We thus can't return any reference to its successor (2) */ 
	//  //@@ Breakpoint 4.2 no other object in list //Lionel!!!
        if (ref_next_handle_index_found != NULL) *ref_next_handle_index_found = (unsigned int)-1; /* Return -1 as the handle index */
        if (ref_ptr_to_object_found != NULL) *ref_ptr_to_object_found = NULL; /* We found no successor to the object referenced, return NULL */
        return 3;
      }
    }
    else {
      current_handle += current_object->nb_jumped_index; /* Add the amount of empty spaces available between the current object and the next object */
      current_handle++; /* Increment the handle because we are going to work on the successor in the list */
      current_object = current_object->next;
      if (current_handle > previous_object_handle_index) { /* We have gone too far in the chained list because the current_handle is greater than the one searched in previous_object_handle_index. We won't come here with current_handle=-1 (as it is intially set) because it is incremented beforehand. We are thus sure not to exit the loop by mistake without having done anything */
        if (current_object == NULL) { /* There is no next object anyway... this was the end of the list */
	  //  //@@ Breakpoint 4.3 no other object in list //Lionel!!!
          if (ref_next_handle_index_found != NULL) *ref_next_handle_index_found = (unsigned int)-1; /* Return -1 as the handle index */
          if (ref_ptr_to_object_found != NULL) *ref_ptr_to_object_found = NULL; /* We found no successor to the object referenced, return NULL */
          return 3;
        }
        else {
	  //  //@! "Breakpoint 4.4 passed the provided starting object handle %d. Following handle is %d", previous_object_handle_index, current_handle //Lionel!!!
          if (ref_next_handle_index_found != NULL) *ref_next_handle_index_found = current_handle; /* Return current_handle as the handle index */
          if (ref_ptr_to_object_found != NULL) *ref_ptr_to_object_found = current_object; /* Return the object corresponding to the handle index (2) */
          return 2;
        }
      }
      /* If we arrive here, we have already updated current_handle and current_object to the next object, so we will work on the successor during the next loop */
    }
  }
  if (ref_next_handle_index_found != NULL) *ref_next_handle_index_found = current_handle; /* The index could not be found in the list (2) */
  if (ref_ptr_to_object_found != NULL) *ref_ptr_to_object_found = NULL; /* Update the object found pointer to NULL */
  return 2;
}


/**
 * @name foreign_t unify_handle_with_index(functor_t base_functor, term_t handle_out, int index_in)
 *
 * @description
 * This function will unify the handle_out term with a handle to the required index and functor
 * Handles are basically just created from the template: base_functor(Index) , replacing Index by the index_in value and base_functor by the functor passed as parameter
**/

foreign_t unify_handle_with_index(functor_t base_functor, term_t handle_out, int index_in) {

term_t index_in_term;

  
  if (PL_unify_functor(handle_out, base_functor)) {
    index_in_term = PL_new_term_ref();
    PL_get_arg(1, handle_out, index_in_term);
    return (PL_unify_integer(index_in_term, index_in));
  }
  else {
    PL_fail;
  }
}


/**
 * @name foreign_t get_index_from_handle(functor_t base_functor, term_t handle_in, int *ref_index_out)
 *
 * @description
 * This function will extract the index from the handle_in term and return it inside ref_index_out (passed as a reference)
 * Note: this function is thread-safe
**/

foreign_t get_index_from_handle(functor_t base_functor, term_t handle_in, unsigned int *ref_index_out) {

term_t index_out_term;
int result;

  
  if (PL_is_functor(handle_in, base_functor)) { /* Check if handle follows the base_functor(...) template */
    index_out_term = PL_new_term_ref(); /* Create a term to contain the integer */
    if (!PL_get_arg(1, handle_in, index_out_term))
      PL_fail; /* Error, the handle doesn't follow the right template */
    if (!PL_get_integer(index_out_term, &result))
      PL_fail; /* Error, the parameter for the handle is not an integer! */
  }
  else {
    PL_fail; /* Error, the handle is invalid because it is not a '$functor'/1 -type based compound */
  }
  if (result<0) {
    PL_fail; /* Error, the handle is invalid because it uses a signed and negative integer */
  }
  *ref_index_out = (unsigned int)result;
  PL_succeed; /* Successfully executed. Return the int of the handle inside *ref_index_out */
}

 
/**
 * @name pl_create_dictionary(term_t t_dictionary_name, term_t t_pp_knowledge_name, term_t t_cons_knowledge_name, term_t t_affix_file_name, term_t dictionary_handle)
 * @prologname create_dictionary/5
 *
 * @description
 * This function creates a new dictionary with the 4 first arguments as database filenames (Dictionary filename, Post processing filename, Constituents filename, Affix filename)
**/

foreign_t pl_create_dictionary(term_t t_dictionary_name,
			       term_t t_pp_knowledge_name,
			       term_t t_cons_knowledge_name,
			       term_t t_affix_file_name,
			       term_t dictionary_handle) {

  //@- //Lionel!!!
term_t                  exception; /* Handle for an possible exception */
unsigned int            new_handle_index; /* Variable to store the handle index referencing the new dictionary in the chained list */
Dictionary              new_dictionary; /* Space to store the new dictionary created */
char                    *dictionary_name, *pp_knowledge_name, *cons_knowledge_name, *affix_file_name; /* Strings got from the text atom parameters */
dict_linked_list_object *chained_new_dictionary_object;



  if (!PL_get_atom_chars(t_dictionary_name, &dictionary_name) ||
      !PL_get_atom_chars(t_pp_knowledge_name, &pp_knowledge_name) ||
      !PL_get_atom_chars(t_cons_knowledge_name, &cons_knowledge_name) ||
      !PL_get_atom_chars(t_affix_file_name, &affix_file_name)) { /* One of the atoms given as parameter is unbound or couldn't be converted to string properly... we will raise a Prolog error and exit */
    exception=PL_new_term_ref(); /* Create a new exception object */
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, "dictionary",
                  PL_CHARS, "instanciation_fault"); /* Fill-in the exception object */
    return PL_raise_exception(exception); /* Raise the exception and exit */
  }

  new_dictionary = dictionary_create(dictionary_name,
                                     pp_knowledge_name,
                                     cons_knowledge_name,
                                     affix_file_name);
  /* The above line will create the dictionary, using the 4 filenames corresponding to the dictionary, the post-processing knowledge database, the constituent database and the affix database */

  if (new_dictionary == NULL) { /* Check if the dictionary has been successfully created. If not, raise a Prolog exception */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "dictionary",
		  PL_CHARS, "cant_register");
    return PL_raise_exception(exception);
  }

  if (!create_object_in_chained_list_with_exception_handling("dictionary", NULL,
                                                             root_dict_list, /* Root for the dictionary chained-list. This doesn't need to be casted because roots are generic objects */
                                                             sizeof(dict_linked_list_object),
                                                             NB_DICTIONARIES-1,
                                                             &new_handle_index,
                                                             (generic_linked_list_object **)&chained_new_dictionary_object)) {
    PL_fail; /* create_object_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }

  /* When execution reaches this point, the new Dictionary object has been created in LGP */
  /* There is an object allocated in the list (chained_new_dictionary_object) for the new dictionary, which points to the relevant Dictionary object using the ->payload field */
  /* We now have to send back a handle to this dictionary */

  if (!unify_handle_with_index(FUNCTOR_dictionary1, dictionary_handle, new_handle_index)) { /* The following block of instruction is to be executed if the unification fails (the handle can't be created properly) */
    dictionary_delete(new_dictionary); /* Delete the dictionary object in the lgp API */

    delete_object_in_chained_list(root_dict_list, new_handle_index); /* Remove the dictionary from the chained list because this dictionary object has been erased. We don't grab the return value from this function because the exception we will raise will anyway be related to the handle we can't create */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "dictionary",
		  PL_CHARS, "cant_create_handle");
    return PL_raise_exception(exception);
  }
  else {
    chained_new_dictionary_object->payload = new_dictionary; /* Store a pointer to the dictionary inside the payload of the new object in the chained-list */
    //  //@! "Breakpoint Dictionary object allocated at address=%p", new_dictionary //Lionel!!!
    PL_succeed; /* The handle has been successfully created, so succeed */
    /* There is now a new dictionary object created inside the lgp library, as well as an object in the chained list starting from root_dict_list */
  }
}


/**
 * @name void delete_dictionary_object_payload(dict_linked_list_object *dict_object)
 *
 * @description
 * This procedure will free up the memory for the dictionary pointer contained in a dictionary-chained-list object (dict_object)
 * This is made in a way that it is called as the "payload_handling_procedure" procedure of a call to the delete_[all_]object[s]_in_chained_list_with_payload_handling (check these procedures above for more information)
**/

void delete_dictionary_object_payload(dict_linked_list_object *dict_object) {

  if (dict_object->payload != NULL) {
    dictionary_delete(dict_object->payload); /* Delete the dictionary in the lgp API */
    dict_object->payload = NULL; /* Reset the pointer to the payload (that doesn't exist anymore!) */
  }
}


/**
 * @name pl_delete_dictionary(term_t dictionary_handle)
 * @prologname delete_dictionary/1
 *
 * @description
 * This function will destroy the dictionary which handle is dictionary_handle from the memory
**/

foreign_t pl_delete_dictionary(term_t dictionary_handle) {

term_t       exception;
unsigned int handle_index_to_delete;

  
  if (!get_index_from_handle(FUNCTOR_dictionary1, dictionary_handle, &handle_index_to_delete)) {
    exception=PL_new_term_ref(); /* The handle could not be changed into an integer */
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "dictionary",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    return delete_object_in_chained_list_with_payload_handling_with_exception_handling("dictionary", NULL,
										       root_dict_list,
										       handle_index_to_delete,
										       (void (*)(generic_linked_list_object *))delete_dictionary_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the dictionary payload at the same time */
  /* If delete_object_in_chained_list_with_payload_handling fails, we will fail as well and thus return the exception created within delete_object_in_chained_list_with_payload_handling */
  }
}


/**
 * @name pl_delete_all_dictionaries()
 * @prologname delete_all_dictionaries/0
 *
 * @description
 * This function will destroy all the dictionaries objects (both chained-list and internal to LGP)
**/

foreign_t pl_delete_all_dictionaries(void) {

term_t       exception;
int          result;
unsigned int current_handle;


  current_handle = -1;
  while (1) { /* This is an infinite loop. The exit will be when get_next_handle_index_in_chained_list returns 3 */
    result = get_next_handle_index_in_chained_list(root_dict_list, current_handle, &current_handle, NULL); /* Get the object following the last one handled in the loop */
    // //@! "Breakpoint 1 handle=%d has been returned by get_next_handle_index_in_chained_list", current_handle //Lionel!!!
    // //@! "Breakpoint 1.2 get_next_handle_index_in_chained_list sent a result=%d", result //Lionel!!!

    switch (result) {
    case 0:
    case 2:
      break; /* This is an acceptable result. Carry on with this handle index */
    case 1:
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		    PL_CHARS, "dictionary",
		    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* This is the exit of this infinite loop */
      check_leak_and_raise_exceptions_macro(result, exception); /* See the declaration for this macro at the beginning of this file */
      PL_succeed;
    }

    /* If we arrive here, we have a valid handle index in current_handle. We can thus try to delete this item from the list */
    result = delete_object_in_chained_list_with_payload_handling(root_dict_list,
                                                                 current_handle,
                                                                 (void (*)(generic_linked_list_object *))delete_dictionary_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the dictionary payload at the same time */
    switch (result) {
    case 2: /* This handle does not exist! */
      //@! "Breakpoint 2 handle=%d returned by get_next_handle_index_in_chained_list does not exist! Bug", current_handle //Lionel!!!
      break; /* Carry on processing the list */
    case 0: /* This object was successfully deleted */
      break;
    case 1: /* root == NULL */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "dictionary",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* current_handle == -1 */
      //@! "Breakpoint 3 handle returned=%d trying to delete the root! Bug", current_handle //Lionel!!!
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "dictionary",
                    PL_CHARS, "cant_delete");
      return PL_raise_exception(exception);
    case 4: /* Dictionary is still referenced */
      // //@! "Breakpoint 4 handle=%d won't delete because still referenced.", current_handle //Lionel!!!
      break;
    default:
      break; /* By default, try again for the next objects of the list */
    }
  }
}


/**
 * @name pl_get_nb_dictionaries(term_t nb_dictionaries)
 * @prologname get_nb_dictionaries/1
 *
 * @description
 * This function returns the number of dictionaries currently in memory
**/

foreign_t pl_get_nb_dictionaries(term_t nb_dictionaries) {
  
  return PL_unify_integer(nb_dictionaries, count_objects_in_chained_list(root_dict_list));
}


/**
 * @name pl_get_handles_nb_references_dictionaries(term_t handle_list, term_t count_references_list)
 * @prologname pl_get_handles_nb_references_dictionaries/2
 *
 * @description
 * This function returns a list containing all the existing handles of allocated dictionaries, and a second list containining integers of how many other objects reference the dictionary which handle is at the same position in the first list
 * For example, if dictionary with handle dict_A is referenced 3 times, and dictionary with handle dict_B is never referenced, the result of this function will be:
 * ([dict_A, dict_B], [3, 0]) or ([dict_B, dict_A], [0, 3])
**/

foreign_t pl_get_handles_nb_references_dictionaries(term_t handle_list, term_t count_references_list) {

unsigned int            last_handle;
term_t                  new_handle_element; /* This will contain the new handle term to insert in the list handle_list */
term_t                  new_count_references_element; /* This will contain the new count references term to insert in the list count_references_list */
term_t                  exception; /* Term used to create exceptions */
term_t                  constructed_count_references_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
term_t                  constructed_handle_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
dict_linked_list_object *dictionary; /* Reference to the current dictionary being processed in the chained list */


  PL_put_nil(constructed_handle_list); /* Create the tail of the list (which is []) */
  PL_put_nil(constructed_count_references_list); /* Create the tail of the list (which is []) */

  last_handle = (unsigned int)-1; /* Start to browse the chained-list from the beginning (root) */

  while ( get_next_handle_index_in_chained_list(root_dict_list, last_handle, &last_handle, (generic_linked_list_object**)(&dictionary)) == 0 ) { /* Find next handle from the chained-list */
    //  //@! "Breakpoint 2 new last_handle=%d found in loop", last_handle //Lionel!!!
    new_handle_element = PL_new_term_ref();
    if (!unify_handle_with_index(FUNCTOR_dictionary1, new_handle_element, last_handle)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "dictionary",
                    PL_CHARS, "cant_create_handle");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_handle_list, new_handle_element, constructed_handle_list);
    }
    new_count_references_element = PL_new_term_ref();
    if (!PL_unify_integer(new_count_references_element, dictionary->count_references)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "dictionary",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_count_references_list, new_count_references_element, constructed_count_references_list);
    }
  }
  return (PL_unify(handle_list, constructed_handle_list) && PL_unify(count_references_list, constructed_count_references_list)); /* The handle and count references lists have been successfully created, so unify with the parameters */
}


/**
 * @name pl_create_parse_options(term_t parse_options_handle)
 * @prologname create_parse_options/1
 *
 * @description
 * This function creates a new parse options structure
**/

foreign_t pl_create_parse_options(term_t parse_options_handle) {

term_t                  exception; /* Handle for an possible exception */
unsigned int            new_handle_index; /* Variable to store the handle index referencing the new parse options in the chained list */
Parse_Options           new_parse_options; /* The new parse options object created (internal LGP Parse_Options object) */
opts_linked_list_object *chained_new_parse_options_object;



  new_parse_options = parse_options_create(); /* Create a new parse options object in LGP */


  if (new_parse_options == NULL) { /* Check if the parse options structure has been successfully created. If not, raise a Prolog exception */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "cant_register");
    return PL_raise_exception(exception);
  }

  if (!create_object_in_chained_list_with_exception_handling("parse_options", NULL,
                                                             root_opts_list, /* Root for the parse options chained-list. This doesn't need to be casted because roots are generic objects */
                                                             sizeof(opts_linked_list_object),
                                                             NB_PARSE_OPTIONS-1,
                                                             &new_handle_index,
                                                             (generic_linked_list_object **)&chained_new_parse_options_object)) {
    PL_fail; /* create_object_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }

  /* When execution reaches this point, the new Parse_Options object has been created in LGP */
  /* There is an object allocated in the list (chained_new_parse_options_object) for the new parse options, which points to the relevant Parse_Options object using the ->payload field */
  /* We now have to send back a handle to this parse options */


  if (!unify_handle_with_index(FUNCTOR_options1, parse_options_handle, new_handle_index)) { /* The following block of instruction is to be executed if the unification fails (the handle can't be created properly) */
    parse_options_delete(new_parse_options);

    delete_object_in_chained_list(root_opts_list, new_handle_index); /* Remove the parse options from the chained list because this parse options object has been erased. We don't grab the return value from this function because the exception we will raise will anyway be related to the handle we can't create */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "cant_create_handle");
    return PL_raise_exception(exception);
  }
  else {
    chained_new_parse_options_object->payload = new_parse_options; /* Store a pointer to the parse options inside the payload of the new object in the chained-list */
    parse_options_set_verbosity(new_parse_options, 0);
    parse_options_set_echo_on(new_parse_options, FALSE);
    parse_options_set_display_on(new_parse_options, FALSE); /* Turn off all the display properties (because use as a DLL) */
    parse_options_set_display_postscript(new_parse_options, FALSE);
    parse_options_set_display_constituents(new_parse_options, FALSE);
    parse_options_set_display_bad(new_parse_options, FALSE);
    parse_options_set_display_links(new_parse_options, FALSE);
    parse_options_set_display_walls(new_parse_options, FALSE);
    parse_options_set_display_union(new_parse_options, FALSE);
    parse_options_reset_resources(new_parse_options);
    PL_succeed; /* The handle has been successfully created, so succeed */
  }
}


/**
 * @name void delete_parse_options_object_payload(opts_linked_list_object *opts_object)
 *
 * @description
 * This procedure will free up the memory for the parse options pointer contained in a parse-options-chained-list object (opts_object)
 * This is made in a way that it is called as the "payload_handling_procedure" procedure of a call to the delete_[all_]object[s]_in_chained_list_with_payload_handling (check these procedures above for more information)
**/

void delete_parse_options_object_payload(opts_linked_list_object *opts_object) {

  if (opts_object->payload != NULL) {
    parse_options_delete(opts_object->payload); /* Delete the parse options in the lgp API */
    opts_object->payload = NULL; /* Reset the pointer to the payload (that doesn't exist anymore!) */
  }
}


/**
 * @name pl_delete_parse_options(term_t parse_options_handle)
 * @prologname delete_parse_options/1
 *
 * @description
 * This function will destroy the parse options structure which handle is parse_options_handle from the memory
**/

foreign_t pl_delete_parse_options(term_t parse_options_handle) {

term_t       exception;
unsigned int handle_index_to_delete;

  
  if (!get_index_from_handle(FUNCTOR_options1, parse_options_handle, &handle_index_to_delete)) {
    exception=PL_new_term_ref(); /* The handle could not be changed into an integer */
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    return delete_object_in_chained_list_with_payload_handling_with_exception_handling("parse_options", NULL,
										       root_opts_list,
										       handle_index_to_delete,
										       (void (*)(generic_linked_list_object *))delete_parse_options_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the parse options payload at the same time */
  }
}


/**
 * @name pl_delete_all_parse_options()
 * @prologname delete_all_parse_options/0
 *
 * @description
 * This function will destroy all the parse options objects (both chained-list and internal to LGP)
 * Warning: avoid calling this procedure because it doesn't check for references, and is thus not safe to use except you know what you're doing!
**/

foreign_t pl_delete_all_parse_options(void) {

term_t       exception;
int          result;
unsigned int current_handle;


  current_handle = -1;
  while (1) { /* This is an infinite loop. The exit will be when get_next_handle_index_in_chained_list returns 3 */
    result = get_next_handle_index_in_chained_list(root_opts_list, current_handle, &current_handle, NULL); /* Get the object following the last one handled in the loop */
    switch (result) {
    case 0:
    case 2:
      break; /* This is an acceptable result. Carry on with this handle index */
    case 1:
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		    PL_CHARS, "parse_options",
		    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* This is the exit of this infinite loop */
      check_leak_and_raise_exceptions_macro(result, exception); /* See the declaration for this macro at the beginning of this file */
      PL_succeed;
    }

    /* If we arrive here, we have a valid handle index in current_handle. We can thus try to delete this item from the list */
    result = delete_object_in_chained_list_with_payload_handling(root_opts_list,
                                                                 current_handle,
                                                                 (void (*)(generic_linked_list_object *))delete_parse_options_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the parse options payload at the same time */
    switch (result) {
    case 2: /* This handle does not exist! */
      //@! "Breakpoint 2 handle=%d returned by get_next_handle_index_in_chained_list does not exist! Bug", current_handle //Lionel!!!
      break; /* Carry on processing the list */
    case 0: /* This object was successfully deleted */
      break;
    case 1: /* root == NULL */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "parse_options",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* current_handle == -1 */
      //@! "Breakpoint 3 handle returned=%d trying to delete the root! Bug", current_handle //Lionel!!!
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "parse_options",
                    PL_CHARS, "cant_delete");
      return PL_raise_exception(exception);
    case 4: /* Parse options is still referenced */
      //@! "Breakpoint 4 handle=%d won't delete because still referenced.", current_handle //Lionel!!!
      break;
    default:
      break; /* By default, try again for the next objects of the list */
    }
  }
}


/**
 * @name pl_get_nb_parse_options(term_t nb_parse_options)
 * @prologname get_nb_parse_options/1
 *
 * @description
 * This function returns the number of parse options currently in memory
**/

foreign_t pl_get_nb_parse_options(term_t nb_parse_options) {
  
  return PL_unify_integer(nb_parse_options, count_objects_in_chained_list(root_opts_list));
}


/**
 * @name pl_get_handles_nb_references_parse_options(term_t handle_list, term_t count_references_list)
 * @prologname pl_get_handles_nb_references_parse_options/2
 *
 * @description
 * This function returns a list containing all the existing handles of allocated parse options, and a second list containining integers of how many other objects reference the parse options which handle is at the same position in the first list
 * For example, if the pase options with handle opts_A is referenced 3 times, and the parse options with handle opts_B is never referenced, the result of this function will be:
 * ([opts_A, opts_B], [3, 0]) or ([opts_B, opts_A], [0, 3])
**/

foreign_t pl_get_handles_nb_references_parse_options(term_t handle_list, term_t count_references_list) {

unsigned int            last_handle;
term_t                  new_handle_element; /* This will contain the new handle term to insert in the list handle_list */
term_t                  new_count_references_element; /* This will contain the new count references term to insert in the list count_references_list */
term_t                  exception; /* Term used to create exceptions */
term_t                  constructed_count_references_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
term_t                  constructed_handle_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
opts_linked_list_object *parse_options; /* Reference to the current parse options being processed in the chained list */


  PL_put_nil(constructed_handle_list); /* Create the tail of the list (which is []) */
  PL_put_nil(constructed_count_references_list); /* Create the tail of the list (which is []) */

  last_handle = (unsigned int)-1; /* Start to browse the chained-list from the beginning (root) */

  while ( get_next_handle_index_in_chained_list(root_opts_list, last_handle, &last_handle, (generic_linked_list_object**)(&parse_options)) == 0 ) { /* Find next handle from the chained-list */
    //  //@! "Breakpoint 2 new last_handle=%d found in loop", last_handle //Lionel!!!
    new_handle_element = PL_new_term_ref();
    if (!unify_handle_with_index(FUNCTOR_options1, new_handle_element, last_handle)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "parse_options",
                    PL_CHARS, "cant_create_handle");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_handle_list, new_handle_element, constructed_handle_list);
    }
    new_count_references_element = PL_new_term_ref();
    if (!PL_unify_integer(new_count_references_element, parse_options->count_references)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "parse_options",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_count_references_list, new_count_references_element, constructed_count_references_list);
    }
  }
  return (PL_unify(handle_list, constructed_handle_list) && PL_unify(count_references_list, constructed_count_references_list)); /* The handle and count references lists have been successfully created, so unify with the parameters */
}


/**
 * @name pl_create_linkage_set(term_t sentence_handle, term_t parse_options_handle, term_t linkage_set_handle)
 * @prologname create_linkage_set/3
 *
 * @description
 * This function creates a new linkage set from a sentence
 * Note: This linkage set object will consist in the number of linkages found, together with the sentence used and the options.
 * It's with the linkage_set_handle returned that the user can use the non-deterministic predicate get_linkage/2
**/

foreign_t pl_create_linkage_set(term_t sentence_handle, term_t parse_options_handle, term_t linkage_set_handle) {

term_t                  exception; /* Handle for an possible exception */
unsigned int            new_handle_index; /* Variable to store the handle index referencing the new linkage set in the chained list */
link_linked_list_object *chained_new_linkage_set_object;
unsigned int            sent_handle_index; /* Handle index for the sentence used */
sent_linked_list_object *sent_object; /* Linked object (corresponding to the handle given as parameter) in the sentence chained-list */
Sentence                sent; /* Sentence object attached to the new linkage set */
unsigned int            opts_handle_index; /* Handle index for the parse options used */
opts_linked_list_object *opts_object; /* Linked object (corresponding to the handle given as parameter) in the parse options chained-list */
Parse_Options           opts; /* Parse options object attached to the new linkage set */
int                     num_linkages; /* Number of linkages computed from the sentence and parse options */



  if (!get_index_from_handle(FUNCTOR_sentence1, sentence_handle, &sent_handle_index)) { /* Transform the sentence handle into an index */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "sentence",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    if (!get_object_from_handle_index_in_chained_list_with_exception_handling("sentence", NULL,
                                                                              root_sent_list, sent_handle_index, (generic_linked_list_object **)&sent_object)) {
      PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
    }
    else { /* Function executed successfully */
      sent_object->count_references++; /* One more reference to this sentence object has been done. Update the sentence object accordingly */
      sent = sent_object->payload.sentence; /* Get a pointer on the sentence corresponding to the handle given as parameter */
    }
  }
  /* We now have a pointer to the parse_options object inside the variable opts */


  if (!get_index_from_handle(FUNCTOR_options1, parse_options_handle, &opts_handle_index)) { /* Transform the parse options handle into an index */
    sent_object->count_references--; /* The reference to this sentence object is not there anymore given that we won't create the parse options. Update the sentence object accordingly */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    if (!get_object_from_handle_index_in_chained_list_with_exception_handling("parse_options", NULL,
                                                                              root_opts_list, opts_handle_index, (generic_linked_list_object **)&opts_object)) {
      sent_object->count_references--; /* The reference to this sentence object is not there anymore given that we won't create the parse options. Update the sentence object accordingly */
      PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
    }
    else { /* Function executed successfully */
      opts_object->count_references++; /* One more reference to this parse options object has been done. Update the parse options object accordingly */
      opts = opts_object->payload; /* Get a pointer on the parse options corresponding to the handle given as parameter */
    }
  }
  /* We now have a pointer to the parse_options object inside the variable opts */


  if (sentence_length(sent) > parse_options_get_max_sentence_length(opts)) {
    exception=PL_new_term_ref();
    sent_object->count_references--; /* Remove the reference to the sentence object given that the linkage set object won't be created */
    opts_object->count_references--; /* Remove the reference to the parse options object given that the linkage set object won't be created */
// This came from the old code copied from LGP example. Commented-out for bugfix. Lionel 20020202
//    sentence_delete(sent); Can't just delete this sentence object. This needs to be done at a higher level, in order to clean the chained-list as well.
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "sentence",
		  PL_CHARS, "too_long");
    return PL_raise_exception(exception);
  }
  
  if (sentence_length(sent) > min_short_sent_len) {
    parse_options_set_short_length(opts, 6);
  }
  else {
    parse_options_set_short_length(opts, max_sentence_length);
  }

  num_linkages = sentence_parse(sent, opts);
  
  if (num_linkages==0) { /* No linkages for this sentence, this predicate will fail */
    sent_object->count_references--; /* Remove the reference to the sentence object given that the linkage set object won't be created */
    opts_object->count_references--; /* Remove the reference to the parse options object given that the linkage set object won't be created */
    PL_fail;
  }

  if (!create_object_in_chained_list_with_exception_handling("linkage_set", NULL,
                                                             root_link_list, /* Root for the linkage set chained-list. This doesn't need to be casted because roots are generic objects */
                                                             sizeof(link_linked_list_object),
                                                             NB_LINKAGE_SETS-1,
                                                             &new_handle_index,
                                                             (generic_linked_list_object **)&chained_new_linkage_set_object)) {
    sent_object->count_references--; /* Remove the reference to the sentence object given that the linkage set object won't be created */
    opts_object->count_references--; /* Remove the reference to the parse options object given that the linkage set object won't be created */
    PL_fail; /* create_object_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }

  /* When execution reaches this point, there is a new object allocated in the list (chained_new_linkage_set_object) for the new linkage set */

  /* We first make sure that we can unify the handle for the new linkage set */
  if (!unify_handle_with_index(FUNCTOR_linkageset1, linkage_set_handle, new_handle_index)) {
    delete_object_in_chained_list(root_link_list, new_handle_index);
    sent_object->count_references--; /* Remove the reference to the sentence object given that the linkage set object won't be created */
    opts_object->count_references--; /* Remove the reference to the parse options object given that the linkage set object won't be created */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "linkage_set",
		  PL_CHARS, "cant_create_handle");
    return PL_raise_exception(exception);
  }

  /* We now have to fill-in the fields of its payload structure */
  
  chained_new_linkage_set_object->payload.num_linkages = num_linkages;
  chained_new_linkage_set_object->payload.associated_sentence_chained_object = sent_object;
  chained_new_linkage_set_object->payload.associated_parse_options_chained_object = opts_object;
  chained_new_linkage_set_object->payload.associated_context_list = NULL; /* This is a new linkage set object, so no pl_get_linkage call on this linkage set has been made yet. No context has been created in pl_get_linkage, so this list is empty for now */

  PL_succeed; /* The handle has been successfully created, so succeed */
}


/**
 * @name void delete_linkage_set_object_payload(link_linked_list_object *link_object)
 *
 * @description
 * This procedure will free up the memory for the sentence pointer contained in a sentence-chained-list object (sent_object)
 * This is made in a way that it is called as the "payload_handling_procedure" procedure of a call to the delete_[all_]object[s]_in_chained_list_with_payload_handling (check these procedures above for more information)
**/

void delete_linkage_set_object_payload(link_linked_list_object *link_object) {

context_list *context_ptr, *next_context_ptr;

  link_object->payload.associated_sentence_chained_object->count_references--; /* Remove the reference to the sentence object given that the linkage set object is deleted */
  link_object->payload.associated_parse_options_chained_object->count_references--; /* Remove the reference to the parse options object given that the linkage set object is deleted */
  link_object->payload.num_linkages = 0; /* This is to make sure that the object is clean... but it will be deleted anyway! */
  
  context_ptr=link_object->payload.associated_context_list; /* Get the root of the context list */
  
  // //@! "Breakpoint 1 going to parse context_list at %p", context_ptr //Lionel!!!
  while ( context_ptr != NULL ) { /* Process the whole chained list */
    //@! "Breakpoint 2 going to setup context(%p)->link_handle_index = -1", context_ptr->context_associated_to_link //Lionel!!!
    context_ptr->context_associated_to_link->link_handle_index = -1; /* Cancel the handle_index, meaning that this context has no value anymore, because the linkage set it relates to has been deleted */
    next_context_ptr = context_ptr->next;
/* We don't free up the context object here, because it's not up to linkage set to handle this, but to pl_get_linkage, when executed (redone, or cut) */
    exfree(context_ptr, sizeof(context_list)); /* We free up context_ptr in the context chained list associate to the linkage set, because, this is, however, the linkage set's structure */
    context_ptr = next_context_ptr; /* Continue on the next item of the list, if any */
  }
/* No further cleanup is needed given that linkage set objects don't have a physical structure allocated in LGP */
}

/**
 * @name pl_delete_linkage_set(term_t linkage_set_handle)
 * @prologname delete_linkage_set/1
 *
 * @description
 * This function will remove the linkage set which handle is linkage_set_handle from the memory
**/

foreign_t pl_delete_linkage_set(term_t linkage_set_handle) {

term_t       exception;
unsigned int handle_index_to_delete;

  
  if (!get_index_from_handle(FUNCTOR_linkageset1, linkage_set_handle, &handle_index_to_delete)) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "linkage_set",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    return delete_object_in_chained_list_with_payload_handling_with_exception_handling("linkage_set", NULL,
										       root_link_list,
										       handle_index_to_delete,
										       (void (*)(generic_linked_list_object *))delete_linkage_set_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the linkage set payload at the same time */
  }
}


/**
 * @name pl_delete_all_linkage_sets()
 * @prologname delete_all_linkage_sets/0
 *
 * @description
 * This function will erase all the linkage sets
**/

foreign_t pl_delete_all_linkage_sets(void) {

term_t       exception;
int          result;
unsigned int current_handle;


  current_handle = -1;
  while (1) { /* This is an infinite loop. The exit will be when get_next_handle_index_in_chained_list returns 3 */
    result = get_next_handle_index_in_chained_list(root_link_list, current_handle, &current_handle, NULL); /* Get the object following the last one handled in the loop */
    switch (result) {
    case 0:
    case 2:
      break; /* This is an acceptable result. Carry on with this handle index */
    case 1:
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		    PL_CHARS, "linkage_set",
		    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* This is the exit of this infinite loop */
// Commented because not needed for linkage set objects, given that they don't use any memory space in LGP
//      check_leak_and_raise_exceptions_macro(result, exception); /* See the declaration for this macro at the beginning of this file */
      PL_succeed;
    }

    /* If we arrive here, we have a valid handle index in current_handle. We can thus try to delete this item from the list */
    result = delete_object_in_chained_list_with_payload_handling(root_link_list,
                                                                 current_handle,
                                                                 (void (*)(generic_linked_list_object *))delete_linkage_set_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the linkage set object at the same time */
    switch (result) {
    case 2: /* This handle does not exist! */
      //@! "Breakpoint 2 handle=%d returned by get_next_handle_index_in_chained_list does not exist! Bug", current_handle //Lionel!!!
      break; /* Carry on processing the list */
    case 0: /* This object was successfully deleted */
      break;
    case 1: /* root == NULL */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "linkage_set",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* current_handle == -1 */
      //@! "Breakpoint 3 handle returned=%d trying to delete the root! Bug", current_handle //Lionel!!!
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "linkage_set",
                    PL_CHARS, "cant_delete");
      return PL_raise_exception(exception);
    case 4: /* This linkage set is still referenced (should never happen, as nothing is using the linkage set */
      //@! "Breakpoint 4 handle=%d won't delete because still referenced. Linkage set should not be referenced! Bug", current_handle //Lionel!!!
      break;
    default:
      break; /* By default, try again for the next objects of the list */
    }
  }
}


/**
 * @name pl_get_nb_linkage_sets(term_t nb_linkage_sets)
 * @prologname get_nb_linkage_sets/1
 *
 * @description
 * This function returns the number of linkage sets currently in memory
**/

foreign_t pl_get_nb_linkage_sets(term_t nb_linkage_sets) {
  
  return PL_unify_integer(nb_linkage_sets, count_objects_in_chained_list(root_link_list));
}


/**
 * @name pl_get_handles_nb_references_linkage_sets(term_t handle_list, term_t count_references_list)
 * @prologname pl_get_handles_nb_references_linkage_sets/2
 *
 * @description
 * This function returns a list containing all the existing handles of allocated linkage sets, and a second list containining integers of how many other objects reference the linkage set which handle is at the same position in the first list
 * For example, if the linkage set with handle link_A is referenced 3 times, and the linkage set with handle link_B is never referenced, the result of this function will be:
 * ([link_A, link_B], [3, 0]) or ([link_B, link_A], [0, 3])
**/

foreign_t pl_get_handles_nb_references_linkage_sets(term_t handle_list, term_t count_references_list) {

unsigned int            last_handle;
term_t                  new_handle_element; /* This will contain the new handle term to insert in the list handle_list */
term_t                  new_count_references_element; /* This will contain the new count references term to insert in the list count_references_list */
term_t                  exception; /* Term used to create exceptions */
term_t                  constructed_count_references_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
term_t                  constructed_handle_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
link_linked_list_object *linkage_set; /* Reference to the current linkage set being processed in the chained list */


  PL_put_nil(constructed_handle_list); /* Create the tail of the list (which is []) */
  PL_put_nil(constructed_count_references_list); /* Create the tail of the list (which is []) */

  last_handle = (unsigned int)-1; /* Start to browse the chained-list from the beginning (root) */

  while ( get_next_handle_index_in_chained_list(root_link_list, last_handle, &last_handle, (generic_linked_list_object**)(&linkage_set)) == 0 ) { /* Find next handle from the chained-list */
    //  //@! "Breakpoint 2 new last_handle=%d found in loop", last_handle //Lionel!!!
    new_handle_element = PL_new_term_ref();
    if (!unify_handle_with_index(FUNCTOR_linkageset1, new_handle_element, last_handle)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "linkage_set",
                    PL_CHARS, "cant_create_handle");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_handle_list, new_handle_element, constructed_handle_list);
    }
    new_count_references_element = PL_new_term_ref();
    if (!PL_unify_integer(new_count_references_element, linkage_set->count_references)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "linkage_set",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_count_references_list, new_count_references_element, constructed_count_references_list);
    }
  }
  return (PL_unify(handle_list, constructed_handle_list) && PL_unify(count_references_list, constructed_count_references_list)); /* The handle and count references lists have been successfully created, so unify with the parameters */
}


/**
 * @name pl_get_num_linkages(term_t linkage_set_handle, term_t t_num_linkages)
 * @prologname get_num_linkages/2
 *
 * @description
 * This function returns the value of num_linkages for a linkage_set object
**/

foreign_t pl_get_num_linkages(term_t linkage_set_handle, term_t t_num_linkages) {

term_t                  exception;
unsigned int            handle_index;
link_linked_list_object *link_object; /* Pointer to the linkage set object in the chained list */


  if (!get_index_from_handle(FUNCTOR_linkageset1, linkage_set_handle, &handle_index)) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "linkage_set",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    if (!get_object_from_handle_index_in_chained_list_with_exception_handling("linkage_set", NULL, root_link_list, handle_index, (generic_linked_list_object **)&link_object)) {
      PL_fail; /* Return the exception that has been prepared by get_object_from_handle_index_in_chained_list_with_exception_handling */
    }
  }

/* When we arrive here, we havee in *link_object the linkage set chained-object given in parameter */
  return PL_unify_integer(t_num_linkages, link_object->payload.num_linkages);
}


/**
 * @name pl_get_parameters_for_linkage_set(term_t linkage_set_handle, term_t parameter_list)
 * @prologname get_parameters_for_linkage_set/2
 *
 * @description
 * This function returns a list containing all the handles and values associated to a linkage set object
**/

foreign_t pl_get_parameters_for_linkage_set(term_t linkage_set_handle, term_t parameter_list) {

term_t                  constructed_list;
term_t                  num_linkage_term = PL_new_term_ref();
term_t                  num_linkage_value = PL_new_term_ref();
term_t                  sentence_handle = PL_new_term_ref();
term_t                  sentence_term = PL_new_term_ref();
term_t                  parse_options_handle = PL_new_term_ref();
term_t                  parse_options_term = PL_new_term_ref();
term_t                  exception;
unsigned int            link_handle_index; /* Index for the linkage set we are having a look at. This is extracted from the parameter linkage_set_handle */
int                     result;
unsigned int            sent_handle_index;
unsigned int            opts_handle_index;
link_linked_list_object *link_object; /* Pointer to the linkage set object in the chained list */



  if (!get_index_from_handle(FUNCTOR_linkageset1, linkage_set_handle, &link_handle_index)) { /* Get the handle index for the requested linkage set objects */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "linkage_set",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }


/* The following will also test whether this handle points to an empty cell or not. If the cell is empty, an exception will be raised and we won't intercept it here, so we will also exit from pl_get_parameters_for_linkage_set */
/* Therefore, we don't have to worry about the handle. If we pass the function below, we know that the linkage set indicated by linkage_set_handle exists (not empty) */
  if (!pl_get_num_linkages(linkage_set_handle, num_linkage_value)) { /* Call pl_get_num_linkages to get the number of linkages inside this linkage set */    

    /* If pl_get_num_linkages fails, it means that an exception has been built and is ready to be thrown when the execution comes back to Prolog. We just fail to raise the exception */
    PL_fail;
  }

/* The following creates a term with the template "num_linkage=xxx" where xxx is the number of linkages for this linkage set handle */
/* (a call to pl_get_num_linkages is done to get this value) */
/* The new term is stored inside num_linkage_term */
  PL_unify_term(num_linkage_term,
                PL_FUNCTOR, FUNCTOR_equals2,
                PL_CHARS, "num_linkage",
                PL_TERM, num_linkage_value);


/* The following part of the code is a simple copy/paste of the top of the code in pl_get_num_linkages/2. It will extract the linkage set chained object matching with the handle given as parameter */
  if (!get_object_from_handle_index_in_chained_list_with_exception_handling("linkage_set", NULL, root_link_list, link_handle_index, (generic_linked_list_object **)&link_object)) {
    PL_fail; /* Return the exception prepared by get_object_from_handle_index_in_chained_list_with_exception_handling */
  }

/* When we arrive here, we have in *link_object the linkage set chained-object given in parameter */

/* The following will get the handle index of the sentence referenced by this linkage set */
/* To do so, we use the function get_handle_index_from_object_in_chained_list_with_exception_handling() that will scan the sentence chained-list to find the first object that corresponds to the reference stored inside the structure in the linkage set object */
  if (!get_handle_index_from_object_in_chained_list_with_exception_handling("sentence", &result,
                                                                            (generic_linked_list_object *)root_sent_list,
                                                                            &sent_handle_index,
                                                                            (generic_linked_list_object *)(link_object->payload.associated_sentence_chained_object)
                                                                           )) {
    if (result == 2) { /* If error returned is 2. Override the standard exception with the following one */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "sentence",
                    PL_CHARS, "reference_erased");
    }
    PL_fail; /* get_handle_index_from_object_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }
  if (!unify_handle_with_index(FUNCTOR_sentence1, sentence_handle, sent_handle_index)) { /* Create a term containing the handle for the sentence associated with our linkage set object */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "sentence",
		  PL_CHARS, "cant_create_handle");
    return PL_raise_exception(exception);
  }
/* The following creates a term with the template "sentence_handle=xxx" where xxx is a handle for the sentence object associated with our linkage set handle */
/* The new term is stored inside sentence_term freshly created */
  PL_unify_term(sentence_term,
                PL_FUNCTOR, FUNCTOR_equals2,
                PL_CHARS, "sentence_handle",
                PL_TERM, sentence_handle);



/* The following will get the handle index of the parse options referenced by this linkage set */
/* To do so, we use the function get_handle_index_from_object_in_chained_list_with_exception_handling that will scan the parse options chained-list to find the first object that corresponds to the reference stored inside the structure in the linkage set object */
  if (!get_handle_index_from_object_in_chained_list_with_exception_handling("parse_options", &result,
                                                                            (generic_linked_list_object *)root_opts_list,
                                                                            &opts_handle_index,
                                                                            (generic_linked_list_object *)(link_object->payload.associated_parse_options_chained_object)
                                                                           )) {
    if (result == 2) { /* If error returned is 2. Override the standard exception with the following one */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "parse_options",
                    PL_CHARS, "reference_erased");
    }
    PL_fail; /* handle_index_from_object_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }
  if (!unify_handle_with_index(FUNCTOR_options1, parse_options_handle, opts_handle_index)) { /* Create a term containing the handle for the parse options associated with our linkage set object */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "cant_create_handle");
    return PL_raise_exception(exception);
  }
/* The following creates a term with the template "parse_options_handle=xxx" where xxx is a handle for the parse options object associated with our linkage set handle */
/* The new term is stored inside parse_options_term freshly created */
  PL_unify_term(parse_options_term,
                PL_FUNCTOR, FUNCTOR_equals2,
                PL_CHARS, "parse_options_handle",
                PL_TERM, parse_options_handle);
  

  constructed_list = PL_new_term_ref(); /* Term used to store the list while constructing it */

  PL_put_nil(constructed_list); /* Create the tail of the list (which is []) */

  PL_cons_list(constructed_list, parse_options_term, constructed_list);
  PL_cons_list(constructed_list, sentence_term, constructed_list);
  PL_cons_list(constructed_list, num_linkage_term, constructed_list);

  return PL_unify(parameter_list, constructed_list); /* The list has been successfully created, so unify with the parameter */
}


/**
 * @name pl_create_sentence(term_t t_input_sentence, term_t dictionary_handle, term_t sentence_handle)
 *
 * @description
 * This function creates a new sentence using the text contained by the t_input_sentence term. The handle for the newly created sentence is bound with sentence_handle
**/

foreign_t pl_create_sentence(term_t t_input_sentence, term_t dictionary_handle, term_t sentence_handle) {

term_t                   exception; /* Handle for an possible exception */
unsigned int            new_handle_index; /* Variable to store the handle index referencing the new sentence object in the chained list */
Sentence                new_sentence; /* The new sentence object created (internal LGP object) */
sent_linked_list_object *chained_new_sentence_object;
unsigned int            dict_handle_index; /* Handle index for the dictionary used */
Dictionary              dict; /* Dictionary object */
dict_linked_list_object *dict_object; /* Linked object corresponding to the handle, in the dictionary chained-list */
char                    *input_sentence; /* Input sentence given as parameter */


  if (!PL_get_atom_chars(t_input_sentence, &input_sentence)) {
    exception=PL_new_term_ref(); /* Create a new exception object */
    PL_unify_term(exception,
		  PL_CHARS, "instanciation_fault"); /* Fill-in the exception object */
    return PL_raise_exception(exception); /* Raise the exception and exit */
  }


  if (!get_index_from_handle(FUNCTOR_dictionary1, dictionary_handle, &dict_handle_index)) {
    exception = PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "dictionary",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    if (!get_object_from_handle_index_in_chained_list_with_exception_handling("dictionary", NULL,
                                                                              root_dict_list, dict_handle_index, (generic_linked_list_object **)&dict_object)) {
      PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
    }
    else { /* Function executed successfully */
      dict_object->count_references++; /* One more reference to this dictionary object has been done. Update the dictionary object accordingly */
      /* When cout_references has been incremented, all paths should take care of the validity of this field. This means that any failure in creating the sentence object should also decrement the count */
      dict = dict_object->payload; /* Get a pointer on the dictionary corresponding to the handle given as parameter */
    }
  }
  /* We now have a pointer to the dictionary object inside the variable dict */

  new_sentence = sentence_create(input_sentence, dict); /* Create the sentence object */
  /* The above line will create the sentence, using the string given through the t_input_sentence term and the dictionary which handle matches with dictionary_handle */

  if (new_sentence == NULL) { /* Check if the sentence has been successfully created. If not, raise a Prolog exception */
    dict_object->count_references--; /* Remove the reference to the dictionary object given that the sentence object won't be created */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "sentence",
		  PL_CHARS, "cant_register");
    return PL_raise_exception(exception);
  }

  if (!create_object_in_chained_list_with_exception_handling("sentence", NULL,
                                                             root_sent_list, /* Root for the sentence chained-list. This doesn't need to be casted because roots are generic objects */
                                                             sizeof(sent_linked_list_object),
                                                             NB_SENTENCES-1,
                                                             &new_handle_index,
                                                             (generic_linked_list_object **)&chained_new_sentence_object)) {
    dict_object->count_references--; /* Remove the reference to the dictionary object given that the sentence object won't be created */
    PL_fail; /* create_object_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }
  
  /* Insert the reference to the Sentence object in the payload of the new chained object */
  chained_new_sentence_object->payload.sentence = new_sentence;

  /* Record the dictionary used for this sentence inside the new chained object as well */
  chained_new_sentence_object->payload.associated_dictionary_chained_object = dict_object;
  /* Note: the field count_references in the dictionary object has already been incremented when the handle for the dictionary was converted to an integer (see above) */
  /* All execution path that lead to a failure (exception) thus need to decrement this count_references again, and free up the sentence object when necessary (see below and above) */
  /* The count_references value is here already up-to-date (counting the fact that the new sentence object is using the dictionary specified). We don't have to increment it here */

  /* When execution reaches this point, the new Sentence object has been created in LGP */
  /* There is an object allocated in the list (chained_new_sentence_object) for the new sentence, which points to the relevant Sentence object using the ->payload field */
  /* We now have to send back a handle to this sentence */

  if (!unify_handle_with_index(FUNCTOR_sentence1, sentence_handle, new_handle_index)) {
    dict_object->count_references--; /* Remove the reference to the dictionary object given that the sentence object won't be created */
    sentence_delete(new_sentence);
    delete_object_in_chained_list(root_sent_list, new_handle_index); /* Remove the sentence from the chained list because this sentence object has been erased. We don't grab the return value from this function because the exception we will raise will anyway be related to the handle we can't create */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "sentence",
		  PL_CHARS, "cant_create_handle");
    return PL_raise_exception(exception);
  }
  else {
    PL_succeed; /* The handle has been successfully created, so succeed */
  }
}


/**
 * @name void delete_sentence_object_payload(sent_linked_list_object *sent_object)
 *
 * @description
 * This procedure will free up the memory for the sentence pointer contained in a sentence-chained-list object (sent_object)
 * This is made in a way that it is called as the "payload_handling_procedure" procedure of a call to the delete_[all_]object[s]_in_chained_list_with_payload_handling (check these procedures above for more information)
**/

void delete_sentence_object_payload(sent_linked_list_object *sent_object) {

  sent_object->payload.associated_dictionary_chained_object->count_references--; /* Remove the reference to the dictionary object given that the sentence object is deleted */
  if ( sent_object->payload.sentence != NULL) {
    sentence_delete(sent_object->payload.sentence); /* Delete the sentence in the lgp API */
    sent_object->payload.sentence = NULL; /* Reset the pointer to the payload (that doesn't exist anymore!) */
  }
}


/**
 * @name pl_delete_sentence(term_t sentence_handle)
 * @prologname delete_sentence/1
 *
 * @description
 * This function will destroy the sentence which handle is sentence_handle from the memory
**/

foreign_t pl_delete_sentence(term_t sentence_handle) {

term_t       exception;
unsigned int handle_index_to_delete;

  
  if (!get_index_from_handle(FUNCTOR_sentence1, sentence_handle, &handle_index_to_delete)) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "sentence",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }
  else {
    return delete_object_in_chained_list_with_payload_handling_with_exception_handling("sentence", NULL,
										       root_sent_list,
										       handle_index_to_delete,
										       (void (*)(generic_linked_list_object *))delete_sentence_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the sentence payload at the same time */
  }
}


/**
 * @name pl_delete_all_sentences()
 * @prologname delete_all_sentences/0
 *
 * @description
 * This function will erase all the sentences
**/

foreign_t pl_delete_all_sentences(void) {

term_t       exception;
int          result;
unsigned int current_handle;


  current_handle = -1;
  while (1) { /* This is an infinite loop. The exit will be when get_next_handle_index_in_chained_list returns 3 */
    result = get_next_handle_index_in_chained_list(root_sent_list, current_handle, &current_handle, NULL); /* Get the object following the last one handled in the loop */
    switch (result) {
    case 0:
    case 2:
      break; /* This is an acceptable result. Carry on with this handle index */
    case 1:
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		    PL_CHARS, "sentence",
		    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* This is the exit of this infinite loop */
      check_leak_and_raise_exceptions_macro(result, exception); /* See the declaration for this macro at the beginning of this file */
      PL_succeed;
    }

    /* If we arrive here, we have a valid handle index in current_handle. We can thus try to delete this item from the list */
    result = delete_object_in_chained_list_with_payload_handling(root_sent_list,
                                                                 current_handle,
                                                                 (void (*)(generic_linked_list_object *))delete_sentence_object_payload); /* Delete the object in the chained list (its index will match the handle_index_to_delete requested) and free up the sentence payload at the same time */
    switch (result) {
    case 2: /* This handle does not exist! */
      //@! "Breakpoint 2 handle=%d returned by get_next_handle_index_in_chained_list does not exist! Bug", current_handle //Lionel!!!
      break; /* Carry on processing the list */
    case 0: /* This object was successfully deleted */
      break;
    case 1: /* root == NULL */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "sentence",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    case 3: /* current_handle == -1 */
      //@! "Breakpoint 3 handle returned=%d trying to delete the root! Bug", current_handle //Lionel!!!
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "sentence",
                    PL_CHARS, "cant_delete");
      return PL_raise_exception(exception);
    case 4: /* Sentence is still referenced */
      // //@! "Breakpoint 4 handle=%d won't delete because still referenced.", current_handle //Lionel!!!
      break;
    default:
      break; /* By default, try again for the next objects of the list */
    }
  }
}


/**
 * @name pl_get_nb_sentences(term_t nb_sentences)
 * @prologname get_nb_sentences/1
 *
 * @description
 * This function returns the number of sentences currently in memory
**/

foreign_t pl_get_nb_sentences(term_t nb_sentences) {

  return PL_unify_integer(nb_sentences, count_objects_in_chained_list(root_sent_list));
}


/**
 * @name pl_get_handles_nb_references_sentences(term_t handle_list, term_t count_references_list)
 * @prologname pl_get_handles_nb_references_sentences/2
 *
 * @description
 * This function returns a list containing all the existing handles of allocated sentences, and a second list containining integers of how many other objects reference the sentence which handle is at the same position in the first list
 * For example, if the sentence with handle sent_A is referenced 3 times, and the sentence with handle sent_B is never referenced, the result of this function will be:
 * ([sent_A, sent_B], [3, 0]) or ([sent_B, sent_A], [0, 3])
**/

foreign_t pl_get_handles_nb_references_sentences(term_t handle_list, term_t count_references_list) {

unsigned int            last_handle;
term_t                  new_handle_element; /* This will contain the new handle term to insert in the list handle_list */
term_t                  new_count_references_element; /* This will contain the new count references term to insert in the list count_references_list */
term_t                  exception; /* Term used to create exceptions */
term_t                  constructed_count_references_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
term_t                  constructed_handle_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
sent_linked_list_object *sentence; /* Reference to the current sentence being processed in the chained list */


  PL_put_nil(constructed_handle_list); /* Create the tail of the list (which is []) */
  PL_put_nil(constructed_count_references_list); /* Create the tail of the list (which is []) */

  last_handle = (unsigned int)-1; /* Start to browse the chained-list from the beginning (root) */

  while ( get_next_handle_index_in_chained_list(root_sent_list, last_handle, &last_handle, (generic_linked_list_object**)(&sentence)) == 0 ) { /* Find next handle from the chained-list */
//  //@! "Breakpoint 2 new last_handle=%d found in loop", last_handle //Lionel!!!
    new_handle_element = PL_new_term_ref();
    if (!unify_handle_with_index(FUNCTOR_sentence1, new_handle_element, last_handle)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "sentence",
                    PL_CHARS, "cant_create_handle");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_handle_list, new_handle_element, constructed_handle_list);
    }
    new_count_references_element = PL_new_term_ref();
    if (!PL_unify_integer(new_count_references_element, sentence->count_references)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "sentence",
                    PL_CHARS, "list_corrupted");
      return PL_raise_exception(exception);
    }
    else {
      PL_cons_list(constructed_count_references_list, new_count_references_element, constructed_count_references_list);
    }
  }
  return (PL_unify(handle_list, constructed_handle_list) && PL_unify(count_references_list, constructed_count_references_list)); /* The handle and count references lists have been successfully created, so unify with the parameters */
}


/**
 * @name po_set_options_integer_(term_t parse_options_handle, term_t integer_term, void (*function_to_call)(Parse_Options, int))
 *
 * @description
 * This predicate sets a property to the integer_term value inside the parse options object associated with the parse_options_handle handle.
 * Note: this function will use function_to_call to know which function to use to set the property (the template must be function_to_call(Parse_Options, int))
**/

static int po_set_options_integer_(term_t parse_options_handle, term_t integer_term, void (*function_to_call)(Parse_Options, int)) {

term_t                  exception;
int                     integer;
unsigned int            opts_handle_index;
opts_linked_list_object *opts_object; /* Linked object corresponding to the handle, in the parse options chained-list */

  
  if (function_to_call==NULL)
    PL_fail;

  if (!PL_get_integer(integer_term, &integer))
    PL_fail;

  if (!get_index_from_handle(FUNCTOR_options1, parse_options_handle, &opts_handle_index)) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }


  if (!get_object_from_handle_index_in_chained_list_with_exception_handling("parse_options", NULL,
                                                                            root_opts_list, opts_handle_index, (generic_linked_list_object **)&opts_object)) {
    PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }
  else { /* Function executed successfully */
    function_to_call(opts_object->payload, integer);
    PL_succeed;
  }
}


/**
 * @name po_get_options_integer_(term_t parse_options_handle, term_t integer_term, int (*function_to_call)(Parse_Options))
 *
 * @description
 * This predicate gets a property from a parse options structure to the integer_term value.
 * Note: this function will use function_to_call to know which function to use to get the property (the template must be int function_to_call(Parse_Options))
**/

static int po_get_options_integer_(term_t parse_options_handle, term_t integer_term, int (*function_to_call)(Parse_Options)) {
  
term_t                  exception;
int                     integer;
unsigned int            opts_handle_index;
opts_linked_list_object *opts_object; /* Linked object corresponding to the handle, in the parse options chained-list */

  
  if (function_to_call==NULL)
    PL_fail;

  if (!get_index_from_handle(FUNCTOR_options1, parse_options_handle, &opts_handle_index)) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "empty_index");
    return PL_raise_exception(exception);
  }

  if (!get_object_from_handle_index_in_chained_list_with_exception_handling("parse_options", NULL,
                                                                            root_opts_list, opts_handle_index, (generic_linked_list_object **)&opts_object)) {
    PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }
  else { /* Function executed successfully */
    integer = function_to_call(opts_object->payload);
    return PL_unify_integer(integer_term, integer);
  }
}

/**
 * @name po_set_options_boolean_(term_t parse_options_handle, term_t boolean_term, void (*function_to_call)(Parse_Options, int)) {
 *
 * @description
 * This predicate sets a property to the boolean_term value inside the parse options object associated with the parse_options_handle handle.
 * Note: this function will use function_to_call to know which function to use to set the property (the template must be function_to_call(Parse_Options, int))
**/

static int po_set_options_boolean_(term_t parse_options_handle, term_t boolean_term, void (*function_to_call)(Parse_Options, int)) {
  
term_t                  exception;
int                     boolean;
unsigned int            opts_handle_index;
opts_linked_list_object *opts_object; /* Linked object corresponding to the handle, in the parse options chained-list */

  
  if (function_to_call==NULL)
    PL_fail;

  if (!PL_unify_atom_chars(boolean_term, "true")) {
    boolean = TRUE;
  }
  else if (!PL_unify_atom_chars(boolean_term, "false")) {
    boolean = FALSE;
  }
  else
    PL_fail; /* Not true nor false. Fail to notify an error about the value */

  if (!get_index_from_handle(FUNCTOR_options1, parse_options_handle, &opts_handle_index)) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "bad_handle");
    return PL_raise_exception(exception);
  }


  if (!get_object_from_handle_index_in_chained_list_with_exception_handling("parse_options", NULL,
                                                                            root_opts_list, opts_handle_index, (generic_linked_list_object **)&opts_object)) {
    PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }
  else { /* Function executed successfully */
    function_to_call(opts_object->payload, boolean);
    PL_succeed;
  }
}

/**
 * @name po_get_options_boolean_(term_t parse_options_handle, term_t boolean_term, int (*function_to_call)(Parse_Options)) {
 *
 * @description
 * This predicate gets a property from a parse options structure to the boolean_term value.
 * Note: this function will use function_to_call to know which function to use to get the property (the template must be int function_to_call(Parse_Options))
**/

static int po_get_options_boolean_(term_t parse_options_handle, term_t boolean_term, int (*function_to_call)(Parse_Options)) {
  
term_t                  exception;
int                     boolean;
unsigned int            opts_handle_index;
opts_linked_list_object *opts_object; /* Linked object corresponding to the handle, in the parse options chained-list */

  
  if (function_to_call==NULL)
    PL_fail;

  if (!get_index_from_handle(FUNCTOR_options1, parse_options_handle, &opts_handle_index)) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "empty_index");
    return PL_raise_exception(exception);
  }

  if (!get_object_from_handle_index_in_chained_list_with_exception_handling("parse_options", NULL,
                                                                            root_opts_list, opts_handle_index, (generic_linked_list_object **)&opts_object)) {
    PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
  }
  else { /* Function executed successfully */
    switch (boolean = function_to_call(opts_object->payload)) {
    case TRUE:
      PL_unify_atom_chars(boolean_term, "true");
      PL_succeed;
      break;
    case FALSE:
      PL_unify_atom_chars(boolean_term, "false");
      PL_succeed;
      break;
    default:
      PL_fail; /* Not true nor false. Fail */
    }
  }
}


/**
 * @name pl_po_set_linkage_limit(term_t parse_options_handle, term_t linkage_limit_term)
 * @prologname po_set_linkage_limit_/2
 *
 * @description
 * This predicate sets the linkage_limit property (from the linkage_limit_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_linkage_limit(term_t parse_options_handle, term_t linkage_limit_term) {
  return po_set_options_integer_(parse_options_handle, linkage_limit_term, parse_options_set_linkage_limit);
}


/**
 * @name pl_po_get_linkage_limit(term_t parse_options_handle, term_t linkage_limit_term)
 * @prologname po_get_linkage_limit_/2
 *
 * @description
 * This predicate gets the linkage_limit property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_linkage_limit(term_t parse_options_handle, term_t linkage_limit_term) {
  return po_get_options_integer_(parse_options_handle, linkage_limit_term, parse_options_get_linkage_limit);
}


/**
 * @name pl_po_set_disjunct_cost(term_t parse_options_handle, term_t disjunct_cost_term)
 * @prologname po_set_disjunct_cost_/2
 *
 * @description
 * This predicate sets the disjunct_cost property (from the disjunct_cost_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_disjunct_cost(term_t parse_options_handle, term_t disjunct_cost_term) {
  return po_set_options_integer_(parse_options_handle, disjunct_cost_term, parse_options_set_disjunct_cost);
}


/**
 * @name pl_po_get_disjunct_cost(term_t parse_options_handle, term_t disjunct_cost_term)
 * @prologname po_get_disjunct_cost_/2
 *
 * @description
 * This predicate gets the disjunct_cost property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_disjunct_cost(term_t parse_options_handle, term_t disjunct_cost_term) {
  return po_get_options_integer_(parse_options_handle, disjunct_cost_term, parse_options_get_disjunct_cost);
}


/**
 * @name pl_po_set_min_null_count(term_t parse_options_handle, term_t min_null_count_term)
 * @prologname po_set_min_null_count_/2
 *
 * @description
 * This predicate sets the min_null_count property (from the min_null_count_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_min_null_count(term_t parse_options_handle, term_t min_null_count_term) {
  return po_set_options_integer_(parse_options_handle, min_null_count_term, parse_options_set_min_null_count);
}


/**
 * @name pl_po_get_min_null_count(term_t parse_options_handle, term_t min_null_count_term)
 * @prologname po_get_min_null_count_/2
 *
 * @description
 * This predicate gets the min_null_count property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_min_null_count(term_t parse_options_handle, term_t min_null_count_term) {
  return po_get_options_integer_(parse_options_handle, min_null_count_term, parse_options_get_min_null_count);
}


/**
 * @name pl_po_set_max_null_count(term_t parse_options_handle, term_t max_null_count_term)
 * @prologname po_set_max_null_count_/2
 *
 * @description
 * This predicate sets the max_null_count property (from the max_null_count_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_max_null_count(term_t parse_options_handle, term_t max_null_count_term) {
  return po_set_options_integer_(parse_options_handle, max_null_count_term, parse_options_set_max_null_count);
}


/**
 * @name pl_po_get_max_null_count(term_t parse_options_handle, term_t max_null_count_term)
 * @prologname po_get_max_null_count_/2
 *
 * @description
 * This predicate gets the max_null_count property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_max_null_count(term_t parse_options_handle, term_t max_null_count_term) {
  return po_get_options_integer_(parse_options_handle, max_null_count_term, parse_options_get_max_null_count);
}


/**
 * @name pl_po_set_null_block(term_t parse_options_handle, term_t null_block_term)
 * @prologname po_set_null_block_/2
 *
 * @description
 * This predicate sets the null_block property (from the null_block_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_null_block(term_t parse_options_handle, term_t null_block_term) {
  return po_set_options_integer_(parse_options_handle, null_block_term, parse_options_set_null_block);
}


/**
 * @name pl_po_get_null_block(term_t parse_options_handle, term_t null_block_term)
 * @prologname po_get_null_block_/2
 *
 * @description
 * This predicate gets the null_block property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_null_block(term_t parse_options_handle, term_t null_block_term) {
  return po_get_options_integer_(parse_options_handle, null_block_term, parse_options_get_null_block);
}


/**
 * @name pl_po_set_islands_ok(term_t parse_options_handle, term_t islands_ok_term)
 * @prologname po_set_islands_ok_/2
 *
 * @description
 * This predicate sets the islands_ok property (from the islands_ok_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_islands_ok(term_t parse_options_handle, term_t islands_ok_term) {
  return po_set_options_boolean_(parse_options_handle, islands_ok_term, parse_options_set_islands_ok);
}


/**
 * @name pl_po_get_islands_ok(term_t parse_options_handle, term_t islands_ok_term)
 * @prologname po_get_islands_ok_/2
 *
 * @description
 * This predicate gets the islands_ok property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_islands_ok(term_t parse_options_handle, term_t islands_ok_term) {
  return po_get_options_boolean_(parse_options_handle, islands_ok_term, parse_options_get_islands_ok);
}


/**
 * @name pl_po_set_short_length(term_t parse_options_handle, term_t short_length_term)
 * @prologname po_set_short_length_/2
 *
 * @description
 * This predicate sets the short_length property (from the short_length_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_short_length(term_t parse_options_handle, term_t short_length_term) {
  return po_set_options_integer_(parse_options_handle, short_length_term, parse_options_set_short_length);
}


/**
 * @name pl_po_get_short_length(term_t parse_options_handle, term_t short_length_term)
 * @prologname po_get_short_length_/2
 *
 * @description
 * This predicate gets the short_length property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_short_length(term_t parse_options_handle, term_t short_length_term) {
  return po_get_options_integer_(parse_options_handle, short_length_term, parse_options_get_short_length);
}


/**
 * @name pl_po_set_all_short_connectors(term_t parse_options_handle, term_t all_short_connectors_term)
 * @prologname po_set_all_short_connectors_/2
 *
 * @description
 * This predicate sets the all_short_connectors property (from the all_short_connectors_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_all_short_connectors(term_t parse_options_handle, term_t all_short_connectors_term) {
  return po_set_options_boolean_(parse_options_handle, all_short_connectors_term, parse_options_set_all_short_connectors);
}


/**
 * @name pl_po_get_all_short_connectors(term_t parse_options_handle, term_t all_short_connectors_term)
 * @prologname po_get_all_short_connectors_/2
 *
 * @description
 * This predicate gets the all_short_connectors property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_all_short_connectors(term_t parse_options_handle, term_t all_short_connectors_term) {
  return po_get_options_boolean_(parse_options_handle, all_short_connectors_term, parse_options_get_all_short_connectors);
}


/**
 * @name pl_po_set_max_parse_time(term_t parse_options_handle, term_t max_parse_time_term)
 * @prologname po_set_max_parse_time_/2
 *
 * @description
 * This predicate sets the max_parse_time property (from the max_parse_time_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_max_parse_time(term_t parse_options_handle, term_t max_parse_time_term) {
  return po_set_options_integer_(parse_options_handle, max_parse_time_term, parse_options_set_max_parse_time);
}


/**
 * @name pl_po_get_max_parse_time(term_t parse_options_handle, term_t max_parse_time_term)
 * @prologname po_get_max_parse_time_/2
 *
 * @description
 * This predicate gets the max_parse_time property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_max_parse_time(term_t parse_options_handle, term_t max_parse_time_term) {
  return po_get_options_integer_(parse_options_handle, max_parse_time_term, parse_options_get_max_parse_time);
}


/**
 * @name pl_po_set_max_memory(term_t parse_options_handle, term_t max_memory_term)
 * @prologname po_set_max_memory_/2
 *
 * @description
 * This predicate sets the max_memory property (from the max_memory_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_max_memory(term_t parse_options_handle, term_t max_memory_term) {
  return po_set_options_integer_(parse_options_handle, max_memory_term, parse_options_set_max_memory);
}


/**
 * @name pl_po_get_max_memory(term_t parse_options_handle, term_t max_memory_term)
 * @prologname po_get_max_memory_/2
 *
 * @description
 * This predicate gets the max_memory property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_max_memory(term_t parse_options_handle, term_t max_memory_term) {
  return po_get_options_integer_(parse_options_handle, max_memory_term, parse_options_get_max_memory);
}


/**
 * @name pl_po_set_max_sentence_length(term_t parse_options_handle, term_t max_sentence_length_term)
 * @prologname po_set_max_sentence_length_/2
 *
 * @description
 * This predicate sets the max_sentence_length property (from the max_sentence_length_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_max_sentence_length(term_t parse_options_handle, term_t max_sentence_length_term) {
  return po_set_options_integer_(parse_options_handle, max_sentence_length_term, parse_options_set_max_sentence_length);
}


/**
 * @name pl_po_get_max_sentence_length(term_t parse_options_handle, term_t max_sentence_length_term)
 * @prologname po_get_max_sentence_length_/2
 *
 * @description
 * This predicate gets the max_sentence_length property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_max_sentence_length(term_t parse_options_handle, term_t max_sentence_length_term) {
  return po_get_options_integer_(parse_options_handle, max_sentence_length_term, parse_options_get_max_sentence_length);
}


/**
 * @name pl_po_set_batch_mode(term_t parse_options_handle, term_t batch_mode_term)
 * @prologname po_set_batch_mode_/2
 *
 * @description
 * This predicate sets the batch_mode property (from the batch_mode_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_batch_mode(term_t parse_options_handle, term_t batch_mode_term) {
  return po_set_options_boolean_(parse_options_handle, batch_mode_term, parse_options_set_batch_mode);
}


/**
 * @name pl_po_get_batch_mode(term_t parse_options_handle, term_t batch_mode_term)
 * @prologname po_get_batch_mode_/2
 *
 * @description
 * This predicate gets the batch_mode property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_batch_mode(term_t parse_options_handle, term_t batch_mode_term) {
  return po_get_options_boolean_(parse_options_handle, batch_mode_term, parse_options_get_batch_mode);
}


/**
 * @name pl_po_set_panic_mode(term_t parse_options_handle, term_t panic_mode_term)
 * @prologname po_set_panic_mode_/2
 *
 * @description
 * This predicate sets the panic_mode property (from the panic_mode_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_panic_mode(term_t parse_options_handle, term_t panic_mode_term) {
  return po_set_options_boolean_(parse_options_handle, panic_mode_term, parse_options_set_panic_mode);
}


/**
 * @name pl_po_get_panic_mode(term_t parse_options_handle, term_t panic_mode_term)
 * @prologname po_get_panic_mode_/2
 *
 * @description
 * This predicate gets the panic_mode property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_panic_mode(term_t parse_options_handle, term_t panic_mode_term) {
  return po_get_options_boolean_(parse_options_handle, panic_mode_term, parse_options_get_panic_mode);
}


/**
 * @name pl_po_set_allow_null(term_t parse_options_handle, term_t allow_null_term)
 * @prologname po_set_allow_null_/2
 *
 * @description
 * This predicate sets the allow_null property (from the allow_null_term integer term) to the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_set_allow_null(term_t parse_options_handle, term_t allow_null_term) {
  return po_set_options_boolean_(parse_options_handle, allow_null_term, parse_options_set_allow_null);
}


/**
 * @name pl_po_get_allow_null(term_t parse_options_handle, term_t allow_null_term)
 * @prologname po_get_allow_null_/2
 *
 * @description
 * This predicate gets the allow_null property from the parse options object associated with the parse_options_handle handle
**/

foreign_t pl_po_get_allow_null(term_t parse_options_handle, term_t allow_null_term) {
  return po_get_options_boolean_(parse_options_handle, allow_null_term, parse_options_get_allow_null);
}


/**
 * @name pl_get_max_sentence(term_t max_sentence)
 * @prologname get_max_sentence/1
 *
 * @description
 * This predicate only returns an atom containing the value of the MAX_SENTENCE define
**/

foreign_t pl_get_max_sentence(term_t max_sentence) {
  return PL_unify_integer(max_sentence, MAX_SENTENCE);
}


/**
 * @name pl_enable_panic_on_parse_options(term_t parse_options_handle)
 * @prologname enable_panic_on_parse_options/1
 *
 * @description
 * This predicate will activate the panic parsing mode for the specified parse options
**/

foreign_t pl_enable_panic_on_parse_options(term_t parse_options_handle) {
  return po_set_options_boolean_(parse_options_handle, TRUE, parse_options_set_panic_mode);
}


/**
 * @name pl_disable_panic_on_parse_options(term_t parse_options_handle)
 * @prologname disable_panic_on_parse_options/1
 *
 * @description
 * This predicate will deactivate the panic parsing mode for the specified parse options
**/

foreign_t pl_disable_panic_on_parse_options(term_t parse_options_handle) {
  return po_set_options_boolean_(parse_options_handle, FALSE, parse_options_set_panic_mode);
}


/**
 * @name word_to_term(char *word_string, term_t word_term)
 *
 * @description
 * This function parses the word_string C-type string and unifies the word_term term with a compound term gathering the information for Prolog
**/

static int word_to_term(char *word_string, term_t word_term) {

char      *word_name;
char      *word_type;
char      *cs, *cd; /* String manipulation pointers */
functor_t word_name_functor;
term_t    word_type_term = PL_new_term_ref();

 
  word_name=exalloc(strlen(word_string)+1);
  for (cs=word_string, cd=word_name; *cs; cs++, cd++)
    *cd=(isupper(*cs) ? tolower(*cs) : *cs); /* Lower case for the word and copy inside word_string_copy */
  
  *cd='\0'; /* Terminate the copied string */
  word_type=NULL; /* No type by now */
  cs=word_name; /* Start again from the beginning */
  while (*cs!='\0') {
    if (*cs=='.') {
      *cs='\0'; /* Close the word_name string */
      word_type=cs+1; /* This will be the type part */
      break; /* Exit the while loop */
    }
    cs++;
  }

  word_name_functor = PL_new_functor(PL_new_atom(word_name), 1); /* Construct the compound for the word */
  if (word_type!=NULL)
    PL_put_atom_chars(word_type_term, word_type); /* Create an atom containing the word's type */

  exfree(word_name, strlen(word_string)+1); /* Free the memory used by the word_name string */

  PL_put_functor(word_term, word_name_functor);
  if (word_type!=NULL)
    return PL_unify_arg(1, word_term, word_type_term); /* If a type has been found, then bound it with the parameter of <word_term>/1 */
  
  PL_succeed;
}


/**
 * @name create_connector(char *connector_string, term_t connector_term)
 *
 * @description
 * This function parses the connector_string C-type string and unifies the connector_term term with the compound result
**/

static int create_connector(char *connector_string, term_t connector_term) {
  
char     *connector_name;
char     *connector_name_end;
char     *connector_subscript_name;
term_t   connector_name_term = PL_new_term_ref();
term_t   new_connector_subscript_element = PL_new_term_ref();
term_t   constructed_connector_subscript_list = PL_new_term_ref();
char     *cs, *cd; /* String manipulation pointers */

  
  connector_name=exalloc(strlen(connector_string)+1); /* Allocate a temporary working string */
  
  connector_subscript_name=NULL; /* No subscript found so far */
  for (cs=connector_string, cd=connector_name; *cs; cs++, cd++) {
    if (islower(*cs) && (connector_subscript_name==NULL)) {
      connector_subscript_name=cd; /* The subscript starts here */
    }
    *cd=(isupper(*cs) ? tolower(*cs) : *cs); /* Transform in lower case */
  }
  connector_name_end=cd; /* Record the end of the connector_name C string inside connector_name_end (this pointer will point on the '\0' terminating character */
  *connector_name_end='\0'; /* Terminate the C string */
  
  PL_put_nil(constructed_connector_subscript_list);
  
  if (connector_subscript_name!=NULL) { /* There is a subscript */
    for (cs=connector_name_end; cs--!=connector_subscript_name; ) { /* We parse this string from the end to the beginning because of tail-to-head list construction method */
      if (*cs=='*')
	PL_put_variable(new_connector_subscript_element); /* an '*' character in the subscript will be handled as an unbound term inside the subscript Prolog list */
      else
	PL_put_atom_chars(new_connector_subscript_element, cs); /* Create an atom with the subscript inside (cs points toward the last character, followed by a '\0') */
      PL_cons_list(constructed_connector_subscript_list, new_connector_subscript_element, constructed_connector_subscript_list); /* Add this element to the list */

      *cs='\0'; /* Set the new string end to overwrite the character added to the list (last character). cs will now progress toward the beginning of the connector_subscript_name string */
    }
  }

  PL_put_atom_chars(connector_name_term, connector_name); /* Transform string in atom */
    
  exfree(connector_name, strlen(connector_string)+1); /* Caution: after this instruction, connector_name AND connector_subscript_name are invalid string pointers */
 
  PL_put_functor(connector_term, FUNCTOR_hyphen2); /* Create the template for connector-connector_subscript (separated by an hyphen) */
  
  return (PL_unify_arg(1, connector_term, connector_name_term) &&
	  PL_unify_arg(2, connector_term, constructed_connector_subscript_list));
}


/**
 * @name linkage_to_compound(Linkage linkage, term_t links_list)
 *
 * @description
 * This function creates a Prolog compund term gathering all the information that could be got from the linkage object (domains, links, names of words, type of words...)
 * When exiting, all the information is contained inside the Prolog term links_list, that is to say that NO MEMORY is used by temporary objects after returning from this function. It's up to the Prolog engine to take care (detecting obsolescence, and releasing unused memory) of the links_list term and all the nested terms it includes.
 * Here is a summary of what is sent back as a the links_list Prolog term:
 * Sentence: The house is in the middle of my garden
 * A call to linkage_print_links_and_domains(linkage) would return the following C-string:
 *       LEFT-WALL      RW      <-RW->  RW        RIGHT-WALL
 * (m)   LEFT-WALL      Wd      <-Wd->  Wd        house.n
 * (m)   the            D       <-Ds->  Ds        house.n
 * (m)   house.n        Ss      <-Ss->  Ss        is.v
 * (m)   is.v           Pp      <-Pp->  Pp        in
 * (m)   in             J       <-Js->  Js        middle.n
 * (m)   the            D       <-Ds->  Ds        middle.n
 * (m)   middle.n       M       <-Mp->  Mp        of
 * (m)   of             J       <-Js->  Js        garden.n
 * (m)   my             D       <-Ds->  Ds        garden.n
 * And a call to linkage_to_compound(Linkage linkage, term_t links_list) will return the following term, inside links_list:
 * [link( [m], connection(d-[s], my(_G1717),  garden(n)) ),
 *  link( [m], connection(j-[s], of(_G1694),  garden(n)) ),
 *  link( [m], connection(m-[p], middle(n),   of(_G1669)) ),
 *  link( [m], connection(d-[s], the(_G1648), middle(n)) ),
 *  link( [m], connection(j-[s], in(_G1625),  middle(n)) ),
 *  link( [m], connection(p-[p], is(v),       in(_G1600)) ),
 *  link( [m], connection(s-[s], house(n),    is(v)) ),
 *  link( [m], connection(d-[s], the(_G1556), house(n)) ),
 * \
 *  link( [m], connection(w-[d], 'left-wall'(_G1533), house(n)) ),
 *  link( [],  connection(rw-[], 'left-wall'(_G1513), 'right-wall'(_G1511)) )
 * ]
 * Let's have a quick but precise look at what has been created:
 * The links_list is (and its name has been chosen for this purpose!) a Prolog list.
 * Each element corresponds to a grammar link returned by the parser (which is equivalent to a line output by linkage_print_links_and_domains.
 * The elements are composed by one functor (link), having 2 arguments:
 * The first one is a list of domain names (in this example it is [m] for almost all lines, but this list can obviously contain more than one element)
 * The second argument for link/2 is a description of the connection that this link represents. This is symbolised by a connection/3 functor, having 3 parameters.
 * And that's all for link/2.
 * Concerning connection/3, let's give a few additional details about the structure.
 * Its first parameter is a description of the grammar link type. The major type is put first, followed by a - and a list of one letter atoms. Each of those letters in the list is part of the link subscript.
 * Here is one example for the link type: MXs will be coded as mx-[s], and Pg*b will be coded p-[g, _, b]. The unbound variable is the Prolog interpretation of * inside the grammar parser.
 * The second and the third parameters for connection/3 are the words bound by the link. Each word has one parameter which is a letter corresponding to the type of word (or an unbound term if the type has not been precised by the underlying grammar parser layer). Therefore, in the preceeding example, house(n) means the 'house' word, used as a name (n)
**/

static int linkage_to_compound(Linkage linkage, term_t links_list) {
  /* To the left of each link, print the sequence of domains it is in. */
  /* Printing a domain means printing its type                         */
  /* Takes info from pp_link_array and pp and chosen_words.            */
int        link; /* Variable to index the links in this linkage */
int        domain_index; /* Index for domains */
int        links_number;
char       **domain_name; /* Domain name array */
term_t     new_link_element = PL_new_term_ref(); /* This term is used to construct new elements before adding them to the list */
term_t     constructed_links_list = PL_new_term_ref(); /* Term used to store the list while constructing it */
term_t     new_domain_element = PL_new_term_ref();
term_t     constructed_domain_list = PL_new_term_ref();
term_t     connector = PL_new_term_ref();
char       *left_word, *right_word; /* Those are pointers on the strings containing the left and the right word of a link */
term_t     left_word_term = PL_new_term_ref();
term_t     right_word_term = PL_new_term_ref();
term_t     words_lr_link = PL_new_term_ref();
term_t     exception; /* Term to return in case of exception */
Sentence   sent; /* Sentence associated with the linkage passed as parameter to this function */
Dictionary dict; /* Dictionary associated with this linkage */
int        l, r; /* Number of words on the right and on the left of the current link */
char       *label, *llabel, *rlabel; /* Connector names */

  links_number = linkage_get_num_links(linkage);
  sent = linkage_get_sentence(linkage); /* Get the sentence handle index for the linkage (this way to access sent from the linkage is internal to the Language Grammar Parser code, and has nothing to do with this API) */
// For some reason, the following doesn't work. Lionel 20020207. This is thus replaced by the direct access below
//  dict = sentence_get_dictionary(sent);
  dict = sent->dict; /* Get the dictionary for this sentence (this way to access dict is internal to the Language Grammar Parser code, and has nothing to do with this API) */

  PL_put_nil(constructed_links_list); /* Create the tail of the list (which is []) */

  for (link=0; link<links_number; link++) { /* Browse through all the links */
    if (linkage_get_link_lword(linkage, link) == -1) continue;
    domain_name = linkage_get_link_domain_names(linkage, link);
    PL_put_nil(constructed_domain_list);
    for (domain_index=0; domain_index<linkage_get_link_num_domains(linkage, link); ++domain_index) {
      PL_put_atom_chars(new_domain_element, domain_name[domain_index]);
      PL_cons_list(constructed_domain_list, new_domain_element, constructed_domain_list);
    }
    
    l      = linkage_get_link_lword(linkage, link); /* Get the number of words on the left of this link */
    r      = linkage_get_link_rword(linkage, link); /* Get the number of words on the right of this link */
    label  = linkage_get_link_label(linkage, link); /* Get the name of the link (connector and subscript) for this link */
    llabel = linkage_get_link_llabel(linkage, link);
    rlabel = linkage_get_link_rlabel(linkage, link);

    if ((l == 0) && dict->left_wall_defined) {
      left_word=LEFT_WALL_DISPLAY;
    } else if ((l == (linkage_get_num_words(linkage)-1)) && dict->right_wall_defined) {
      left_word=RIGHT_WALL_DISPLAY;	
    } else {
      left_word=linkage_get_word(linkage, l);
    }
    right_word=linkage_get_word(linkage, r);

    if (!(word_to_term(right_word, right_word_term) &&
	  word_to_term(left_word, left_word_term))) {
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 1),
		    PL_CHARS, "cant_create_word_term");
      return PL_raise_exception(exception);
    }
    
    if (!create_connector(label, connector)) {
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 1),
		    PL_CHARS, "cant_create_connector_term");
      return PL_raise_exception(exception);
    }
    
    PL_put_functor(words_lr_link, FUNCTOR_connection3); /* Construct the compound putting the left and right words as parameters of the connector structure */
    if (!(PL_unify_arg(1, words_lr_link, connector) &&
	  PL_unify_arg(2, words_lr_link, left_word_term) &&
	  PL_unify_arg(3, words_lr_link, right_word_term))) {
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 1),
		    PL_CHARS, "cant_create_words_link_term");
      return PL_raise_exception(exception);
    }
    
    PL_put_functor(new_link_element, FUNCTOR_link2);
    if (!(PL_unify_arg(1, new_link_element, constructed_domain_list) &&
	  PL_unify_arg(2, new_link_element, words_lr_link))) { /* We have constructed the link/2 term gathering the link details and the domains*/
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 1),
		    PL_CHARS, "cant_create_link_term");
      return PL_raise_exception(exception);
    }
    PL_cons_list(constructed_links_list, new_link_element, constructed_links_list);
  }

  return PL_unify(links_list, constructed_links_list);
}


/**
 * @name static int add_to_context_list(link_object, context)
 *
 * @description
 * This function adds a context objects to a context list (chained list containing context references)
 * The context_list is taken from the link_object passed as the first argument. Its payload contains a field .context_list pointing to the root of the context list
 * The new context object to add is passed as the second argument (it is a pointer on a pl_get_linkage_context structure). The new context will always be added at the root of the context list
**/

static int add_to_context_list(link_linked_list_object *link_object, pl_get_linkage_context *context) {

term_t             exception;
context_list*      old_root_context_list;

//  //@@ Breakpoint 1 Entering add_to_context_link //Lionel!!!

//  //@! "Breakpoint 2 Linkage set object is at %p", link_object //Lionel!!!
  old_root_context_list=link_object->payload.associated_context_list;
  //  //@! "Breakpoint 3 Root of context list is at %p", old_root_context_list //Lionel!!!
  link_object->payload.associated_context_list = exalloc(sizeof(context_list));
  if (link_object->payload.associated_context_list == NULL) { /* Allocation for the new context_list object failed */
    link_object->payload.associated_context_list = old_root_context_list; /* Revert to previous root element */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
                  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                  PL_CHARS, "linkage_set",
                  PL_CHARS, "not_enough_memory");
    return PL_raise_exception(exception);
  }
  // //@! "Breakpoint 4 New context created as root at %p", link_object->payload.associated_context_list //Lionel!!!
  link_object->payload.associated_context_list->next = old_root_context_list; /* Relink the first item with the rest of the list */
  link_object->payload.associated_context_list->context_associated_to_link=context; /* Make the new context element of the linkage set object point to the context item we are creating in order to redo the current Prolog predicate */
  //  //@! "Breakpoint 5 New context has got a next=%p", link_object->payload.associated_context_list->next //Lionel!!!
  PL_succeed;
}


/**
 * @name static int delete_from_context_link(link_object, context) 
 *
 * @description
 * This function deletes all context objects a context list (chained list containing context references)
 * The context_list is taken from the link_object passed as the first argument. Its payload contains a field .context_list pointing to the root of the context list
 * The context objects that are matched with context, passed as the second argument will be deleted from the context list
**/

static int delete_from_context_link(link_linked_list_object *link_object, pl_get_linkage_context *context) {

context_list*      current_context_in_context_list;
context_list*      previous_context_in_context_list;
context_list*      following_context_in_context_list;
context_list*      root_context_list;

//  //@@ Breakpoint 1 Entering delete_from_context_link //Lionel!!!

//  //@! "Breakpoint 2 Linkage set object is at %p", link_object //Lionel!!!
  previous_context_in_context_list=NULL;
  root_context_list=link_object->payload.associated_context_list;
  //  //@! "Breakpoint 3 Root of context list is at %p", root_context_list //Lionel!!!
  current_context_in_context_list=root_context_list; /* Get the root of the context list for the linkage set */

  while (current_context_in_context_list!=NULL) { /* Parse the whole context list */
    // //@! "Breakpoint 4 In loop current_context_in_context_list=%p", current_context_in_context_list //Lionel!!!
    // //@! "Breakpoint 5 In loop current_context_in_context_list->context_associated_to_link=%p", current_context_in_context_list->context_associated_to_link //Lionel!!!
    if (current_context_in_context_list->context_associated_to_link == context) { /* We found a matching context in the list, we will remove it... */
      //  //@@ Breakpoint 6 Match found. Will be deleted //Lionel!!!
      if (previous_context_in_context_list!=NULL) { /* We are not currently processing the root of the list */
	//	//@@ Breakpoint 7 Match found is not on the root //Lionel!!!
        previous_context_in_context_list->next=current_context_in_context_list->next; /* Shortcut this context in the context list */
	//	//@! "Breakpoint 7.1 %p->next has been set to %p", previous_context_in_context_list, current_context_in_context_list->next //Lionel!!!
      }
      else { /* This is the case when no previous_context_in_context_list has been recorded yet, which means that the element that matched was actually the root of the list */
	//	//@@ Breakpoint 8 Match found is on the root //Lionel!!!
        link_object->payload.associated_context_list=current_context_in_context_list->next;
	//	//@! "Breakpoint 8.1 Root of context list has been set to %p", current_context_in_context_list->next //Lionel!!!
      }
      //      //@@ Breakpoint 9 freeing up memory for the context_list item //Lionel!!!
      following_context_in_context_list=current_context_in_context_list->next; /* We do this before freeing current_context_in_context_list, of course! */
      exfree(current_context_in_context_list, sizeof(context_list)); /* Free the context structure */
      current_context_in_context_list=following_context_in_context_list; /* Move to the next context in the list. We don't move the previous_context_in_context_list because, given that we have deleted current_context_in_context_list, the following element in the list is still following previous_context_in_context_list */
//      //@@ Breakpoint 10 continuing on next item in the list having deleted an item //Lionel!!!
    }
    else {
      //      //@@ Breakpoint 10 continuing on next item in the list w/o deleting //Lionel!!!
      previous_context_in_context_list=current_context_in_context_list;
      current_context_in_context_list=current_context_in_context_list->next; /* Carry on processing on the rest of the list */
    }
  }
  //  //@! "Breakpoint 11 Root of context list is now %p", link_object->payload.associated_context_list //Lionel!!!
  PL_succeed;
}


/**
 * @name pl_get_linkage
 * @prologname get_linkage/2
 *
 * @description
 * This predicate parses a sentence, according to a dictionary and to parse options
 * Note: this predicate manipulates the linkage_set objects, which also involves that it works with its associated sentence and parse options objects
 * While this predicate will be valid (from the PL_FIRST_CALL to PL_CUTTED or a failure to PL_REDO), a context variable will be created to memorise the status of the last call of the predicate. We don't need to make sure that references to the parse options and sentence objects are valid (objects haven't been deleted in the meantime), because the creation of linkage set objects already handle this via the reference counts
**/

foreign_t pl_get_linkage(term_t linkage_set_handle,
			 term_t t_result,
			 foreign_t handle) {

pl_get_linkage_context        *context;

term_t                        exception;

Linkage                       linkage; /* Linkage object containing the linkages found for a sentence */
int                           num_linkages; /* Number of linkages found for a the current linkage set */
unsigned int                  link_handle_index;
link_linked_list_object       *link_object; /* Linkage set object on which we work here */
sent_linked_list_object       *sent_object; /* Linked object corresponding to the linkage set, in the sentence chained-list */
opts_linked_list_object       *opts_object; /* Linked object corresponding to the linkage set, in the parse options chained-list */
//@-

//  //@@ Breakpoint 0 Entering pl_get_linkage //Lionel!!!
  switch (PL_foreign_control(handle)) {
  case PL_FIRST_CALL:
    //  //@@ Breakpoint 1 Entering pl_get_linkage for first call //Lionel!!!
    if (!get_index_from_handle(FUNCTOR_linkageset1, linkage_set_handle, &link_handle_index)) {
      exception = PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		    PL_CHARS, "linkage_set",
		    PL_CHARS, "bad_handle");
      return PL_raise_exception(exception);
    }

    if (!get_object_from_handle_index_in_chained_list_with_exception_handling("linkage_set", NULL,
                                                                              root_link_list, link_handle_index, (generic_linked_list_object **)&link_object)) {
      PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
    }
    else { /* Function executed successfully */
      sent_object =  link_object->payload.associated_sentence_chained_object; /* Get the sentence object from the record inside the linkage set */
      opts_object =  link_object->payload.associated_parse_options_chained_object; /* Get the parse options object from the record inside the linkage set */
      num_linkages = link_object->payload.num_linkages; /* Get the number of linkages that can be found for this sentence */
    }

    
    context=exalloc(sizeof(pl_get_linkage_context)); /* Allocate the context structure */
    if (context == NULL) { /* Check if memory has been successfully allocated */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
		    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		    PL_CHARS, "foreign_context",
		    PL_CHARS, "not_enough_memory");
      return PL_raise_exception(exception);
    }
    context->link_handle_index = link_handle_index;
    context->last_handled_linkage = 0;

    linkage = linkage_create(0, sent_object->payload.sentence, opts_object->payload);
    if (!linkage_to_compound(linkage, t_result)) {
      linkage_delete(linkage);
      exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "linkage_to_compound",
                    PL_CHARS, "failed");
      return PL_raise_exception(exception);
    }
    else {
      linkage_delete(linkage);

      if (!add_to_context_list(link_object, context)) { /* If this fails, there is an exception to raise, so PL_fail will pass the exception to Prolog */
        exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
        PL_fail;
      }
      // //@! "Breakpoint 2 Setting up retry structure with context %p", context //Lionel!!!
      
      PL_retry_address(context); /* Allow redo, and precise the address of the context structure */
    }
    break;

  case PL_REDO:
    //  //@@ Breakpoint 3 Entering pl_get_linkage for redo call //Lionel!!!
    context=PL_foreign_context_address(handle);
    //  //@! "Breakpoint 4 Got retry structure with context %p", context //Lionel!!!
    if (context == NULL) {
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "foreign_context",
                    PL_CHARS, "pointer_lost"); /* Note: this is a bug in either Prolog or the API (more likely ini Prolog, actually, so this will hopefully never happen) */
      return PL_raise_exception(exception);
    }
    link_handle_index = context->link_handle_index; /* This part retrieves the context variables and stores them inside local stack variables */
 
    if (link_handle_index == -1) { /* No linkage set object is referenced by context->link_handle_index... this must come from the fact that the linkage set has been deleted, and our context was updated accordingly */
      exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "linkage_set",
                    PL_CHARS, "reference_erased");
      return PL_raise_exception(exception);
    }

    if (!get_object_from_handle_index_in_chained_list_with_exception_handling("linkage_set", NULL,
                                                                              root_link_list, link_handle_index, (generic_linked_list_object **)&link_object)) {
      PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
    }
    else { /* Function executed successfully */
      sent_object =  link_object->payload.associated_sentence_chained_object; /* Get the sentence object from the record inside the linkage set */
      opts_object =  link_object->payload.associated_parse_options_chained_object; /* Get the parse options object from the record inside the linkage set */
      num_linkages = link_object->payload.num_linkages; /* Get the number of linkages that can be found for this sentence */
    }

    (context->last_handled_linkage)++; /* Update this context variable to contain the number of the linkage currently created */

    if ( context->last_handled_linkage>=num_linkages ) {
      /* num_linkages is the number of linkages, and last_handled_linkage is 0-based, which means that the last linkage will be num_linikages-1. If we arrive there, we have already returned the last linkage, so fail and terminate this predicate */
      // //@@ last linkage already returned. Going to fail //Lionel!!!
      // //@! "going to call delete_from_context_link(%p, %p)", link_object, context  //Lionel!!!
      delete_from_context_link(link_object, context); /* Delete all references to our context from the linkage set object we used to reference, given that we are going to destroy the context item itself */
      // //@! "going to call exfree on %p", context  //Lionel!!!
      exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
      PL_fail;
    }
    
    
    linkage = linkage_create(context->last_handled_linkage, sent_object->payload.sentence, opts_object->payload);
    if (!linkage_to_compound(linkage, t_result)) {
      linkage_delete(linkage);
      delete_from_context_link(link_object, context); /* Delete all references to our context from the linkage set object we used to reference, given that we are going to destroy the context item itself */
      exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
      PL_fail;
    }
    else {
      linkage_delete(linkage);
      PL_retry_address(context); /* Allow redo, and precise the address of the context structure */
    }
    break;
 
  case PL_CUTTED:
    //  //@@ Breakpoint 5 Entering pl_get_linkage for cut //Lionel!!!
    context=PL_foreign_context_address(handle);
    //  //@! "Breakpoint 6 Got cut structure with context at %p", context //Lionel!!!
    if (context == NULL) {
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "foreign_context",
                    PL_CHARS, "pointer_lost"); /* Note: this is a bug in either Prolog or the API (more likely ini Prolog, actually, so this will hopefully never happen) */
      return PL_raise_exception(exception);
    }
    link_handle_index = context->link_handle_index; /* This part retrieves the context variables and stores them inside local stack variables */
 
    if (link_handle_index == -1) { /* No linkage set object is referenced by context->link_handle_index... this must come from the fact that the linkage set has been deleted, and our context was updated accordingly */
      exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
      exception=PL_new_term_ref();
      PL_unify_term(exception,
                    PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
                    PL_CHARS, "linkage_set",
                    PL_CHARS, "reference_erased");
      return PL_raise_exception(exception);
    }

    if (!get_object_from_handle_index_in_chained_list_with_exception_handling("linkage_set", NULL,
                                                                              root_link_list, link_handle_index, (generic_linked_list_object **)&link_object)) {
      PL_fail; /* get_object_from_handle_index_in_chained_list failed and prepared an exception. PL_fail will return to Prolog and raise the pending exception */
    }

    //    //@@ Breakpoint 7 Going to call delete_from_context_link //Lionel!!!
    delete_from_context_link(link_object, context); /* Delete all references to our context from the linkage set object we used to reference, given that we are going to destroy the context item itself */
    //    //@@ Breakpoint 8 Going to call exfree //Lionel!!!
    exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
    PL_succeed;
    break;
  }

  /* If we arrive here, Prolog did not call us with a normal non-deterministic handle. Just fail and exist in a clean way */
  /* Given that we want to repair, we will try our best, but if anything goes wrong, we won't raise an exception but continue silently... */
  if (handle!=NULL) {
    context=PL_foreign_context_address(handle);
    if (context!=NULL) {
      link_handle_index = context->link_handle_index; /* This part retrieves the context variables and stores them inside local stack variables */
      if (link_handle_index != -1) {
        if (get_object_from_handle_index_in_chained_list_with_exception_handling("linkage_set", NULL,
                                                                                 root_link_list, link_handle_index, (generic_linked_list_object **)&link_object)) {
          delete_from_context_link(link_object, context); /* Delete all references to our context from the linkage set object we used to reference, given that we are going to destroy the context item itself */
        }
      }
      exfree(context, sizeof(pl_get_linkage_context)); /* Free the context structure */
    }
  }
  PL_fail;
}


/**
 * @name main(int argc, char **argv)
 *
 * @description
 * This main function is there to give a WinMain@16 label to CygWin. It is not used during the DLL normal use
**/

int main(int argc, char **argv) {
  return 0;
}


#if (defined(__MINGW32__) || defined(__MINGW64__) || defined(WIN32) || defined(_WIN32))
/**
 * @name lgp_init_dll(HANDLE h, DWORD reason, void *foo)
 *
 * @description
 * This function is the entry point of the DLL. It is started when the DLL is loaded in memory and does... nothing!
**/

int WINAPI lgp_init_dll(HANDLE h, DWORD reason, void *foo) {
  return 1;
}
#endif


/**
 * @name install_lgp()
 *
 * @description
 * This is the initialisation routine for this foreign language library
 * This function has to be called by load_foreign_library/2 from the Prolog side in order to register the predicates implemented here as C code
**/

install_t install_lgp() {

term_t exception; /* Handle for a possible exception */

  PL_register_foreign("create_dictionary", 5, pl_create_dictionary, 0);
  PL_register_foreign("delete_dictionary", 1, pl_delete_dictionary, 0);
  PL_register_foreign("delete_all_dictionaries", 0, pl_delete_all_dictionaries, 0);
  PL_register_foreign("get_nb_dictionaries", 1, pl_get_nb_dictionaries, 0);
  PL_register_foreign("get_handles_nb_references_dictionaries", 2, pl_get_handles_nb_references_dictionaries, 0);

  PL_register_foreign("create_parse_options_", 1, pl_create_parse_options, 0);
  PL_register_foreign("delete_parse_options", 1, pl_delete_parse_options, 0);
  PL_register_foreign("delete_all_parse_options", 0, pl_delete_all_parse_options, 0);
  PL_register_foreign("get_nb_parse_options", 1, pl_get_nb_parse_options, 0);
  PL_register_foreign("get_handles_nb_references_parse_options", 2, pl_get_handles_nb_references_parse_options, 0);

  PL_register_foreign("create_linkage_set", 3, pl_create_linkage_set, 0);
  PL_register_foreign("delete_linkage_set", 1, pl_delete_linkage_set, 0);
  PL_register_foreign("delete_all_linkage_sets", 0, pl_delete_all_linkage_sets, 0);
  PL_register_foreign("get_nb_linkage_sets", 1, pl_get_nb_linkage_sets, 0);
  PL_register_foreign("get_handles_nb_references_linkage_sets", 2, pl_get_handles_nb_references_linkage_sets, 0);

  PL_register_foreign("get_num_linkages", 2, pl_get_num_linkages, 0);
  PL_register_foreign("get_parameters_for_linkage_set", 2, pl_get_parameters_for_linkage_set, 0);

  PL_register_foreign("create_sentence", 3, pl_create_sentence, 0);
  PL_register_foreign("delete_sentence", 1, pl_delete_sentence, 0);
  PL_register_foreign("delete_all_sentences", 0, pl_delete_all_sentences, 0);
  PL_register_foreign("get_nb_sentences", 1, pl_get_nb_sentences, 0);
  PL_register_foreign("get_handles_nb_references_sentences", 2, pl_get_handles_nb_references_sentences, 0);

  PL_register_foreign("po_set_linkage_limit_", 2, pl_po_set_linkage_limit, 0);
  PL_register_foreign("po_get_linkage_limit_", 2, pl_po_get_linkage_limit, 0);
  PL_register_foreign("po_set_disjunct_cost_", 2, pl_po_set_disjunct_cost, 0);
  PL_register_foreign("po_get_disjunct_cost_", 2, pl_po_get_disjunct_cost, 0);
  PL_register_foreign("po_set_min_null_count_", 2, pl_po_set_min_null_count, 0);
  PL_register_foreign("po_get_min_null_count_", 2, pl_po_get_min_null_count, 0);
  PL_register_foreign("po_set_max_null_count_", 2, pl_po_set_max_null_count, 0);
  PL_register_foreign("po_get_max_null_count_", 2, pl_po_get_max_null_count, 0);
  PL_register_foreign("po_set_null_block_", 2, pl_po_set_null_block, 0);
  PL_register_foreign("po_get_null_block_", 2, pl_po_get_null_block, 0);
  PL_register_foreign("po_set_islands_ok_", 2, pl_po_set_islands_ok, 0);
  PL_register_foreign("po_get_islands_ok_", 2, pl_po_get_islands_ok, 0);
  PL_register_foreign("po_set_short_length_", 2, pl_po_set_short_length, 0);
  PL_register_foreign("po_get_short_length_", 2, pl_po_get_short_length, 0);
  PL_register_foreign("po_set_all_short_connectors_", 2, pl_po_set_all_short_connectors, 0);
  PL_register_foreign("po_get_all_short_connectors_", 2, pl_po_get_all_short_connectors, 0);
  PL_register_foreign("po_set_max_parse_time_", 2, pl_po_set_max_parse_time, 0);
  PL_register_foreign("po_get_max_parse_time_", 2, pl_po_get_max_parse_time, 0);
  PL_register_foreign("po_set_max_memory_", 2, pl_po_set_max_memory, 0);
  PL_register_foreign("po_get_max_memory_", 2, pl_po_get_max_memory, 0);
  PL_register_foreign("po_set_max_sentence_length_", 2, pl_po_set_max_sentence_length, 0);
  PL_register_foreign("po_get_max_sentence_length_", 2, pl_po_get_max_sentence_length, 0);
  PL_register_foreign("po_set_batch_mode_", 2, pl_po_set_batch_mode, 0);
  PL_register_foreign("po_get_batch_mode_", 2, pl_po_get_batch_mode, 0);
  PL_register_foreign("po_set_panic_mode_", 2, pl_po_set_panic_mode, 0);
  PL_register_foreign("po_get_panic_mode_", 2, pl_po_get_panic_mode, 0);
  PL_register_foreign("po_set_allow_null_", 2, pl_po_set_allow_null, 0);
  PL_register_foreign("po_get_allow_null_", 2, pl_po_get_allow_null, 0);

  PL_register_foreign("get_max_sentence", 1, pl_get_max_sentence, 0);
  PL_register_foreign("enable_panic_on_parse_options", 1, pl_enable_panic_on_parse_options, 0);
  PL_register_foreign("disable_panic_on_parse_options", 1, pl_disable_panic_on_parse_options, 0);

  PL_register_foreign("get_linkage", 2, pl_get_linkage, PL_FA_NONDETERMINISTIC);

  FUNCTOR_dictionary1 = PL_new_functor(PL_new_atom("$dictionary"), 1); /* Create a '$dictionary'/1 functor for dictionary table handling */
  FUNCTOR_options1 = PL_new_functor(PL_new_atom("$options"), 1); /* Create a '$options'/1 functor for parse options table handling */
  FUNCTOR_linkageset1 = PL_new_functor(PL_new_atom("$linkageset"), 1); /* Create a '$linkage'/1 functor for parse options table handling */
  FUNCTOR_sentence1 = PL_new_functor(PL_new_atom("$sentence"), 1); /* Create a '$sentence'/1 functor for parse options table handling */

  FUNCTOR_list2 = PL_new_functor(PL_new_atom("."), 2); /* Create the list functor (./2) */
  FUNCTOR_equals2 = PL_new_functor(PL_new_atom("="), 2); /* Create the =/2 functor */
  FUNCTOR_link2 = PL_new_functor(PL_new_atom("link"), 2); /* Create the link/2 functor */
  FUNCTOR_hyphen2 = PL_new_functor(PL_new_atom("-"), 2); /* Create the -/2 functor */
  FUNCTOR_connection3 = PL_new_functor(PL_new_atom("connection"), 3); /* Create the connection/3 functor */

/* We test that root_dict_list is NULL here (it has been initialised with this value in its declaration above) */
  if (root_dict_list != NULL) {
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "dictionary",
		  PL_CHARS, "list_corrupted");
    PL_raise_exception(exception);
    return;
  }

  root_dict_list=malloc(sizeof(generic_linked_list_object)); /* Allocate the root of dictionaries with a generic object type */
  if (root_dict_list == NULL) { /* Allocation failed */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "dictionary",
		  PL_CHARS, "not_enough_memory");
    PL_raise_exception(exception);
    return;
  }
  root_dict_list->next=NULL; /* No first element attached to the root: the list is empty */
  root_dict_list->nb_jumped_index=0; /* No jumped free space */
  root_dict_list->count_references=((unsigned int)-1)>>1; /* Make sure that the root will never be deleted */

  root_opts_list=malloc(sizeof(generic_linked_list_object)); /* Allocate the root of parse options with a generic object type */
  if (root_opts_list == NULL) { /* Allocation failed */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "parse_options",
		  PL_CHARS, "not_enough_memory");
    PL_raise_exception(exception);
    return;
  }
  root_opts_list->next=NULL; /* No first element attached to the root: the list is empty */
  root_opts_list->nb_jumped_index=0; /* No jumped free space */
  root_opts_list->count_references=((unsigned int)-1)>>1; /* Make sure that the root will never be deleted */

  root_link_list=malloc(sizeof(generic_linked_list_object)); /* Allocate the root of linkage sets with a generic object type */
  if (root_link_list == NULL) { /* Allocation failed */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "linkage_set",
		  PL_CHARS, "not_enough_memory");
    PL_raise_exception(exception);
    return;
  }
  root_link_list->next=NULL; /* No first element attached to the root: the list is empty */
  root_link_list->nb_jumped_index=0; /* No jumped free space */
  root_link_list->count_references=((unsigned int)-1)>>1; /* Make sure that the root will never be deleted */

  root_sent_list=malloc(sizeof(generic_linked_list_object)); /* Allocate the root of sentences with a generic object type */
  if (root_sent_list == NULL) { /* Allocation failed */
    exception=PL_new_term_ref();
    PL_unify_term(exception,
		  PL_FUNCTOR, PL_new_functor(PL_new_atom("lgp_api_error"), 2),
		  PL_CHARS, "sentence",
		  PL_CHARS, "not_enough_memory");
    PL_raise_exception(exception);
    return;
  }
  root_sent_list->next=NULL; /* No first element attached to the root: the list is empty */
  root_sent_list->nb_jumped_index=0; /* No jumped free space */
  root_sent_list->count_references=((unsigned int)-1)>>1; /* Make sure that the root will never be deleted */
}


/**
 * @name uninstall_lgp()
 *
 * @description
 * This is the uninstallation routine for this foreign language library
 * This function is be called by unload_foreign_library/2 from the Prolog side in order to free the memory used by the DLL
**/

install_t uninstall_lgp() {
  pl_delete_all_linkage_sets(); /* These functions have to be called in this precise order to avoid signal 11 exceptions (segmentation fault) */
  pl_delete_all_sentences(); /* The linkage set uses the sentence object and must therefore be deleted before */
  pl_delete_all_parse_options();
  pl_delete_all_dictionaries(); /* The sentence object uses the dictionary one and must therefore be deleted before */
}
