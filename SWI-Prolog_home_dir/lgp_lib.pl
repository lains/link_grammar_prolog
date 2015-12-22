/**
 * @modulename lgp_lib.pl
 *
 * Exported predicates:
 * @export
 *
 * create_dictionary/5 : this predicate loads a dictionary from the disk and sets up the memory structures accordingly
 * delete_dictionary/1 : this predicate deletes a dictionary from the memory
 * delete_all_dictionaries/0 : delete all the recorded dictionaries from the memory
 * get_nb_dictionaries/1 : get the number of dictionaries currently in the memory
 * get_handles_dictionaries/1 : get a list containing all the exiting handles of allocated dictionaries
 * get_handles_nb_references_dictionaries/2 : get two lists associating the exiting handles of allocated dictionaries to the count other object references
 * create_parse_options/2 : this predicate creates a parse options structure
 * delete_parse_options/1 : this predicate deletes a parse options from the memory
 * delete_all_parse_options/0 : delete all the recorded parse options from the memory
 * set_parse_options/2 : set the options for a preexisting parse options structure
 * get_parse_options/2 : get the options for a preexisting parse options structure
 * get_nb_parse_options/2 : get the number of parse options currently in the memory
 * get_handles_parse_options/1 : get a list containing all the exiting handles of allocated parse option objects
 * get_handles_nb_references_parse_options/2 : get two lists associating the exiting handles of allocated parse options to the count other object references
 * create_linkage_set/3 : this predicate creates a linkage set, gathering a sentence with its parse options
 * delete_linkage_set/1 : this predicate deletes a linkage set from the memory
 * delete_all_linkage_sets/0 : delete all the recorded linkage sets from the memory
 * get_nb_linkage_sets/1 : get the number of linkage sets currently in the memory
 * get_handles_linkage_sets/1 : get a list containing all the existing handles of allocated linkage set objects
 * get_handles_nb_references_linkage_sets/2 : get two lists associating the exiting handles of allocated linkage sets to the count other object references
 * get_num_linkages/2 : get the number of linkages available from a linkage set object
 * get_parameters_for_linkage_set/2 : return a list containing all the handles and values associated to a linkage set object
 * get_full_info_linkage_sets/1 : give the complete list of parameters for all the existing linkage sets
 * create_sentence/3 : this predicate creates a sentence and tokenises it accordingly to a dictionary
 * delete_sentence/1 : this predicate deletes a sentence object from the memory
 * delete_all_sentences/0 : delete all the recorded sentences from the memory
 * get_nb_sentences/1 : get the number of sentence objects currently in the memory
 * get_handles_sentences/1 : get a list containing all the existing handles of allocated sentence objects
 * get_handles_nb_references_sentences/2 : get two lists associating the exiting handles of allocated sentences to the count other object references
 * get_max_sentence/1 : this predicate retrieves the value of MAX_SENTENCE inside the DLL
 * enable_panic_on_parse_options/1 : this predicate activates panic mode on a parse options structure
 * disable_panic_on_parse_options/1 : this predicate deactivates panic mode on a parse options structure
 * get_linkage/2 : this is the foreign predicate making the link with the Link Grammar Parser's API
**/

:- module(lgp_lib,
	  [
	   create_dictionary/5,
	   delete_dictionary/1,
	   delete_all_dictionaries/0,
	   get_nb_dictionaries/1,
           get_handles_dictionaries/1,
	   get_handles_nb_references_dictionaries/2,
	   create_parse_options/2,
	   delete_parse_options/1,
	   delete_all_parse_options/0,
	   set_parse_options/2,
	   get_parse_options/2,
	   get_nb_parse_options/1,
           get_handles_parse_options/1,
           get_handles_nb_references_parse_options/2,
	   create_linkage_set/3,
	   delete_linkage_set/1,
	   delete_all_linkage_sets/0,
	   get_nb_linkage_sets/1,
           get_handles_linkage_sets/1,
	   get_handles_nb_references_linkage_sets/2,
	   get_num_linkages/2,
           get_parameters_for_linkage_set/2,
           get_full_info_linkage_sets/1,
	   create_sentence/3,
	   delete_sentence/1,
	   delete_all_sentences/0,
	   get_nb_sentences/1,
           get_handles_sentences/1,
	   get_handles_nb_references_sentences/2,
	   get_max_sentence/1,
	   enable_panic_on_parse_options/1,
	   disable_panic_on_parse_options/1,
	   get_linkage/2
	  ]).

:- use_module(library(shlib)).

:- initialization
   load_foreign_library(lgp, install).

/**
 * @name get_handles_dictionaries/1
 * @mode get_handles_dictionaries(-)
 *
 * @usage
 * get_handles_dictionaries(Handles_list).
 *
 * @description
 * This predicate returns a list of all the currently valid dictionary handles
**/

get_handles_dictionaries(Handles_list):-
	get_handles_nb_references_dictionaries(Handles_list, _).

/**
 * @name create_parse_options/2
 * @mode create_parse_options(+, -)
 *
 * @usage
 * create_parse_options(Option_list, Parse_options_handle).
 *
 * @description
 * This predicate creates a new parse options object (using the Option_list or the default value if a property is not specified) and unifies Parse_options_handle with a handle on this new object
**/

create_parse_options(Option_list, Parse_options_handle):-
	create_parse_options_(Parse_options_handle),
	set_parse_options(Parse_options_handle, Option_list).

/**
 * @name set_parse_options/2
 * @mode set_parse_options(+, +)
 *
 * @usage
 * set_parse_options(Parse_options_handle, Option_list).
 *
 * @description
 * This predciate sets the options in the Parse_options_handle object, accordingly to the Option_list
**/

set_parse_options(Parse_options_handle, Option_list):-
	(   memberchk(linkage_limit=Linkage_limit, Option_list),
	    nonvar(Linkage_limit),
	    integer(Linkage_limit)
	->  po_set_linkage_limit_(Parse_options_handle, Linkage_limit)
	;   true
	),
	(   memberchk(disjunct_cost=Disjunct_cost, Option_list),
	    nonvar(Disjunct_cost),
	    integer(Disjunct_cost)
	->  po_set_disjunct_cost_(Parse_options_handle, Disjunct_cost)
	;   true
	),
	(   memberchk(min_null_count=Min_null_count, Option_list),
	    nonvar(Min_null_count),
	    integer(Min_null_count)
	->  po_set_min_null_count_(Parse_options_handle, Min_null_count)
	;   true
	),
	(   memberchk(max_null_count=Max_null_count, Option_list),
	    nonvar(Max_null_count),
	    integer(Max_null_count)
	->  po_set_max_null_count_(Parse_options_handle, Max_null_count)
	;   true
	),
	(   memberchk(null_block=Null_block, Option_list),
	    nonvar(Null_block),
	    integer(Null_block)
	->  po_set_null_block_(Parse_options_handle, Null_block)
	;   true
	),
	(   memberchk(islands_ok=Islands_ok, Option_list),
	    nonvar(Islands_ok)
	->  (   Islands_ok=true
	    ->	po_set_islands_ok_(Parse_options_handle, true)
	    ;	po_set_islands_ok_(Parse_options_handle, false)
	    )
	;   true
	),
	(   memberchk(short_length=Short_length, Option_list),
	    nonvar(Short_length),
	    integer(Short_length)
	->  po_set_short_length_(Parse_options_handle, Short_length)
	;   true
	),
	(   memberchk(all_short_connectors=All_short_connectors, Option_list),
	    nonvar(All_short_connectors)
	->  (   All_short_connectors=true
	    ->	po_set_all_short_connectors_(Parse_options_handle, true)
	    ;	po_set_all_short_connectors_(Parse_options_handle, false)
	    )
	;   true
	),
	(   memberchk(max_parse_time=Max_parse_time, Option_list),
	    nonvar(Max_parse_time),
	    integer(Max_parse_time)
	->  po_set_max_parse_time_(Parse_options_handle, Max_parse_time)
	;   true
	),
	(   memberchk(max_memory=Max_memory, Option_list),
	    nonvar(Max_memory),
	    integer(Max_memory)
	->  po_set_max_memory_(Parse_options_handle, Max_memory)
	;   true
	),
	(   memberchk(max_sentence_length=Max_sentence_length, Option_list),
	    nonvar(Max_sentence_length),
	    integer(Max_sentence_length)
	->  po_set_max_sentence_length_(Parse_options_handle, Max_sentence_length)
	;   true
	),
	(   memberchk(batch_mode=Batch_mode, Option_list),
	    nonvar(Batch_mode)
	->  (   Batch_mode=true
	    ->	po_set_batch_mode_(Parse_options_handle, true)
	    ;	po_set_batch_mode_(Parse_options_handle, false)
	    )
	;   true
	),
	(   memberchk(panic_mode=Panic_mode, Option_list),
	    nonvar(Panic_mode)
	->  (   Panic_mode=true
	    ->	po_set_panic_mode_(Parse_options_handle, true)
	    ;	po_set_panic_mode_(Parse_options_handle, false)
	    )
	;   true
	),
	(   memberchk(allow_null=Allow_null, Option_list),
	    nonvar(Allow_null)
	->  (   Allow_null=true
	    ->	po_set_allow_null_(Parse_options_handle, true)
	    ;	po_set_allow_null_(Parse_options_handle, false)
	    )
	;   true
	).

/**
 * @name get_parse_options/2
 * @mode get_parse_options(+, -)
 *
 * @usage
 * set_parse_options(Parse_options_handle, Option_list).
 *
 * @description
 * This predciate gets the options in the Parse_options_handle object and unifies them with Option_list
**/

get_parse_options(Parse_options_handle, Option_list):-
	!,
	(   po_get_linkage_limit_(Parse_options_handle, Linkage_limit)
	->  Linkage_limit_property=(linkage_limit=Linkage_limit)
	;   Linkage_limit_property=pl_skip_element
	),
	(   po_get_disjunct_cost_(Parse_options_handle, Disjunct_cost)
	->  Disjunct_cost_property=(disjunct_cost=Disjunct_cost)
	;   Disjunct_cost_property=pl_skip_element
	),
	(   po_get_min_null_count_(Parse_options_handle, Min_null_count)
	->  Min_null_count_property=(min_null_count=Min_null_count)
	;   Min_null_count_property=pl_skip_element
	),
	(   po_get_max_null_count_(Parse_options_handle, Max_null_count)
	->  Max_null_count_property=(max_null_count=Max_null_count)
	;   Max_null_count_property=pl_skip_element
	),
	(   po_get_null_block_(Parse_options_handle, Null_block)
	->  Null_block_property=(null_block=Null_block)
	;   Null_block_property=pl_skip_element
	),
	(   po_get_islands_ok_(Parse_options_handle, Islands_ok)
	->  Islands_ok_property=(islands_ok=Islands_ok)
	;   Islands_ok_property=pl_skip_element
	),
	(   po_get_short_length_(Parse_options_handle, Short_length)
	->  Short_length_property=(short_length=Short_length)
	;   Short_length_property=pl_skip_element
	),
	(   po_get_all_short_connectors_(Parse_options_handle, All_short_connectors)
	->  All_short_connectors_property=(all_short_connectors=All_short_connectors)
	;   All_short_connectors_property=pl_skip_element
	),
	(   po_get_max_parse_time_(Parse_options_handle, Max_parse_time)
	->  Max_parse_time_property=(max_parse_time=Max_parse_time)
	;   Max_parse_time_property=pl_skip_element
	),
	(   po_get_max_memory_(Parse_options_handle, Max_memory)
	->  Max_memory_property=(max_memory=Max_memory)
	;   Max_memory_property=pl_skip_element
	),
	(   po_get_max_sentence_length_(Parse_options_handle, Max_sentence_length)
	->  Max_sentence_length_property=(max_sentence_length=Max_sentence_length)
	;   Max_sentence_length_property=pl_skip_element
	),
	(   po_get_batch_mode_(Parse_options_handle, Batch_mode)
	->  Batch_mode_property=(batch_mode=Batch_mode)	
	;   Batch_mode_property=pl_skip_element
	),
	(   po_get_panic_mode_(Parse_options_handle, Panic_mode)
	->  Panic_mode_property=(panic_mode=Panic_mode)
	;   Panic_mode_property=pl_skip_element
	),
	(   po_get_allow_null_(Parse_options_handle, Allow_null)
	->  Allow_null_property=(allow_null=Allow_null)
	;   Allow_null_property=pl_skip_element
	),
	Option_list_tmp=[Linkage_limit_property, Disjunct_cost_property, Min_null_count_property,
			 Max_null_count_property, Null_block_property, Islands_ok_property,
			 Short_length_property, All_short_connectors_property, Max_parse_time_property,
			 Max_memory_property, Max_sentence_length_property, Batch_mode_property,
			 Panic_mode_property, Allow_null_property],
	tools:clean_nested_list(Option_list_tmp, Option_list).

/**
 * @name get_handles_parse_options/1
 * @mode get_handles_parse_options(-)
 *
 * @usage
 * get_handles_parse_options(Handles_list).
 *
 * @description
 * This predicate returns a list of all the currently valid parse options handles
**/

get_handles_parse_options(Handles_list):-
	get_handles_nb_references_parse_options(Handles_list, _).

/**
 * @name get_full_info_linkage_sets/1, get_full_info_linkage_sets_/2
 * @mode get_full_info_linkage_sets(-), get_full_info_linkage_sets_(+, -)
 *
 * @usage
 * get_full_info_linkage_sets(Information_list).
 *
 * @description
 * This predicate will return in Information_list, a list containing the details of all existing linkage set objects
**/

get_full_info_linkage_sets(Information_list):-
	get_handles_linkage_sets(Handle_list),
	get_full_info_linkage_sets_(Handle_list, Information_list).
get_full_info_linkage_sets_([Head_handle|Tail_Handle_list], [Head_information|Tail_information_list]):-
	get_parameters_for_linkage_set(Head_handle, Information_for_handle),
	Head_information=[handle=Head_handle|Information_for_handle],
	get_full_info_linkage_sets_(Tail_Handle_list, Tail_information_list).
get_full_info_linkage_sets_([], []).

/**
 * @name get_handles_linkage_sets/1
 * @mode get_handles_linkage_sets(-)
 *
 * @usage
 * get_handles_linkage_sets(Handles_list).
 *
 * @description
 * This predicate returns a list of all the currently valid linkage sets handles
**/

get_handles_linkage_sets(Handles_list):-
	get_handles_nb_references_linkage_sets(Handles_list, _).

/**
 * @name get_handles_sentences/1
 * @mode get_handles_sentences(-)
 *
 * @usage
 * get_handles_sentences(Handles_list).
 *
 * @description
 * This predicate returns a list of all the currently valid sentence handles
**/

get_handles_sentences(Handles_list):-
	get_handles_nb_references_sentences(Handles_list, _).
